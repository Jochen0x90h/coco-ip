#include "IpSocket_Win32.hpp"
#include <ws2tcpip.h>
#include <mswsock.h>
#include <iostream>


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
    WSACleanup();
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
        int e = WSAGetLastError();
        return false;
    }

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        //int e = WSAGetLastError();
        closesocket(socket);
        return false;
    }

    // bind to any local address/port (required by ConnectEx)
    sockaddr_in6 local = {.sin6_family = endpoint.protocolId, .sin6_port = htons(localPort)};
    if (bind(socket, (sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        int e = WSAGetLastError();
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
        int e = WSAGetLastError();
        closesocket(socket);
        return false;
    }

    if (type_ == SOCK_DGRAM) {
        // connect UDP
        if (::connect(socket, (struct sockaddr *)&endpoint, size) == SOCKET_ERROR) {
            int error = WSAGetLastError();
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
    } else {
        // get pointer to ConnectEx function
        GUID ConnectExGuid = WSAID_CONNECTEX;
        LPFN_CONNECTEX ConnectEx = NULL;
        DWORD transferred;
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &ConnectExGuid, sizeof(ConnectExGuid),
            &ConnectEx, sizeof(ConnectEx),
            &transferred, NULL, NULL) != 0)
        {
            int error = WSAGetLastError();
            closesocket(socket);
            return false;
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
                // "real" error
                closesocket(socket);
                return false;
            }
        }
        socket_ = socket;

        // set state
        st.set(State::OPENING);

        // enable buffers
        for (auto &buffer : buffers_) {
            buffer.setReady();
        }

        // resume all coroutines waiting for state change
        st.notify(Events::ENTER_OPENING);
    }

    return true;
}

void IpSocket_Win32::close() {
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

void IpSocket_Win32::handle(OVERLAPPED *overlapped) {
    if (overlapped == &overlapped_) {
        // result of ConnectEx
        DWORD transferred;
        DWORD flags;
        auto result = WSAGetOverlappedResult(socket_, overlapped, &transferred, false, &flags);
        if (!result) {
            // "real" error or cancelled (ERROR_OPERATION_ABORTED): close
            auto error = WSAGetLastError();
            close();
        } else {
            setsockopt(socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

            // set state
            st.set(State::READY);

            // start pending transfers
            for (auto &buffer : transfers_) {
                buffer.start();
            }

            // resume all coroutines waiting for state change
            st.notify(Events::ENTER_READY);
        }
    } else {
        for (auto &buffer : transfers_) {
            if (overlapped == &buffer.overlapped_) {
                buffer.handle(overlapped);
                break;
            }
        }
    }
}


// IpSocket_Win32::Buffer

IpSocket_Win32::Buffer::Buffer(IpSocket_Win32 &device, int size)
    : coco::Buffer(new uint8_t[size], size, device.st.state)
    , device_(device)
{
    device.buffers_.add(*this);
}

IpSocket_Win32::Buffer::~Buffer() {
    delete [] data_;
}

bool IpSocket_Win32::Buffer::start(Op op) {
    if (st.state != State::READY) {
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

bool IpSocket_Win32::Buffer::cancel() {
    if (st.state != State::BUSY)
        return false;

    auto result = CancelIoEx((HANDLE)device_.socket_, &overlapped_);
    if (!result) {
        auto e = WSAGetLastError();
        std::cerr << "cancel error " << e << std::endl;
    }

    return true;
}

void IpSocket_Win32::Buffer::start() {
    // initialize overlapped
    memset(&overlapped_, 0, sizeof(OVERLAPPED));

    int result;
    if ((op_ & Op::WRITE) == 0) {
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
            // "real" error
            setReady(0);
        }
    }
}

void IpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
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
