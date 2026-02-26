#include "IpSocket_Win32.hpp"
#include <ws2tcpip.h>
#include <mswsock.h>
//#include <iostream>


namespace coco {

IpSocket_Win32::IpSocket_Win32(Loop_Win32 &loop, int type, int protocol)
    : IpSocket(State::DISABLED)
    , loop_(loop)
    , type_(type), protocol_(protocol)
{
    // initialize winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
}

IpSocket_Win32::~IpSocket_Win32() {
    closesocket(socket_);
}

int IpSocket_Win32::getBufferCount() {
    return buffers_.count();
}

IpSocket_Win32::Buffer &IpSocket_Win32::getBuffer(int index) {
    return buffers_.get(index);
}

bool IpSocket_Win32::connect(const ip::Endpoint &endpoint, int size, int localPort) {
    if (socket_ != INVALID_SOCKET)
        return false;

    // create socket
    SOCKET socket = WSASocket(endpoint.protocolId, type_, protocol_, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        setSystemError(error);
        return false;
    }

    int reuse = 1;
    sockaddr_in6 local = {.sin6_family = endpoint.protocolId, .sin6_port = htons(localPort)};

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
        goto error;

    // bind to any local address/port (required by ConnectEx)
    if (bind(socket, (sockaddr *)&local, sizeof(local)) < 0)
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

    Events events;
    if (type_ == SOCK_DGRAM) {
        // connect UDP
        if (::connect(socket, (struct sockaddr *)&endpoint, size) < 0)
            goto error;

        socket_ = socket;
        setSuccess();

        // set state
        state_ = State::READY;

        // set buffers READY
        // transfers can be started when device is OPENING, but get submitted to the OS when the device becomes READY
        for (auto &buffer : buffers_) {
            buffer.setSuccess(0);
            buffer.setReady();
        }

        events = Events::ENTER_OPENING | Events::ENTER_READY;
    } else {
        // connect TCP

        // get pointer to ConnectEx function
        GUID ConnectExGuid = WSAID_CONNECTEX;
        LPFN_CONNECTEX ConnectEx = NULL;
        DWORD transferred;
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &ConnectExGuid, sizeof(ConnectExGuid),
            &ConnectEx, sizeof(ConnectEx),
            &transferred, NULL, NULL) != 0)
        {
            goto error;
        }

        // connect TCP
        memset(&overlapped_, 0, sizeof(OVERLAPPED));
        if (ConnectEx(socket, (struct sockaddr *)&endpoint, size,
            nullptr, 0, // send buffer
            nullptr, // transferred
            &overlapped_) == FALSE)
        {
            int error = WSAGetLastError();
            if (error != ERROR_IO_PENDING) {
                // error
                setSystemError(error);
                closesocket(socket);
                return false;
            }
        }
        socket_ = socket;
        setSuccess();

        // set state
        state_ = State::OPENING;

        events = Events::ENTER_OPENING;
    }

    // resume all coroutines waiting for state change
    notify(events);

    return true;
error:
    int error = WSAGetLastError();
    setSystemError(error);
    closesocket(socket);
    return false;
}

void IpSocket_Win32::close() {
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

void IpSocket_Win32::handle(OVERLAPPED *overlapped) {
    if (overlapped == &overlapped_) {
        // result of ConnectEx
        DWORD transferred;
        DWORD flags;
        auto result = WSAGetOverlappedResult(socket_, overlapped, &transferred, false, &flags);
        if (!result) {
            // error
            // ERROR_OPERATION_ABORTED: cancelled
            // ERROR_CONNECTION_REFUSED: connection refused
            auto error = WSAGetLastError();
            setSystemError(error);

            // close socket
            closesocket(socket_);
            socket_ = INVALID_SOCKET;

            // set state
            state_ = State::DISABLED;

            // resume all coroutines waiting for state change
            notify(Events::ENTER_DISABLED);
        } else {
            setsockopt(socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

            // set state
            state_ = State::READY;

            // set buffers READY
            // transfers can be started when device is OPENING, but get submitted to the OS when the device becomes READY
            for (auto &buffer : buffers_) {
                buffer.setSuccess(0);
                buffer.setReady();
            }

            // resume all coroutines waiting for state change
            notify(Events::ENTER_READY);
        }
    } else {
        // search the buffer that caused the event
        for (auto &buffer : buffers_) {
            if (overlapped == &buffer.overlapped_) {
                buffer.handle(overlapped);
                break;
            }
        }
    }
}


// IpSocket_Win32::Buffer

IpSocket_Win32::Buffer::Buffer(IpSocket_Win32 &device, int size)
    : coco::Buffer(new uint8_t[size], size, device.state_)
    , device_(device)
{
    device.buffers_.add(*this);
}

IpSocket_Win32::Buffer::~Buffer() {
    delete [] data_;
}

bool IpSocket_Win32::Buffer::start() {
    if (state_ != State::READY || (op_ & Op::READ_WRITE) == 0 || size_ == 0) {
        assert(state_ != State::BUSY);
        setSuccess(0);
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

bool IpSocket_Win32::Buffer::cancel() {
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

bool IpSocket_Win32::Buffer::transfer() {
    // initialize overlapped
    memset(&overlapped_, 0, sizeof(OVERLAPPED));

    int result;
    if ((Op(steps_) & Op::WRITE) == 0) {
        // receive
        WSABUF buffer{capacity_, (CHAR*)data_};
        DWORD flags = 0;
        result = WSARecv(device_.socket_, &buffer, 1, nullptr, &flags, &overlapped_, nullptr);
    } else {
        // send
        WSABUF buffer{size_, (CHAR*)data_};
        result = WSASend(device_.socket_, &buffer, 1, nullptr, 0, &overlapped_, nullptr);
    }
    if (result != 0) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            // error
            setSystemError(error);
            setReady();
            return false;
        }
    }
    return true;
}

void IpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
    DWORD transferred;
    DWORD flags;
    auto result = WSAGetOverlappedResult(device_.socket_, overlapped, &transferred, false, &flags);
    if (result) {
        // success

        // check for connection closed by peer
        if (Op(steps_) == Op::READ && transferred == 0) {
            device_.close();
            return;
        }

        // check for read after write
        if (Op(steps_) == Op::READ_WRITE) {
            steps_ = int(Op::READ);
            transfer();
            return;
        }

        setSuccess(transferred);
    } else {
        // error
        // ERROR_OPERATION_ABORTED: cancelled
        auto error = WSAGetLastError();
        setSystemError(error);
    }

    // transfer finished
    setReady();
}

} // namespace coco
