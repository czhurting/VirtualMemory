/*
* Memory.hpp
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
*/

#pragma once
#include <cstddef>
#include <atomic>


namespace vm
{
	/*返回页大小*/
	size_t PageSize();

	/*内存保护权限*/
	enum class Permission : std::uint8_t
	{
		Read = 1 << 0,
		Write = 1 << 1,
		Execute = 1 << 2,
		None = 0
	};
	Permission operator|(Permission lhs, Permission rhs);
	Permission operator&(Permission lhs, Permission rhs);
	

	namespace detail2
	{
		struct Node
		{
			std::size_t size{};
			 std::size_t committed_size{};
		};
	}

	/*内存锁*/
	class UniqueMemoryLock
	{
		detail2::Node* memory;
		bool unlocked = false;
	public:
		UniqueMemoryLock(detail2::Node* p);
		UniqueMemoryLock(UniqueMemoryLock&) = delete;
		UniqueMemoryLock& operator=(UniqueMemoryLock&) = delete;
		UniqueMemoryLock(UniqueMemoryLock&&) noexcept;
		UniqueMemoryLock& operator=(UniqueMemoryLock&&) noexcept;
		void Unlock();
		std::size_t Size() const noexcept;
		~UniqueMemoryLock();
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

		enum class State
		{
			NodeCommit=1,
			ReLeasedCommit,
			None=0
		};

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


	/*
	* UniqueVirtual类
	* 作用：独占虚拟内存
	*/
	class UniqueVirtual
	{
		detail2::Node* base;

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
		size_t Size() const noexcept { return base->size; }
		size_t CommittedSize() const noexcept { return base->committed_size; }
		bool IsFullyCommitted() const noexcept { return base->committed_size == base->size; }
		explicit operator bool() const noexcept { return base != nullptr; }

		//分配
		UniqueCommit Commit(std::size_t size);

		// 确保某段可用（不够就补）
		void EnsureRange(size_t bytes);

		UniqueMemoryLock Lock();
		// 释放所有权
		void* Release() noexcept;
	};
}
