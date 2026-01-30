#pragma once

#include "ip.hpp"
#include <coco/BufferDevice.hpp>


namespace coco {

/// @brief Connectionless UDP socket.
/// UDP is the IP protocol for datagram communication.
class UdpSocket : public BufferDevice {
public:
    UdpSocket(State state) : BufferDevice(state) {}

    /// @brief Open the socket on a local port.
    /// @param protocolId Protocol id such as ip::v4::PROTOCOL_ID or ip::v6::PROTOCOL_ID
    /// @param localPort Local port number
    /// @return true if successful
    virtual bool open(uint16_t protocolId, int localPort) = 0;

    /// @brief Join an IPv6 multicast group
    /// @param multicastGroup Address of multicast group
    /// @return true if successful
    virtual bool join(const ip::v6::Address &multicastGroup) = 0;
};

} // namespace coco
