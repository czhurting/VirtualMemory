#pragma once

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace SystemInfo
{
#ifdef _WIN32
    struct SYSTEM_INFO
    {
        DWORD page_size;
        DWORD nproc_onln;
        DWORD nproc_conf;
    };

    inline SYSTEM_INFO GetSystemInfo()
    {
        ::SYSTEM_INFO si;
        ::GetSystemInfo(&si);
        return { si.dwPageSize, si.dwNumberOfProcessors, si.dwNumberOfProcessors };
    }

    inline DWORD GetPID()
    {
        return ::GetCurrentProcessId();
    }
#else

    struct SYSTEM_INFO
    {
        long page_size{};
        long nproc_onln{};
        long nproc_conf{};
    };

    inline SYSTEM_INFO GetSystemInfo()
    {
        return {
            sysconf(_SC_PAGESIZE),
            sysconf(_SC_NPROCESSORS_ONLN),
            sysconf(_SC_NPROCESSORS_CONF)
        };
    }
    inline int GetPID()
    {
        return ::getpid();
    }
#endif
}