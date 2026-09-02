# VirtualMemory

跨平台虚拟内存封装库，提供独占虚拟内存管理、物理内存提交/回收、以及基于引用计数的共享内存机制。

*特性：

 *UniqueVirtual：独占虚拟地址空间管理，支持延迟提交物理内存
 *UniqueCommit：物理内存分配封装
 *SharedCreator / SharedObserver：基于侵入式引用计数的跨进程共享内存，控制块页对齐隔离避免 false sharing
 *MemoryLock：共享内存区域锁
 *权限控制（Read / Write / Execute）
