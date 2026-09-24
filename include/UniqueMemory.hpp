/*
* VirtualMemory
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
* CopyRight by CZHurting(Bilibili:cz欠揍了)
*/

#ifndef UniqueMemory
#define UniqueMemory
#include <cstddef>
#include <atomic>
#include <SystemInfo.hpp>
#include <cassert>
#include <vector>
#include <array>


namespace vm
{
	inline size_t PageSize = SystemInfo::GetSystemInfo().page_size;

	namespace temp
	{
		template<typename T>
		struct Node
		{
			T* p{};
			std::size_t offset{};
			std::size_t size{};
		};
	}

	/*内存锁*/
	class UniqueMemoryLock
	{
		void* p;
		std::size_t size;
		bool unlocked = false;
	public:
		UniqueMemoryLock(void* p, std::size_t size);
		UniqueMemoryLock(UniqueMemoryLock&) = delete;
		UniqueMemoryLock& operator=(UniqueMemoryLock&) = delete;
		UniqueMemoryLock(UniqueMemoryLock&&) noexcept;
		UniqueMemoryLock& operator=(UniqueMemoryLock&&) noexcept;
		void Unlock();
		std::size_t Size() const noexcept;
		explicit operator bool() { return !unlocked; }
		~UniqueMemoryLock();
	};


	/*
	* UniqueCommit类
	* 作用：申请物理内存
	*/
	class UniqueCommit
	{
		void* p;
		std::size_t size;
	public:
		UniqueCommit(std::size_t size, void* hint = nullptr);
		UniqueCommit(UniqueCommit&) = delete;
		UniqueCommit& operator=(UniqueCommit&) = delete;
		UniqueCommit(UniqueCommit&& other) noexcept
		{
			p = other.p;
			size = other.size;
			other.p = nullptr;
			other.size = 0;
		}
		UniqueCommit& operator=(UniqueCommit&& other) noexcept
		{
			if (this == &other) return *this;
			if (p) Destroy();
			p = other.p;
			size = other.size;
			return *this;
		}

		void Destroy();
		operator bool() { return p; }
		void* Get()
		{
			return p;
		}
		const void* Get()const { return p; }
		
		UniqueMemoryLock MLock() { return { p,size }; }

		~UniqueCommit()
		{
			if (p) Destroy();
		}

	};


	/*
	* UniqueVirtual类
	* 作用：独占虚拟内存
	*/
	class UniqueVirtual
	{
		void* node;
		std::size_t size;
		std::vector<std::array<int, 2>> freelist = std::vector<std::array<int, 2>>(1);
	public:
		/*受TLB影响，size的实际值可能并非参数而是size/页大小*页大小向上取整，这点可以通过调用Size函数看到实际的值。注：不同系统对应的页大小不同*/
		UniqueVirtual(std::size_t size);
		UniqueVirtual(UniqueVirtual&) = delete;
		UniqueVirtual& operator=(UniqueVirtual&) = delete;
		UniqueVirtual(UniqueVirtual&& other) noexcept
		{
			node = other.node;
			size = other.size;
			other.node = nullptr;
		}
		UniqueVirtual& operator=(UniqueVirtual&& other) noexcept
		{
			if (this == &other) return *this;
			if (node) Destroy();
			node = other.node;
			size = other.size;
			other.node = nullptr;
			return *this;
		}
		void Destroy();
		explicit operator bool() const { return node; }
		std::size_t Size() { return size; }

		//不保证可用
		template<typename T>
		temp::Node<T> Base(std::size_t offset,std::size_t size)
		{
			assert(offset + size <= this->size - sizeof(T));
			return { static_cast<T*>(static_cast<char*>(node) + offset),offset,size };
		}

		//保证可用，但可能有额外的分配
		template<typename T>
		temp::Node<T> At(std::size_t offset,std::size_t size)
		{
			assert(offset + size <= this->size - sizeof(T));
			return { static_cast<T*>(Commit(offset,size)),offset,size };
		}

		void* Commit(std::size_t offset,std::size_t size);
		void Decommit(std::size_t offset,std::size_t size);

		UniqueMemoryLock MLock() { return { node,size }; }

		~UniqueVirtual()
		{
			if (node) Destroy();
		}
	};
}
#endif