#pragma once

#include <coco/IpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#include <coco/platform/Loop_native.hpp>


namespace coco {

/// @brief IP socket for TCP client and connected UDP using IO completion ports on Windows.
///
class IpSocket_Win32 : public IpSocket, public Loop_Win32::CompletionHandler {
public:
    /// @brief Constructor.
    /// Calls WSAStartup(), WSACleanup() has to be called by the user if necessary.
    /// @param loop event loop
    /// @param type socket type such as SOCK_STREAM or SOCK_DGRAM
    /// @param protocol protocol such as IPPROTO_TCP or IPPROTO_UDP
    IpSocket_Win32(Loop_Win32 &loop, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);

    ~IpSocket_Win32() override;


    /// @brief Buffer for transferring data to/from a TCP socket.
    ///
    class Buffer : public coco::Buffer, public IntrusiveListNode {
        friend class IpSocket_Win32;
    public:
        Buffer(IpSocket_Win32 &device, int size);
        ~Buffer() override;

        bool start() override;
        bool cancel() override;

    protected:
        bool transfer();
        void handle(OVERLAPPED *overlapped);

        IpSocket_Win32 &device_;
        OVERLAPPED overlapped_;
    };


    // TcpSocket methods
    bool connect(const ip::Endpoint &endpoint, int size = sizeof(ip::Endpoint), int localPort = 0) override;
    using IpSocket::connect;

    // BufferDevice methods
    int getBufferCount() override;
    Buffer &getBuffer(int index) override;

    // Device methods
    void close() override;

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
};

} // namespace coco
