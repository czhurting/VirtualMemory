#ifndef VM_TIME
#define VM_TIME

#include <cstdint>

#ifdef _WIN32
#include <windows.h>

std::uint32_t get_process_create_time() {
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
        return 0;
    }
    // FILETIME 是 100ns 间隔，从 1601/1/1 起算
    // 转成 Unix 时间戳（秒）或者直接用低 32 位做标识都行
    ULARGE_INTEGER ul;
    ul.LowPart = createTime.dwLowDateTime;
    ul.HighPart = createTime.dwHighDateTime;
    // Windows tick → Unix epoch (秒)
    return static_cast<std::uint32_t>((ul.QuadPart / 10000000ULL) - 11644473600ULL);
}
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

std::uint32_t get_process_create_time() {
    // /proc/self/stat 第 22 个字段是 starttime（时钟滴答数）
    int fd = open("/proc/self/stat", O_RDONLY);
    if (fd < 0) return 0;

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;

    buf[n] = '\0';

    // 跳过 comm 字段（可能含空格，用括号包着）
    char* p = buf;
    while (*p && *p != '(') p++;
    if (*p == '(') {
        while (*p && *p != ')') p++;
        if (*p == ')') p++;
    }
    while (*p && *p == ' ') p++;

    // 跳过前 21 个字段
    int field = 1;
    while (*p && field < 20) {
        if (*p == ' ') {
            field++;
            while (*p == ' ') p++;
        }
        else {
            p++;
        }
    }

    unsigned long long starttime = 0;
    while (*p >= '0' && *p <= '9') {
        starttime = starttime * 10 + (*p - '0');
        p++;
    }

    // starttime 是 clock ticks，转秒
    long clk_tck = sysconf(_SC_CLK_TCK);  // 通常 100
    if (clk_tck <= 0) clk_tck = 100;

    // 再拿系统启动时间
    unsigned long long btime = 0;
    int stat_fd = open("/proc/stat", O_RDONLY);
    if (stat_fd >= 0) {
        char stat_buf[4096];
        size_t sn = read(stat_fd, stat_buf, sizeof(stat_buf) - 1);
        close(stat_fd);
        if (sn <= 0) return 0;
        stat_buf[sn] = '\0';
        if (bp) {
            bp += 6;
            while (*bp >= '0' && *bp <= '9') {
                btime = btime * 10 + (*bp - '0');
                bp++;
            }
        }
    }

    return static_cast<std::uint32_t>(btime + starttime / clk_tck);
}
#endif

#endif