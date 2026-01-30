#include <coco/ip.hpp>
#define NOMINMAX
#include <Winsock2.h>
#include <ws2tcpip.h>


namespace coco {
namespace ip {

namespace v4 {

std::optional<Address> Address::fromString(String str) {
    // copy string to ensure zero termination
    char buffer[64];
    int count = std::min(str.size(), int(std::size(buffer)) - 1);
    std::copy(str.begin(), str.begin() + count, buffer);
    buffer[count] = 0;

    // convert to address
    Address address;
    int result = inet_pton(AF_INET, buffer, (in_addr *)&address);
    if (result)
        return address;
    return {};
}

} // namespace v4

namespace v6 {

std::optional<Address> Address::fromString(String str) {
    // copy string to ensure zero termination
    char buffer[64];
    int count = std::min(str.size(), int(std::size(buffer)) - 1);
    std::copy(str.begin(), str.begin() + count, buffer);
    buffer[count] = 0;

    // convert to address
    Address address;
    int result = inet_pton(AF_INET6, buffer, (in6_addr *)&address);
    if (result)
        return address;
    return {};
}

} // namespace v6


} // namespace ip
} // namespace coco
