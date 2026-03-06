#include "UdpSocket_Win32.hpp"
//#include <iostream>


namespace coco {

UdpSocket_Win32::UdpSocket_Win32(Loop_Win32 &loop)
    : UdpSocket(State::DISABLED)
    , loop_(loop)
{
    // initialize winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
}

UdpSocket_Win32::~UdpSocket_Win32() {
    closesocket(socket_);
}

bool UdpSocket_Win32::open(uint16_t protocolId, int localPort) {
    if (socket_ != INVALID_SOCKET)
        return false;

    // create socket
    SOCKET socket = WSASocket(protocolId, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        setSystemError(error);
        return false;
    }

    int reuse = 1;
    sockaddr_in6 ep = {.sin6_family = protocolId, .sin6_port = htons(localPort)};

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
        goto error;

    // bind to local port
    if (bind(socket, (struct sockaddr*)&ep, sizeof(ep)) < 0)
        goto error;

    // add socket to completion port of event loop
    if (CreateIoCompletionPort(
        (HANDLE)socket,
        loop_.port,
        ULONG_PTR(static_cast<Loop_Win32::CompletionHandler *>(this)),
        0) == nullptr)
    {
        goto error;
    }
    socket_ = socket;
    setSuccess();

    // set state
    state_ = State::READY;

    // enable buffers
    for (auto &buffer : buffers_) {
        buffer.setSuccess(0);
        buffer.setReady();
    }

    // resume all coroutines waiting for state change
    notify(Events::ENTER_OPENING | Events::ENTER_READY);

    return true;
error:
    int error = WSAGetLastError();
    setSystemError(error);
    closesocket(socket);
    return false;
}

bool UdpSocket_Win32::join(ip::v6::Address const &multicastGroup) {
    // join multicast group
    struct ipv6_mreq group;
    std::copy(multicastGroup.u8, multicastGroup.u8 + 16, group.ipv6mr_multiaddr.s6_addr);
    group.ipv6mr_interface = 0;
    int r = setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&group, sizeof(group));
    if (r < 0) {
        int error = WSAGetLastError();
        setSystemError(error);
        return false;
    }
    setSuccess();
    return true;
}

int UdpSocket_Win32::getBufferCount() {
    return buffers_.count();
}

UdpSocket_Win32::Buffer &UdpSocket_Win32::getBuffer(int index) {
    return buffers_.get(index);
}

void UdpSocket_Win32::close() {
    if (socket_ == INVALID_SOCKET)
        return;

    // close socket
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    setSuccess();

    // set state
    state_ = State::DISABLED;

    // disable buffers
    for (auto &buffer : buffers_) {
        buffer.setDisabled();
    }

    // resume all coroutines waiting for state change
    notify(Events::ENTER_CLOSING | Events::ENTER_DISABLED);
}

void UdpSocket_Win32::handle(OVERLAPPED *overlapped) {
    // search the buffer that caused the event
    for (auto &buffer : buffers_) {
        if (overlapped == &buffer.overlapped_) {
            buffer.handle(overlapped);
            break;
        }
    }
}


// UdpSocket_Win32::Buffer

UdpSocket_Win32::Buffer::Buffer(UdpSocket_Win32 &device, int size)
    : coco::Buffer(&endpoint_, sizeof(endpoint_), new uint8_t[size], size, device.state_)
    , device_(device)
{
    device.buffers_.add(*this);
}

UdpSocket_Win32::Buffer::~Buffer() {
    delete [] data_;
}

bool UdpSocket_Win32::Buffer::start() {
    if (state_ != State::READY || (op_ & Op::READ_WRITE) == 0 || size_ == 0) {
        assert(state_ != State::BUSY);
        setSuccess();
        return false;
    }

    // store read/write flags for use in transfer(), handle() and cancel()
    steps_ = uint8_t(op_ & Op::READ_WRITE);

    // start transfer
    if (!transfer())
        return false;

    // set state
    setBusy();
    return true;
}

bool UdpSocket_Win32::Buffer::cancel() {
    if (state_ != State::BUSY)
        return false;

    if (steps_ != 0) {
        auto result = CancelIoEx((HANDLE)device_.socket_, &overlapped_);
        if (!result) {
            // error
            auto error = WSAGetLastError();
            setSystemError(error);
            //debug::out << "cancel error " << dec(e) << '\n';
            return false;
        }
        steps_ = 0;
    }
    return true;
}

bool UdpSocket_Win32::Buffer::transfer() {
    // initialize overlapped
    memset(&overlapped_, 0, sizeof(OVERLAPPED));

    // get header
    CHAR *data = (CHAR *)data_;

    int result;
    if ((Op(steps_) & Op::WRITE) == 0) {
        // receive
        WSABUF buffer{capacity_, data};
        DWORD flags = 0;
        endpointSize_ = sizeof(endpoint_);
        result = WSARecvFrom(device_.socket_, &buffer, 1, nullptr, &flags, &endpoint_.generic, &endpointSize_, &overlapped_, nullptr);
    } else {
        // send
        WSABUF buffer{size_, data};
        result = WSASendTo(device_.socket_, &buffer, 1, nullptr, 0, &endpoint_.generic, sizeof(endpoint_), &overlapped_, nullptr);
    }

    if (result != 0) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            // error
            // nobody listens on the other end: WSAECONNRESET = 10054
            setSystemError(error);
            setReady();
            return false;
        }
    }
    return true;
}

void UdpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
    DWORD transferred;
    DWORD flags;
    auto result = WSAGetOverlappedResult(device_.socket_, overlapped, &transferred, false, &flags);
    if (result) {
        // success, check for read after write
        if (Op(steps_) == Op::READ_WRITE) {
            steps_ = int(Op::READ);
            transfer();
            return;
        }
        setSuccess(transferred);
    } else {
        // error
        // canceled: ERROR_OPERATION_ABORTED
        auto error = WSAGetLastError();
        setSystemError(error);
    }

    // remove from list of active transfers
    //remove2();

    // transfer finished
    setReady();
}

} // namespace coco
