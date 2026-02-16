#include "UdpSocket_Win32.hpp"
#include <iostream>


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
        //int e = WSAGetLastError();
        return false;
    }

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        //int e = WSAGetLastError();
        closesocket(socket);
        return false;
    }

    // bind to local port
    sockaddr_in6 ep = {.sin6_family = protocolId, .sin6_port = htons(localPort)};
    if (bind(socket, (struct sockaddr*)&ep, sizeof(ep)) < 0) {
        //int e = WSAGetLastError();
        closesocket(socket);
        return false;
    }

    // add socket to completion port of event loop
    Loop_Win32::CompletionHandler *handler = this;
    if (CreateIoCompletionPort(
        (HANDLE)socket,
        loop_.port,
        ULONG_PTR(handler),
        0) == nullptr)
    {
        //int e = WSAGetLastError();
        closesocket(socket);
        return false;
    }
    socket_ = socket;

    // set state
    st.set(State::READY);

    // enable buffers
    for (auto &buffer : buffers_) {
        buffer.setReady(0);
    }

    // resume all coroutines waiting for state change
    st.notify(Events::ENTER_OPENING | Events::ENTER_READY);

    return true;
}

bool UdpSocket_Win32::join(ip::v6::Address const &multicastGroup) {
    // join multicast group
    struct ipv6_mreq group;
    std::copy(multicastGroup.u8, multicastGroup.u8 + 16, group.ipv6mr_multiaddr.s6_addr);
    group.ipv6mr_interface = 0;
    int r = setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&group, sizeof(group));
    if (r < 0) {
        int e = WSAGetLastError();
        return false;
    }
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

    // set state
    st.set(State::DISABLED);

    // disable buffers
    for (auto &buffer : buffers_) {
        buffer.setDisabled();
    }

    // resume all coroutines waiting for state change
    st.notify(Events::ENTER_CLOSING | Events::ENTER_DISABLED);
}

void UdpSocket_Win32::handle(OVERLAPPED *overlapped) {
    for (auto &buffer : transfers_) {
        if (overlapped == &buffer.overlapped_) {
            buffer.handle(overlapped);
            break;
        }
    }
}


// UdpSocket_Win32::Buffer

UdpSocket_Win32::Buffer::Buffer(UdpSocket_Win32 &device, int size)
    : coco::Buffer(&endpoint_, sizeof(endpoint_), 0, new uint8_t[size], size, device.st.state)
    , device_(device)
{
    device.buffers_.add(*this);
}

UdpSocket_Win32::Buffer::~Buffer() {
    delete [] data_;
}

bool UdpSocket_Win32::Buffer::start(Op op) {
    if (st.state != State::READY || (op & Op::READ_WRITE) == 0 || size_ == 0) {
        assert(st.state != State::BUSY);
        return false;
    }

    // check if READ or WRITE flag is set
    assert((op & Op::READ_WRITE) != 0);
    op_ = op;

    // add to list of pending transfers
    device_.transfers_.add(*this);

    // start if device is ready
    if (device_.st.state == Device::State::READY)
        start();

    // set state
    setBusy();

    return true;
}

bool UdpSocket_Win32::Buffer::cancel() {
    if (st.state != State::BUSY)
        return false;

    auto result = CancelIoEx((HANDLE)device_.socket_, &overlapped_);
    if (!result) {
        auto e = WSAGetLastError();
        std::cerr << "cancel error " << e << std::endl;
    }

    return true;
}

void UdpSocket_Win32::Buffer::start() {
    // initialize overlapped
    memset(&overlapped_, 0, sizeof(OVERLAPPED));

    // get header
    CHAR *data = (CHAR *)data_;

    int result;
    if ((op_ & Op::WRITE) == 0) {
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
            // "real" error (e.g. if nobody listens on the other end we get WSAECONNRESET = 10054)
            setReady(0);
            //return false;
        }
    }
}

void UdpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
    DWORD transferred;
    DWORD flags;
    auto result = WSAGetOverlappedResult(device_.socket_, overlapped, &transferred, false, &flags);
    if (!result) {
        // "real" error or cancelled (ERROR_OPERATION_ABORTED): return zero size
        auto error = WSAGetLastError();
        transferred = 0;
    }

    // remove from list of active transfers
    remove2();

    // transfer finished
    setReady(transferred);
}

} // namespace coco
