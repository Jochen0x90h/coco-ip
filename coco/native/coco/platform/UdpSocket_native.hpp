#pragma once

#if defined(_WIN32)
#include "UdpSocket_Win32.hpp"
namespace coco {
using UdpSocket_native = UdpSocket_Win32;
}
#elif defined(__linux__)
#include "UdpSocket_io_uring.hpp"
namespace coco {
using UdpSocket_native = UdpSocket_io_uring;
}
#endif
