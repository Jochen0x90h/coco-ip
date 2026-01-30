#pragma once

#include <coco/ip.hpp>
#include <coco/BufferDevice.hpp>


namespace coco {

/// @brief Connection based IP socket.
/// Used for UDP with fixed destination address or TCP client that connects to serever.
class IpSocket : public BufferDevice {
public:
    IpSocket(State state) : BufferDevice(state) {}

    /// @brief Connect to a server.
    /// For UDP, the socket immediately transitions to ready state.
    /// For TCP, the socket immediately transitions to opening state and to ready state later when connected.
    /// @param endpoint Endpoint (address and port) of server
    /// @param size Size of endpoint structure
    /// @param localPort Local port to bind to (0 = any)
    /// @return true if connect operation was started, false on error
    virtual bool connect(const ip::Endpoint &endpoint, int size = sizeof(ip::Endpoint), int localPort = 0) = 0;
    bool connect(const ip::v4::Endpoint &endpoint, int localPort = 0) {return connect((ip::Endpoint &)endpoint, sizeof(endpoint), localPort);}
    bool connect(const ip::v6::Endpoint &endpoint, int localPort = 0) {return connect((ip::Endpoint &)endpoint, sizeof(endpoint), localPort);}
};

} // namespace coco
