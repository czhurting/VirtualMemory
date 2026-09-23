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


namespace vm
{
	inline size_t PageSize = SystemInfo::GetSystemInfo().page_size;
	

	enum class State:uint8_t
	{
		COMMIT = 1,
		RESERVE = 2,
		GUARD = 4
	};

	namespace detail
	{
		struct alignas(4096) Node
		{
			std::size_t size{};
			std::byte bitmap[4096 - sizeof(std::size_t)];
		};
	}

	/*内存锁*/
	class UniqueMemoryLock
	{
		void* p;
		std::size_t size;
		bool unlocked = false;
	public:
		UniqueMemoryLock(detail::Node* p);
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
		detail::Node* node;
		std::size_t page_count;
	public:
		UniqueVirtual(std::size_t size);
		UniqueVirtual(UniqueVirtual&) = delete;
		UniqueVirtual& operator=(UniqueVirtual&) = delete;
		UniqueVirtual(UniqueVirtual&& other) noexcept
		{
			node = other.node;
			page_count = other.page_count;
			other.node = nullptr;
		}
		UniqueVirtual& operator=(UniqueVirtual&& other) noexcept
		{
			if (this == &other) return *this;
			if (node) Destroy();
			node = other.node;
			page_count = other.page_count;
			other.node = nullptr;
			return *this;
		}
		void Destroy();
		operator bool() const { return node; }

		//不保证可用
		template<typename T>
		T* Base(std::size_t offset)
		{
			assert(offset <= node->size - sizeof(T));
			return static_cast<T*>(reinterpret_cast<std::byte*>(node) + PageSize + offset);
		}

		//保证可用，但可能有额外的分配
		template<typename T>
		T* At(std::size_t offset)
		{
			assert(offset <= node->size - sizeof(T));
			return Commit(offset);
		}

		void* Commit(std::size_t offset);
		void Decommit(void* p);//需要保证p在这个内存中，否则可能失败

		UniqueMemoryLock MLock() { return { node }; }

		~UniqueVirtual()
		{
			if (node) Destroy();
		}
	};
}
#endif