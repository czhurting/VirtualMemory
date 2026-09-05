/*
* Memory.hpp
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
*/

#pragma once
#include <cstddef>
#include <string_view>
#include <atomic>


namespace VirtualMemory
{
	/*返回页大小*/
	size_t PageSize();

	/*内存保护权限*/
	enum class Permission : std::uint8_t
	{
		Read = 1 << 2,
		Write = 1 << 1,
		Execute = 1,
		None = 0
	};
	Permission operator|(Permission lhs, Permission rhs);
	Permission operator&(Permission lhs, Permission rhs);
	



	/*内存锁*/
	class UniqueMemoryLock
	{
		std::size_t size;
		void* memory;
		bool unlocked = false;
	public:
		UniqueMemoryLock(void* p, std::size_t size);
		UniqueMemoryLock(UniqueMemoryLock&) = delete;
		UniqueMemoryLock& operator=(UniqueMemoryLock&) = delete;
		UniqueMemoryLock(UniqueMemoryLock&&) noexcept;
		UniqueMemoryLock& operator=(UniqueMemoryLock&&) noexcept;
		void Unlock();
		std::size_t Size() const noexcept;
		~UniqueMemoryLock();
	};

	/*
	* UniqueVirtual类
	* 作用：独占虚拟内存
	*/
	class UniqueVirtual
	{
    	void* base = nullptr;
    	size_t total_size = 0;
    	size_t committed_size = 0;

	public:
    	explicit UniqueVirtual(size_t size, void* hint = nullptr);
    	~UniqueVirtual() noexcept;

    	// 移动
    	UniqueVirtual(UniqueVirtual&&) noexcept;
    	UniqueVirtual& operator=(UniqueVirtual&&) noexcept;	

    	// 禁止拷贝
    	UniqueVirtual(UniqueVirtual const&) = delete;
    	UniqueVirtual& operator=(UniqueVirtual const&) = delete;

    	// 查询
    	void* Data() const noexcept { return base; }
    	size_t Size() const noexcept { return total_size; }
    	size_t CommittedSize() const noexcept { return committed_size; }
    	bool IsFullyCommitted() const noexcept { return committed_size == total_size; }
    	explicit operator bool() const noexcept { return base != nullptr; }

    	// 提交/退回
    	void CommitAll();                    // 全部提交，失败抛std::bad_alloc
    	void CommitRange(size_t offset, size_t bytes);  // 部分提交
    	void DecommitAll();                  // 全部退回
    	void DecommitRange(size_t offset, size_t bytes); // 部分退回

    	// 确保某段可用（不够就补）
    	void EnsureRange(size_t offset, size_t bytes);

		UniqueMemoryLock Lock();
    	// 释放所有权
    	void* Release() noexcept;	
	};


	/*
	* UniqueCommit类
	* 作用：申请物理内存
	*/
	class UniqueCommit
	{
		std::size_t size;
		void* data;
	public:
		/*分配物理内存，若失败抛std::bad_alloc*/
		explicit UniqueCommit(std::size_t const size, void* hint = nullptr);
		/*禁止拷贝*/
		UniqueCommit(UniqueCommit const&) = delete;
		UniqueCommit& operator=(UniqueCommit const&) = delete;
		UniqueCommit(UniqueCommit&& other) noexcept;
		UniqueCommit& operator=(UniqueCommit&& other) noexcept;
		/*保证返回的指针能用*/
		void* Data() const noexcept { return data; }
		/*大小*/
		std::size_t Size() const noexcept { return size; }
		/*析构函数*/
		~UniqueCommit() noexcept;
		/*检查自己死了么*/
		operator bool() const noexcept { return data != nullptr; }
	};

	namespace detail
	{
		/*共享节点*/
		struct Node
		{
			const std::uint64_t magic = 0xDEADBEEF;
			std::atomic<std::size_t> ref_count{ 1 };
			std::atomic<std::size_t> mlock_count{ 0 };
			char name[256];
			std::size_t size{};
			std::atomic<std::size_t> weak_count{ 0 };

		};
	}

	/*共享内存锁*/
	class SharedMemoryLock
	{
		detail::Node* node;
		bool unlocked = false;
	public:
		SharedMemoryLock(detail::Node* node);
		SharedMemoryLock(SharedMemoryLock&&) noexcept;
		SharedMemoryLock& operator=(SharedMemoryLock&&) noexcept;
		SharedMemoryLock(SharedMemoryLock const&) = delete;
		SharedMemoryLock& operator=(SharedMemoryLock const&) = delete;
		void Lock();
		void Unlock();
		operator bool() const noexcept { return !unlocked; }
		~SharedMemoryLock();
	};
	
	/*带权限的观察者，不知道创建者是否死亡，只知道当前引用计数是否归零，如果归零则内存归还*/
	class SharedTempObserver
	{
		detail::Node* node;
		Permission permissions;
		bool page_set = true;
	public:
		SharedTempObserver(Permission permissions, char name[256]);
		SharedTempObserver(SharedTempObserver const&) = delete;
		SharedTempObserver& operator=(SharedTempObserver const&) = delete;
		SharedTempObserver(SharedTempObserver&& other) noexcept;
		SharedTempObserver& operator=(SharedTempObserver&& other) noexcept;

		std::size_t Size() const noexcept;
		void* Data() const noexcept;
		std::string_view Name() const noexcept;
		bool CanWrite() const noexcept;
		bool CanRead() const noexcept;
		bool CanExecute() const noexcept;

		void ChangeVisitMethod(bool page_set) const noexcept;

		SharedMemoryLock Lock();

		/*重置权限*/
		void ResetPermissions(Permission permissions);

		~SharedTempObserver();
	};

	SharedTempObserver TempCreator(std::size_t size, std::string_view name);//默认RWX权限
	SharedTempObserver TempObserver(Permission permissions, std::string_view name);//设置权限

	class SharedPermObserver
	{
		detail::Node* node;
		Permission permissions;
		bool page_set = true;
	public:
		SharedPermObserver(Permission permissions, char name[256]);
		SharedPermObserver(SharedPermObserver const&) = delete;
		SharedPermObserver& operator=(SharedPermObserver const&) = delete;
		SharedPermObserver(SharedPermObserver&& other) noexcept;
		SharedPermObserver& operator=(SharedPermObserver&& other) noexcept;

		std::size_t Size() const noexcept;

		void* Data() const noexcept;
		std::string_view Name() const noexcept;

		bool CanWrite() const noexcept;
		bool CanRead() const noexcept;
		bool CanExecute() const noexcept;

		void ChangeVisitMethod(bool page_set) const noexcept;
		SharedMemoryLock Lock();
		/*重置权限*/
		void ResetPermissions(Permission permissions);



		~SharedPermObserver();
	};
	SharedPermObserver PermCreator(std::size_t size, std::string_view name);//默认RWX权限
	SharedPermObserver PermObserver(Permission permissions, std::string_view name);//设置权限
	/*
	*  注意：
	* Node产生的内容为单独一页，并且不会暴露给外部。可能会产生不必要的内存浪费。
	*/
}
