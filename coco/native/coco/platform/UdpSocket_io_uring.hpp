#pragma once

#include <coco/UdpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#include <coco/platform/Loop_native.hpp>
#include <netinet/ip.h>


namespace coco {

class UdpSocket_io_uring : public UdpSocket {
public:
    /// @brief Constructor.
    /// @param loop event loop
    UdpSocket_io_uring(Loop_io_uring &loop);

    ~UdpSocket_io_uring() override;

    // UdpSocket methods
    bool open(uint16_t protocolId, int localPort) override;
    bool join(ip::v6::Address const &multicastGroup) override;

    // BufferDevice methods
    class Buffer;
    int getBufferCount() override;
    Buffer &getBuffer(int index) override;

    // Device methods
    void close() override;


    /// @brief Buffer for transferring data to/from a file.
    ///
    class Buffer : public coco::Buffer, public Loop_io_uring::CompletionHandler, public IntrusiveListNode, public IntrusiveListNode2 {
        friend class UdpSocket_io_uring;
    public:
        Buffer(UdpSocket_io_uring &device, int size);
        ~Buffer() override;

        // Buffer methods
        bool start(Op op) override;
        bool cancel() override;

    protected:
        void start();
        void handle(io_uring_cqe &cqe) override;

        UdpSocket_io_uring &device_;
        union {
            sockaddr generic;
            sockaddr_in v4;
            sockaddr_in6 v6;
        } endpoint_;
        iovec buffer_;
        msghdr message_;
        Op op_;
    };

protected:
    //void handle(io_uring_cqe &cqe) override;

    Loop_io_uring &loop_;

    // socket handle
    static constexpr int INVALID_SOCKET = -1;
    int socket_ = INVALID_SOCKET;

    // list of buffers
    IntrusiveList<Buffer> buffers_;

    // pending transfers
    IntrusiveList2<Buffer> transfers_;
};

} // namespace coco
