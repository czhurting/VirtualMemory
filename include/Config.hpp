#pragma once

constexpr int VIRTUAL_MEMORY__MAJOR_VERSION = 1;
constexpr int VIRTUAL_MEMORY__MINOR_VERSION = 0;
constexpr int VIRTUAL_MEMORY__PATCH_VERSION = 0;

// pid_t across platforms:
//  - Linux/Unix: <sys/types.h> already provides the real pid_t (signed int).
//    Redefining it here would clash, so just include the system header.
//  - Windows: there is no standard pid_t; GetCurrentProcessId() returns DWORD
//    (unsigned long). Define our own pid_t to match, so the code can use one name.
#ifdef _WIN32
using pid_t = unsigned long;   // == DWORD on Windows
#else
#include <sys/types.h>         // brings in the real system pid_t
#endif