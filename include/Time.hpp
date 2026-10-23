#ifndef VM_TIME_HPP
#define VM_TIME_HPP

#include <cstdint>
#include <Config.hpp>   // pid_t (cross-platform)

// get_process_create_time(): a stable per-process creation token.
//   - Windows: process creation FILETIME packed as (high << 32) | low
//             (100-ns intervals since 1601-01-01)
//   - Linux:   starttime from /proc/<pid>/stat, in clock ticks since boot
// The exact meaning is OS-specific, but the encoding is consistent within one
// OS, so a token recorded at registration can be compared against a token read
// later for the same PID. That comparison is exactly how PID reuse is detected
// in the observer model (a recycled PID yields a different creation token).
//
// Three overloads (not just "self"):
//   get_process_create_time()              -> current process
//   get_process_create_time(HANDLE)        -> any process you already hold a handle to (Windows)
//   get_process_create_time(pid_t / int)   -> any process by PID (cross-platform)
//
// All overloads are inline so this header can be included by multiple TUs
// without triggering multiple-definition (ODR) errors.

#ifdef _WIN32
#include <windows.h>

// Core overload: query an arbitrary process by its HANDLE.
// Pass GetCurrentProcess() to query yourself, or any HANDLE you already hold
// (e.g. one opened via OpenProcess, or handed to you by another subsystem) to
// skip the open/close round-trip entirely.
inline std::uint64_t get_process_create_time(HANDLE handle)
{
    FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
    if (!GetProcessTimes(handle, &ftCreate, &ftExit, &ftKernel, &ftUser))
        return 0;
    return (static_cast<std::uint64_t>(ftCreate.dwHighDateTime) << 32) |
            static_cast<std::uint64_t>(ftCreate.dwLowDateTime);
}

// Convenience overload: open the target by PID, query, then close.
inline std::uint64_t get_process_create_time(pid_t pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr)
        return 0;
    std::uint64_t result = get_process_create_time(h);
    CloseHandle(h);
    return result;
}

// Self: just a specialization of the HANDLE overload.
inline std::uint64_t get_process_create_time()
{
    return get_process_create_time(GetCurrentProcess());
}

#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <cstdio>

// Extract starttime (field 22) from a /proc/<pid>/stat buffer.
// comm may contain spaces but never ')', so scanning to the first ')' is safe.
inline std::uint64_t parse_starttime(const char* buf)
{
    const char* p = buf;
    while (*p && *p != ')')
        ++p;
    if (*p == ')')
        ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    // After ')', skip stat fields 3..21 (19 fields) to land on field 22 (starttime).
    for (int i = 0; i < 19; ++i)
    {
        while (*p && *p != ' ' && *p != '\t')
            ++p;
        while (*p == ' ' || *p == '\t')
            ++p;
    }
    std::uint64_t v = 0;
    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + static_cast<std::uint64_t>(*p - '0');
        ++p;
    }
    return v;
}

inline std::uint64_t get_process_create_time()
{
    int fd = open("/proc/self/stat", O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    return parse_starttime(buf);
}

inline std::uint64_t get_process_create_time(pid_t pid)
{
    char path[32];
    if (snprintf(path, sizeof(path), "/proc/%u/stat", static_cast<unsigned>(pid)) < 0)
        return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    return parse_starttime(buf);
}
#endif

#endif // VM_TIME_HPP
