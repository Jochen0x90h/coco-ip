#pragma once

#include <coco/PackedValue.hpp>
#include <coco/String.hpp>
#include <optional>
#if defined(_WIN32)
#include <coco/platform/WindowsDef.hpp>
#include <winsock2.h> // see https://learn.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
#include <Windows.h>
#include <coco/platform/WindowsUndef.hpp>
#elif defined(__linux__)
#include <sys/socket.h>
#else
constexpr int AF_INET = 1;
constexpr int AF_INET6 = 2;
#endif


namespace coco {

/// @brief Helpers vor IP protocol
///
namespace ip {

// IPv4
namespace v4 {

/// @brief IPv4 protocol ID
constexpr uint16_t PROTOCOL_ID =
#if defined(_WIN32) || defined(__linux__)
    AF_INET;
#else
    2;
#endif


union alignas(4) Address {
    uint8_t u8[4];
    U16B u16[2];
    U32B u32[1];


    Address() : u32{0} {}
    Address(const Address &a) : u32{a.u32[0]} {}

    /// @brief Create an address from a string
    /// @param s String containing the address, e.g. "::1" for localhost
    /// @return Address
    static std::optional<Address> fromString(String s);

    bool operator ==(const Address &b) const {
        return u32[0] == b.u32[0];
    }
};

struct Endpoint {
    uint16_t protocolId = PROTOCOL_ID;
    U16B port;
    Address address;
    uint8_t zero[8];


    //static Endpoint fromString(String s, uint16_t defaultPort);

    bool operator ==(const Endpoint &e) const {
        return e.address == address && e.port == port;
    }
};

} // namespace v4


// IPv6
namespace v6 {

/// @brief IPv6 protocol ID
constexpr uint16_t PROTOCOL_ID =
#if defined(_WIN32) || defined(__linux__)
    AF_INET6;
#else
    2;
#endif


union alignas(4) Address {
    U8 u8[16];
    U16B u16[8];
    U32B u32[4];


    Address() : u32{0, 0, 0, 0} {}
    Address(const Address &a) : u32{a.u32[0], a.u32[1], a.u32[2], a.u32[3]} {}

    /// @brief Create an address from a string
    /// @param s String containing the address, e.g. "::1" for localhost
    /// @return Address
    static std::optional<Address> fromString(String s);

    /// @brief Check if it is a link local address.
    /// @return True if link local address
    bool linkLocal() const {
        return u32[0] == 0xfe800000U && u32[1] == 0;
    }

    bool operator ==(const Address &b) const {
        for (int i = 0; i < 4; ++i) {
            if (u32[i] != b.u32[i])
                return false;
        }
        return true;
    }
};

struct Endpoint {
    uint16_t protocolId = PROTOCOL_ID;
    U16B port;
    U32B flowInfo;
    Address address;
    uint32_t scopeId;


    //static Endpoint fromString(String s, uint16_t defaultPort);

    bool operator ==(const Endpoint &e) const {
        return e.address == address && e.port == port;
    }
};

} // namespace v6


/// @brief Endpoin either for IPv4 or v6
/// Must be initialized, e.g. ip::Endpoint ep = {}
/// or ip::Endpoint ep = {.v6 = {.port = 80, .address = myAddress}};
union Endpoint {
    uint16_t protocolId;
    struct {
        uint16_t protocolId;
        U16B port;
    } generic;
    v4::Endpoint v4;
    v6::Endpoint v6;
};

} // namespace ip
} // namespace coco
