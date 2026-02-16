#pragma once

#include <coco/IpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#include <coco/platform/Loop_native.hpp>
#include <netinet/ip.h>


namespace coco {

class IpSocket_io_uring : public IpSocket, public Loop_io_uring::CompletionHandler {
public:
    /// @brief Constructor.
    /// @param loop event loop
    /// @param type socket type such as SOCK_STREAM or SOCK_DGRAM
    /// @param protocol protocol such as IPPROTO_TCP or IPPROTO_UDP
    IpSocket_io_uring(Loop_io_uring &loop, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);

    ~IpSocket_io_uring() override;

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
    class Buffer : public coco::Buffer, public Loop_io_uring::CompletionHandler, public IntrusiveListNode, public IntrusiveListNode2 {
        friend class IpSocket_io_uring;
    public:
        Buffer(IpSocket_io_uring &device, int size);
        ~Buffer() override;

        bool start(Op op) override;
        bool cancel() override;

    protected:
        void start();
        void handle(io_uring_cqe &cqe) override;

        IpSocket_io_uring &device_;
        Op op_;
    };

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

    // pending transfers
    IntrusiveList2<Buffer> transfers_;
};

} // namespace coco
