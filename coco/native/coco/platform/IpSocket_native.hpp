#pragma once

#ifdef _WIN32
#include "IpSocket_Win32.hpp"
namespace coco {
using IpSocket_native = IpSocket_Win32;
}
#endif
#ifdef __linux__
#include "IpSocket_io_uring.hpp"
namespace coco {
using IpSocket_native = IpSocket_io_uring;
}
#endif
