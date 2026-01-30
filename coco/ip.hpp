#pragma once

#include <coco/String.hpp>
#include <optional>


namespace coco {

/// @brief Helpers vor IP protocol
///
namespace ip {

/// @brief 16 bit integer that stores internally in network byte order.
///
struct Net16 {
    uint16_t value;

    Net16() = default;

    Net16(uint16_t x) {
        value = (x >> 8)
        | (x << 8);
    }

    operator uint16_t () const {
        return (value >> 8)
        | (value << 8);
    }

    void operator =(uint16_t x) {
        value = (x >> 8)
        | (x << 8);
    }
};

template <typename T>
bool operator ==(const Net16 &x, const T &y) {
    return x.value == Net16(y).value;
}

template <typename T>
bool operator ==(const T &x, const Net16 &y) {
    return Net16(x).value == y.value;
}

inline bool operator ==(const Net16 &x, const Net16 &y) {
    return x.value == y.value;
}


/// @brief 32 bit integer that stores internally in network byte order.
///
struct Net32 {
    uint32_t value;

    Net32() = default;

    Net32(uint32_t x) {
        value = (x >> 24)
        | ((x >> 8) & 0x0000ff00)
        | ((x << 8) & 0x00ff0000)
        | (x << 24);
    }

    operator uint32_t () const {
        return (value >> 24)
        | ((value >> 8) & 0x0000ff00)
        | ((value << 8) & 0x00ff0000)
        | (value << 24);
    }

    void operator =(uint32_t x) {
        value = ((x >> 24)
        | ((x >> 8) & 0x0000ff00)
        | ((x << 8) & 0x00ff0000)
        | (x << 24));
    }
};

template <typename T>
bool operator ==(const Net32 &x, const T &y) {
    return x.value == Net32(y).value;
}

template <typename T>
bool operator ==(const T &x, const Net32 &y) {
    return Net32(x).value == y.value;
}

inline bool operator ==(const Net32 &x, const Net32 &y) {
    return x.value == y.value;
}


constexpr uint32_t hostToNetwork(uint16_t x) {
    return (x >> 8)
        | (x << 8);
}

constexpr uint32_t hostToNetwork(uint32_t x) {
    return (x >> 24)
        | ((x >> 8) & 0x0000ff00)
        | ((x << 8) & 0x00ff0000)
        | (x << 24);
}

// IPv4
namespace v4 {

/// @brief IPv4 protocol ID, equals AF_INET
constexpr uint16_t PROTOCOL_ID = 2;

union Address {
    uint8_t u8[4];
    Net16 u16[2];
    Net32 u32[1];


    /// @brief Create an address from a string
    /// @param s String containing the address, e.g. "::1" for localhost
    /// @return Address
    static std::optional<Address> fromString(String s);

    bool operator ==(const Address &b) const {
        return this->u32[0] != b.u32[0];
    }
};

struct Endpoint {
    uint16_t protocolId = PROTOCOL_ID;
    Net16 port;
    Address address;
    uint8_t zero[8];


    //static Endpoint fromString(String s, uint16_t defaultPort);

    bool operator ==(const Endpoint &e) const {
        return e.address == this->address && e.port == this->port;
    }
};

} // namespace v4


// IPv6
namespace v6 {

/// @brief IPv6 protocol ID, equals AF_INET6
constexpr uint16_t PROTOCOL_ID = 23;


union Address {
    uint8_t u8[16];
    Net16 u16[8];
    Net32 u32[4];


    /// @brief Create an address from a string
    /// @param s String containing the address, e.g. "::1" for localhost
    /// @return Address
    static std::optional<Address> fromString(String s);

    /// @brief Check if it is a link local address.
    /// @return True if link local address
    bool linkLocal() const {
        return this->u32[0] == (0xfe800000U) && this->u32[1] == 0;
    }

    bool operator ==(const Address &b) const {
        for (int i = 0; i < 4; ++i) {
            if (this->u32[i] != b.u32[i])
                return false;
        }
        return true;
    }
};

struct Endpoint {
    uint16_t protocolId = PROTOCOL_ID;
    Net16 port;
    Net32 flowInfo;
    Address address;
    uint32_t scopeId;


    //static Endpoint fromString(String s, uint16_t defaultPort);

    bool operator ==(const Endpoint &e) const {
        return e.address == this->address && e.port == this->port;
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
        Net16 port;
    } generic;
    v4::Endpoint v4;
    v6::Endpoint v6;
};

} // namespace ip
} // namespace coco
