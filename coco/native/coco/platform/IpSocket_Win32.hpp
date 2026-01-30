#pragma once

#include <coco/IpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // see https://learn.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
#include <coco/platform/Loop_native.hpp> // includes Windows.h (after winsock2.h)


namespace coco {

class IpSocket_Win32 : public IpSocket, public Loop_Win32::CompletionHandler {
public:
    /// @brief Constructor.
    /// @param loop event loop
    /// @param type socket type such as SOCK_STREAM or SOCK_DGRAM
    /// @param protocol protocol such as IPPROTO_TCP or IPPROTO_UDP
    IpSocket_Win32(Loop_Win32 &loop, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);

    ~IpSocket_Win32() override;

    // TcpSocket methods
    bool connect(const ip::Endpoint &endpoint, int size = sizeof(ip::Endpoint), int localPort = 0) override;
    using IpSocket::connect;

    // BufferDevice methods
    class Buffer;
    int getBufferCount() override;
    Buffer &getBuffer(int index) override;

    // Device methods
    void close() override;


    /// @brief Buffer for transferring data to/from a TCP socket.
    ///
    class Buffer : public coco::Buffer, public IntrusiveListNode, public IntrusiveListNode2 {
        friend class IpSocket_Win32;
    public:
        Buffer(IpSocket_Win32 &device, int size);
        ~Buffer() override;

        bool start(Op op) override;
        bool cancel() override;

    protected:
        void start();
        void handle(OVERLAPPED *overlapped);

        IpSocket_Win32 &device_;
        OVERLAPPED overlapped_;
        Op op_;
    };

protected:
    void handle(OVERLAPPED *overlapped) override;

    Loop_Win32 &loop_;
    int type_;
    int protocol_;

    // socket handle
    SOCKET socket_ = INVALID_SOCKET;
    OVERLAPPED overlapped_;

    // list of buffers
    IntrusiveList<Buffer> buffers_;

    // pending transfers
    IntrusiveList2<Buffer> transfers_;
};

} // namespace coco
