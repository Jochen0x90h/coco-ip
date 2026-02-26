#pragma once

#include <coco/platform/IpSocket_native.hpp>


using namespace coco;

// drivers for UdpSocketTest
struct Drivers {
    Loop_native loop;
    IpSocket_native socket{loop, SOCK_STREAM, IPPROTO_TCP};
    IpSocket_native::Buffer buffer{socket, 1024};
};

Drivers drivers;
