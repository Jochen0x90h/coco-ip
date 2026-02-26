#pragma once

#include <coco/UdpSocket.hpp>
#include <coco/IntrusiveList.hpp>
#include <coco/platform/Loop_native.hpp>
#include <ws2tcpip.h> // sockaddr_in6


namespace coco {

/// @brief UDP message socket using IO completion ports on Windows.
///
class UdpSocket_Win32 : public UdpSocket, public Loop_Win32::CompletionHandler {
public:
    /// @brief Constructor.
    /// Calls WSAStartup(), WSACleanup() has to be called by the user if necessary.
    /// @param loop event loop
    UdpSocket_Win32(Loop_Win32 &loop);

    ~UdpSocket_Win32() override;

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
    class Buffer : public coco::Buffer, public IntrusiveListNode {//, public IntrusiveListNode2 {
        friend class UdpSocket_Win32;
    public:
        Buffer(UdpSocket_Win32 &device, int size);
        ~Buffer() override;

        // Buffer methods
        bool start() override;
        bool cancel() override;

    protected:
        bool transfer();
        void handle(OVERLAPPED *overlapped);

        UdpSocket_Win32 &device_;
        union {
            sockaddr generic;
            sockaddr_in v4;
            sockaddr_in6 v6;
        } endpoint_;
        INT endpointSize_;
        OVERLAPPED overlapped_;
    };

protected:
    void handle(OVERLAPPED *overlapped) override;

    Loop_Win32 &loop_;

    // socket handle
    SOCKET socket_ = INVALID_SOCKET;

    // list of buffers
    IntrusiveList<Buffer> buffers_;
};

} // namespace coco
