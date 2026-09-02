/*
* Memory.hpp
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
*/

#pragma once
#include <cstddef>
#include <span>
#include <concepts>
#include <atomic>


namespace VirtualMemory
{

	inline size_t PageSize();

	enum class Permission : std::uint8_t
	{
		Read = 1 << 2,
		Write = 1 << 1,
		Execute = 1,
		None = 0
	};
	Permission operator|(Permission lhs, Permission rhs);
	Permission operator&(Permission lhs, Permission rhs);
	/*
	* UniqueVirtual类
	* （我不会英语，就这么取名了）
	* 作用：独占虚拟内存
	*/
	class UniqueVirtual
	{
		std::size_t size;
		/*检查是否已分配物理内存*/
		std::size_t is_committed = 0;
		void* base;
		Permission protection = Permission::None;
	public:
		/*仅分配虚拟内存，不分配物理内存，而且分配失败直接炸*/
		explicit UniqueVirtual(std::size_t const size, void* hit_address = nullptr, Permission protection = Permission::None);
		/*禁止拷贝*/
		UniqueVirtual(UniqueVirtual const&) = delete;
		UniqueVirtual& operator=(UniqueVirtual const&) = delete;
		UniqueVirtual(UniqueVirtual&& other) noexcept;
		UniqueVirtual& operator=(UniqueVirtual&& other) noexcept;
		/*返回基础的指针，不保证可访问*/
		void* BaseAddress() const noexcept;
		/*申请物理内存，不足就抛*/
		void AllCommit();
		void PartCommit(std::size_t offset, std::size_t size);
		/*退回物理内存*/
		void AllDecommit();
		void PartDecommit(std::size_t offset, std::size_t size);
		/*检查状态，若不足就提前返回*/
		void EnsureCommitted(std::size_t size);
		/*检查是否分配了物理内存*/
		bool IsCommit() const noexcept;
		/*检查已经分配了多少物理内存*/
		std::size_t CommittedSize() const noexcept;
		/*析构函数*/
		~UniqueVirtual() noexcept;
		/*返回自身大小*/
		std::size_t Size() const noexcept;
		/*提交所有权*/
		void* Release() noexcept;
		/*检查自己是不是死了*/
		explicit operator bool() const noexcept;
		
		char* begin();
		char* end();

		char const* begin() const;
		char const* end() const;
		
		std::byte& operator[](std::size_t);
		const std::byte& operator[](std::size_t) const;

		template<typename T>
		T* At(std::size_t offset = 0)
		{
			if (offset + sizeof(T) > size) throw std::out_of_range{};
			return static_cast<T*>(static_cast<char*>(base) + offset);
		}

		/*适用于std::span，别问我为何只给const，已经有BaseAddress函数了*/
		const void* data() const;

		/*修改权限*/
		void Protect(Permission perm);
		Permission GetProtection() const;
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
		explicit UniqueCommit(std::size_t const size);
		/*禁止拷贝*/
		UniqueCommit(UniqueCommit const&) = delete;
		UniqueCommit& operator=(UniqueCommit const&) = delete;
		UniqueCommit(UniqueCommit&& other) noexcept;
		UniqueCommit& operator=(UniqueCommit&& other) noexcept;
		/*保证返回的指针能用*/
		void* BaseAddress() const noexcept;
		/*大小*/
		std::size_t Size() const noexcept;
		/*析构函数*/
		~UniqueCommit() noexcept;
		/*检查自己死了么*/
		operator bool() const noexcept;
	};


	/*共享节点*/
	struct Node
	{
		std::atomic<std::size_t> ref_count = 1;
		std::atomic<bool> locked = false;
	};

	/*创建者，仅作为创建，可能死亡，最高权限（7）*/
	class SharedCreator
	{
		Node* node;
		std::size_t size;
		char* name;
	public:
		SharedCreator(std::size_t size, char* name = "");
		SharedCreator(SharedCreator const&) = delete;
		SharedCreator& operator=(SharedCreator const&) = delete;
		SharedCreator(SharedCreator&& other) noexcept;
		SharedCreator& operator=(SharedCreator&& other) noexcept;

		template<typename PrintIn>
		void PrintName(PrintIn&&) const;

		char* Name() const noexcept;
		/*对齐锁*/
		std::size_t Size() const noexcept;
		void* BaseAddress() const noexcept;
		~SharedCreator();
	};

	/*带权限的观察者，不知道创建者是否死亡，只知道当前引用计数是否归零，如果归零则内存归还*/
	class SharedObserver
	{
		Node* node;
		std::size_t size;
		char* name;
		Permission permissions;
	public:
		SharedObserver(std::size_t size, Permission permissions, char* name);
		SharedObserver(SharedObserver const&) = delete;
		SharedObserver& operator=(SharedObserver const&) = delete;
		SharedObserver(SharedObserver&& other) = delete;
		SharedObserver& operator=(SharedObserver&& other) = delete;

		std::size_t Size() const noexcept;
		void* BaseAddress() const noexcept;
		
		void ResetPermissions(Permission permissions);

		~SharedObserver();
	};

	/*内存锁*/
	class MemoryLock
	{
		std::size_t size;
		void* memory;
		bool unlocked = false;
	public:
		MemoryLock(void* p, std::size_t size);
		template<typename T>
			requires requires(T t) {
				{ t.BaseAddress() } -> std::same_as<void*>;
				{ t.Size() } -> std::same_as<std::size_t>;
		}
		MemoryLock(T&& memory);
		MemoryLock(MemoryLock&) = delete;
		MemoryLock& operator=(MemoryLock&) = delete;
		MemoryLock(MemoryLock&&) = delete;
		MemoryLock& operator=(MemoryLock&&) = delete;
		void Unlock();
		std::size_t Size() const noexcept;
		~MemoryLock();
	};
}
