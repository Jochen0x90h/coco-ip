#pragma once

#ifdef _WIN32
#include "IpSocket_Win32.hpp"
namespace coco {
using IpSocket_native = IpSocket_Win32;
}
#endif
