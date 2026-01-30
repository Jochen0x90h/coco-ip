#pragma once

#include <coco/platform/IpSocket_native.hpp>


using namespace coco;

// drivers for UdpSocketTest
struct Drivers {
    Loop_native loop;
    IpSocket_native socket{loop, SOCK_DGRAM, IPPROTO_UDP};
    IpSocket_native::Buffer buffer1{socket, 128};
    IpSocket_native::Buffer buffer2{socket, 128};
};

Drivers drivers;
