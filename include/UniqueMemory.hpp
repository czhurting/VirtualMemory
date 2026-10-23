/*
* VirtualMemory
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
* CopyRight by CZHurting(Bilibili:cz欠揍了)
*/

#ifndef VM_UNIQUE_MEMORY_HPP
#define VM_UNIQUE_MEMORY_HPP
#include <cstddef>
#include <atomic>
#include <SystemInfo.hpp>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <algorithm>

#ifdef __linux__
#include <sys/mman.h>
#endif

namespace vm
{
	inline std::size_t PageSize()
	{
		static auto Pagesize=SystemInfo::GetSystemInfo().page_size;
		return Pagesize;
	}

	
	namespace detail
	{
		template<typename T>
		struct Node
		{
			T* data{};
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
		UniqueMemoryLock(void* p, std::size_t size):p(p),size(size)
		{
#ifdef _WIN32
			unlocked = !(VirtualLock(p, size));
#elif defined(__linux__)
			unlocked = (static_cast<bool>(mlock(p, size)));
#endif
		}
		UniqueMemoryLock(UniqueMemoryLock&) = delete;
		UniqueMemoryLock& operator=(UniqueMemoryLock&) = delete;
		UniqueMemoryLock(UniqueMemoryLock&& other) noexcept :p(other.p), size(other.size), unlocked(other.unlocked)
		{
			other.unlocked = true;
			other.p = nullptr;
		}
		UniqueMemoryLock& operator=(UniqueMemoryLock&&other) noexcept
		{
			if (&other == this) return *this;
			Unlock();
			p = other.p;
			size = other.size;
			unlocked = other.unlocked;
			other.unlocked = true;
			other.p = nullptr;
		}
		void Unlock()
		{

			if (!unlocked)
			{
#ifdef _WIN32
				VirtualUnlock(p, size);
#elif defined(__linux__)
				munlock(p, size);
#endif
				unlocked = true;
			}
		}
		std::size_t Size() const noexcept { return size; }
		~UniqueMemoryLock()
		{
			Unlock();
		}
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
		UniqueCommit(std::size_t size, void* hint = nullptr):size(size)
		{
#ifdef _WIN32
			p = VirtualAlloc(hint, size, MEM_COMMIT, PAGE_READWRITE);

			if (p == nullptr) throw std::bad_alloc{};
#elif defined(__linux__)
			p = mmap(hint, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			if (p == MAP_FAILED) throw std::bad_alloc{};
#endif
		}
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
			other.p = nullptr;
			other.size = 0;
			return *this;
		}

		void Destroy()
		{
#ifdef _WIN32
			VirtualFree(p, 0, MEM_RELEASE);
#elif defined(__linux__)
			munmap(p, size);
#endif
			p = nullptr;
		}
		explicit operator bool() { return p; }
		void* Get() noexcept { return p; }
		const void* Get() const noexcept { return p; }
		
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
		std::vector<std::pair<std::size_t, std::size_t>> freelist;//注：freelist中1freelist node=4096 byte
		struct n
		{
			void* p{};
			std::size_t offset{};
		};
	public:
		UniqueVirtual(std::size_t size)
		{
			std::size_t pagecount = (size - 1) / PageSize() + 1;
			this->size = pagecount * PageSize();
			node =
#ifdef _WIN32
				VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
#elif defined(__linux__)
				mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
			freelist.push_back({ 0,pagecount });

		}
		UniqueVirtual(UniqueVirtual&) = delete;
		UniqueVirtual& operator=(UniqueVirtual&) = delete;
		UniqueVirtual(UniqueVirtual&& other) noexcept:freelist(std::move(other.freelist))
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
			freelist = std::move(other.freelist);
			other.node = nullptr;
			return *this;
		}
		void Destroy()
		{
#ifdef _WIN32
			VirtualFree(node, size, MEM_RELEASE);
#elif defined(__linux__)
			munmap(node, size);
#endif
			freelist.clear();
			freelist.reserve(1);
		}
		operator bool() const { return node; }

#ifdef VM_TEST
		// 仅供单元测试观察 free list 内部状态；生产构建（未定义 VM_TEST）下不存在
		const std::vector<std::pair<std::size_t, std::size_t>>& free_list() const { return freelist; }
#endif

		//不保证可用
		template<typename T>
		detail::Node<T> Base(std::size_t offset,std::size_t size)
		{
			assert(size <= this->size && offset <= this->size - size);  // 溢出安全:避免 offset+size 无符号回绕 / this->size-size 下溢
			return { static_cast<T*>(static_cast<char*>(node) + offset) ,offset,size };
		}

		//保证可用，但可能有额外的分配
		template<typename T>
		detail::Node<T> At(std::size_t offset,std::size_t size)
		{
			assert(size <= this->size && offset <= this->size - size);  // 溢出安全:避免 offset+size 无符号回绕 / this->size-size 下溢
			auto no = Commit(offset, size);
			return { no.p ,no.offset,size };
		}

		//Commit(期望offset，所要大小)
		n Commit(std::size_t offset, std::size_t size)
		{
			assert(size <= this->size && offset <= this->size - size);  // 溢出安全:避免 offset+size 无符号回绕 / this->size-size 下溢
			std::size_t pagecount = (size - 1) / PageSize() + 1;
			std::size_t realoffset = 0;
			for (auto& [i, j] : freelist)
			{
				if (j - i < pagecount) continue;
				realoffset = i * PageSize();
				i += pagecount;
				break;
			}
			void* hint = static_cast<void*>(static_cast<char*>(node) + realoffset);
#ifdef _WIN32
			void* p = VirtualAlloc(hint, size, MEM_COMMIT, PAGE_READWRITE);
			if(!p) throw std::bad_alloc{};
#else
			if (mprotect(hint, size, PROT_READ | PROT_WRITE) != 0)
				throw std::bad_alloc{};
			void* p = hint;
#endif
			return { p, realoffset };
		}
		void Decommit(std::size_t offset, std::size_t size)
		{
			assert(size <= this->size && offset <= this->size - size);  // 溢出安全:避免 offset+size 无符号回绕 / this->size-size 下溢
			const std::size_t ps = PageSize();
			const std::size_t dstart = offset / ps;
			const std::size_t dend = (offset + size + ps - 1) / ps;

			std::size_t insert = freelist.size();          // 默认插末尾
			for (std::size_t i = 0; i < freelist.size(); ++i)
			{
				if (dend <= freelist[i].first) {          // 新块整段落在 freelist[i] 之前
					insert = i;
					break;
				}
				if (freelist[i].first < dend && freelist[i].second > dstart)
					throw std::logic_error{ "already decommitted / never committed" };
			}

			freelist.insert(freelist.begin() + insert, { dstart, dend });

			if (insert + 1 < freelist.size() &&
				freelist[insert].second == freelist[insert + 1].first) 
			{
				freelist[insert].second = freelist[insert + 1].second;
				freelist.erase(freelist.begin() + insert + 1);
			}
			if (insert > 0 &&
				freelist[insert - 1].second == freelist[insert].first)    // 左邻居
			{
				freelist[insert - 1].second = freelist[insert].second;
				freelist.erase(freelist.begin() + insert);
			}

			void* addr = static_cast<char*>(node) + dstart * ps;
			std::size_t bytes = (dend - dstart) * ps;
#ifdef _WIN32
			if (!VirtualFree(addr, bytes, MEM_DECOMMIT)) throw std::runtime_error{ "MEM_DECOMMIT failed" };
#else
			if (mprotect(addr, bytes, PROT_NONE) != 0)    throw std::runtime_error{ "mprotect failed" };
#endif
		}

		UniqueMemoryLock MLock() { return { node,size }; }

		~UniqueVirtual()
		{
			if (node) Destroy();
		}
	};
}
#endif