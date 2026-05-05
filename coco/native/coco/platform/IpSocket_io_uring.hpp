#pragma once

#include <coco/IpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#include <coco/platform/Loop_native.hpp>
#include <netinet/ip.h>


namespace coco {

/// @brief IP socket for TCP client and connected UDP using io_uring on Linux.
///
class IpSocket_io_uring : public IpSocket, public Loop_io_uring::CompletionHandler {
public:
    /// @brief Constructor.
    /// @param loop event loop
    /// @param type socket type such as SOCK_STREAM or SOCK_DGRAM
    /// @param protocol protocol such as IPPROTO_TCP or IPPROTO_UDP
    IpSocket_io_uring(Loop_io_uring &loop, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);

    ~IpSocket_io_uring() override;


    /// @brief Buffer for transferring data to/from a TCP socket.
    ///
    class Buffer : public coco::Buffer, public Loop_io_uring::CompletionHandler, public IntrusiveListNode {
        friend class IpSocket_io_uring;
    public:
        Buffer(IpSocket_io_uring &device, int size);
        ~Buffer() override;

        bool start() override;
        bool cancel() override;

    protected:
        bool transfer();
        void handle(io_uring_cqe &cqe) override;

        IpSocket_io_uring &device_;
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
    void handle(io_uring_cqe &cqe) override;

    Loop_io_uring &loop_;
    int type_;
    int protocol_;

    // socket handle
    static constexpr int INVALID_SOCKET = -1;
    int socket_ = INVALID_SOCKET;

    // list of buffers
    IntrusiveList<Buffer> buffers_;
};

} // namespace coco
