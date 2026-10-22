# VirtualMemory

跨平台虚拟内存封装库（C++20）。**非智能指针**，偏向底层内存控制：
独占虚拟地址空间管理、物理内存的提交 / 回收 / 页锁定，以及跨进程共享内存框架。

> ⚠️ 项目仍在活跃开发中，部分模块为 WIP（见下方「开发状态」）。

---

## 特性

### `vm` 命名空间 —— 底层虚拟内存原语（`UniqueMemory.hpp`）
- `UniqueMemoryLock`：物理页锁定（Linux `mlock` / Windows `VirtualLock`）的 RAII 封装，析构自动解锁。
- `UniqueCommit`：物理内存分配与释放（Linux `mmap`+`mprotect` / Windows `VirtualAlloc`）+ 一键页锁定。
- `UniqueVirtual`：独占虚拟地址空间，支持**延迟提交**——`reserve` 大块虚拟地址，按需 `commit` / `decommit` 子区间。
- `vm::detail::Node<T>`：带偏移与长度的区间视图包装（`data` / `offset` / `size`）。

### `shm` 命名空间 —— 跨进程共享内存（`SharedMemory.hpp`，WIP）
- `SharedMemory`：基于控制块的共享内存段管理。
- `SharedMemoryLock`：共享内存区域锁。
- 控制块 `detail::Node` 含：`magic`、语义版本号 `version`、`ProcessInformation[128]`（进程存活探测）、`ref_count`、`mlock_count`。
- `ProcessMutex`：基于 futex 的跨进程读写锁（进行中）。

### 辅助模块（`include/`）
- `SystemInfo`：跨平台页大小、CPU 数获取（`GetSystemInfo` / `GetPID`）。
- `Permission`：内存保护权限枚举 `Read` / `Write` / `Execute` / `None`，并支持 `|` / `&` 位运算。
- `Mapping`：`Map<OSP>` 平台句柄 RAII（Windows `HANDLE` / Linux `fd`）。
- `Config`：库版本号常量（`VIRTUAL_MEMORY__*_VERSION`）。
- `Time`：`get_process_create_time()` 跨平台进程创建时间，供存活探测使用（WIP）。

---

## 目录结构

```
VirtualMemory/
├── CMakeLists.txt
├── LICENSE
├── include/
│   ├── UniqueMemory.hpp    # vm：页锁 / 物理提交 / 延迟提交虚拟空间
│   ├── SharedMemory.hpp    # shm：跨进程共享内存（WIP）
│   ├── SystemInfo.hpp      # 页大小 / CPU 数
│   ├── Permission.hpp      # 权限枚举
│   ├── Mapping.hpp         # 平台句柄 RAII
│   ├── Config.hpp          # 版本号常量
│   └── Time.hpp            # 进程创建时间（WIP）
├── src/
│   ├── UniqueMemoryForWindows.cpp
│   └── UniqueMemoryForLinux.cpp
└── test/
    └── size.cpp            # 打印 shm::detail::Node 的大小
```

---

## 构建

要求：CMake ≥ 3.10，C++20 编译器（MSVC / g++ / clang++）。

```bash
git clone https://github.com/czhurting/VirtualMemory.git
cd VirtualMemory
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

构建产物：
- `VirtualMemory`：SHARED 动态库（按平台编译 `src/UniqueMemoryFor*.cpp`）。
- `test_execute`：测试程序，打印 `shm::detail::Node` 的字节大小。

> 注：当前实现多为 header-only（逻辑在头文件内联），`src/` 下的平台源文件为预留编译单元。

---

## 快速示例

### 分配物理内存并页锁定

```cpp
#include <UniqueMemory.hpp>

void example_lock() {
    vm::UniqueCommit mem(4096 * 4);          // 分配 4 页物理内存
    vm::UniqueMemoryLock lock = mem.MLock();  // 锁定，析构时自动解锁
    // 使用 mem.Get() 访问内存……
}
```

### 延迟提交虚拟地址空间

```cpp
#include <UniqueMemory.hpp>

void example_virtual() {
    vm::UniqueVirtual vmem(1024 * 1024);     // 预留 1MB 虚拟地址（暂不占物理内存）
    auto node = vmem.At<char>(0, 4096);      // 按需提交第 0 页
    // 使用 node.data 访问……
    vmem.Decommit(0, 4096);                  // 回收物理页，保留虚拟地址
}
```

> 示例为用法示意，具体接口（成员名、模板参数）请以头文件注释为准。

---

## 开发状态

| 模块 | 状态 | 说明 |
|------|------|------|
| `vm`（UniqueMemoryLock / UniqueCommit / UniqueVirtual） | ✅ 可用 | 已修复若干边界 bug，建议自测后再用于生产 |
| `SystemInfo` / `Permission` / `Mapping` / `Config` | ✅ 稳定 | 小而完整 |
| `shm`（SharedMemory / SharedMemoryLock / ProcessMutex） | 🚧 WIP | 跨进程锁、进程存活探测尚未完成；Windows 端 futex 等价物未实现 |
| `Time`（get_process_create_time） | 🚧 WIP | Linux 分支存在未声明变量，暂不可编译 |

---

## 已知问题 / 待办

- [ ] `SharedMemory`：跨进程 `ProcessMutex` 仅 Linux（futex），Windows 端待实现（如 `SRWLOCK` / `ConditionVariable`）。
- [ ] `Time.hpp`：Linux 分支 `bp` 变量未声明，编译失败，待修复。
- [ ] `SharedMemory.hpp`：头文件保护宏 `#ifndef` 与 `#define` 不一致（`VM_SHM_VIRTUAL_MEMOR_HPP` vs `VM_SHAREDMEMORY_HPP`），存在潜在重复包含风险。
- [ ] `Config.hpp`：全局 `using pid_t = int;` 与 Linux `<sys/types.h>` 冲突，待移入命名空间。
- [ ] `Mapping.hpp`：头文件保护宏过泛（`#ifndef MAP`），与 `<sys/mman.h>` 的 `MAP_*` 系易冲突。
- [ ] 平台源文件 `src/UniqueMemoryFor*.cpp` 目前为空壳，实现仍在头文件内联。

---

## 许可证

见 [LICENSE](LICENSE)。

---

CopyRight by CZHurting（Bilibili: cz欠揍了）
