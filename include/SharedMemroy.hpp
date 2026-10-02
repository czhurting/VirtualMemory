/*
* VirtualMemory
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
* CopyRight by CZHurting(Bilibili:cz欠揍了)
*/

#ifndef SHM_VIRTUAL_MEMORY_HPP
#define SHM_VIRTUAL_MEMORY_HPP

#include <atomic>
#include <cstdint>
#include <string_view>
#include <SystemInfo.hpp>
#include <Permission.hpp>
#include <Config.hpp>
#include <stdexcept>
#include <Mapping.hpp>

namespace shm 
{
	inline std::size_t PageSize()
	{
		return SystemInfo::GetSystemInfo().page_size;
	}
	inline int StatePID()
	{
		return SystemInfo::GetPID();
	}

	namespace detail
	{
		/*共享节点*/
		struct Node
		{
			const std::uint64_t magic = 0xDEADBEEF;
			const std::uint64_t version =
				(std::uint64_t(VIRTUAL_MEMORY__MAJOR_VERSION) << 32) |
				(std::uint64_t(VIRTUAL_MEMORY__MINOR_VERSION) << 16) |
				(std::uint64_t(VIRTUAL_MEMORY__PATCH_VERSION));
			struct ProcessInformation
			{
				int pid{};
				int create_time{};
				enum class State :std::uint8_t
				{
					Dead = 0,
					Alive = 1
				};
				State state{ State::Dead };
			};
			ProcessInformation pinfo[64];

			std::atomic<std::size_t> ref_count{ 1 };
			std::atomic<std::size_t> mlock_count{ 0 };
			char name[256];
			std::size_t size{};
			//不知道mutex是否为跨进程是否安全所以等会额外写跨进程锁
			class ProcessMutex
			{
				std::atomic<std::int16_t> read_count{};
				std::atomic<int> writer_pid{ 0 };

				void FutexWait(std::int32_t expected);  // 平台相关实现放 .cpp
				void FutexWake(int count);
			public:
				void ReadLock();
				void ReadUnlock();
				void WriteLock();
				void WriteUnlock();
			};
			ProcessMutex mutex{};
		};
	}

	/*共享内存锁*/


	template<OSP os>
	class SharedMemory
	{
		detail::Node* node;
		Map<os> map;
		Permission permission;
		SharedMemory(detail::Node* value, Permission permission, Map<os> map) :node(value), permission(permission),map(map)
		{
			if (!node)
			{
				throw std::runtime_error("Failed to create or open shared memory");
			}
		}
	public:
		~SharedMemory() noexcept
		{
			Destroy();
		}
		SharedMemory(SharedMemory const&) = delete;
		SharedMemory& operator=(SharedMemory const&) = delete;
		SharedMemory(SharedMemory&& other) noexcept :node(other.node), permission(other.permission), fd(other.fd)
		{
			other.node = nullptr;
			other.fd = 0;

		}
		SharedMemory& operator=(SharedMemory&& other) noexcept
		{
			if (this == &other) return;
			Destroy();
			node = other.node;
			permission = other.permission;
			fd = other.fd;
			other.node = nullptr;
			other.fd = 0;
			return *this;
		}

		void Destroy();
		operator bool() const noexcept { return node; }
		//需要写Magic成员变量
		static SharedMemory Create(std::string_view name, std::size_t size);
		//需要验证magic成员变量
		static SharedMemory Observe(std::string_view name, Permission permission);
		template<typename T>
		requires std::is_pod<T>
		T* Write()
		{
			if (static_cast<bool>(permission & Permission::Write))
			{
				return static_cast<T*>(static_cast<char*>(static_cast<void*>(node)) + PageSize);
			}
			throw std::runtime_error("No write permission");
		}

		template<typename T>
		const T* Read() const
		{
			if (permission & Permission::Read)
			{
				return static_cast<const T*>(static_cast<const char*>(static_cast<void*>(node)) + PageSize);
			}
			throw std::runtime_error("No read permission");
		}
		class SharedMemoryLock
		{
			detail::Node& node;
			bool unlocked = false;
		public:
			SharedMemoryLock(detail::Node& node);
			SharedMemoryLock(SharedMemoryLock&&) noexcept;
			SharedMemoryLock& operator=(SharedMemoryLock&&) noexcept;
			SharedMemoryLock(SharedMemoryLock const&) = delete;
			SharedMemoryLock& operator=(SharedMemoryLock const&) = delete;
			void Lock();
			void Unlock();
			operator bool() const noexcept { return !unlocked; }
			~SharedMemoryLock();
		};

		SharedMemoryLock MLock()
		{
			return { *node };
		}

		std::size_t Size() const noexcept { return node->size; }
	};

	/*
	*  注意：
	* Node产生的内容为单独一页，并且不会暴露给外部。可能会产生不必要的内存浪费。
	*/
}
#endif