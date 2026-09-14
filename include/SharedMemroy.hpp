#pragma once

#include <atomic>
#include <string_view>
#include <SystemInfo.hpp>
#include <Permission.hpp>
#include <Config.hpp>
#include <stdexcept>
#include <typeinfo>

namespace shm 
{

	inline std::size_t PageSize = System::GetSystemInfo().page_size;

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
			std::atomic<pid_t> owner_pid;
			std::atomic<std::uint64_t> create_time;
			std::atomic<std::size_t> ref_count{ 1 };
			std::atomic<std::size_t> mlock_count{ 0 };
			char name[256];
			std::size_t size{};
			std::atomic<std::size_t> weak_count{ 0 };
			//不知道mutex是否为跨进程是否安全所以等会额外写跨进程锁
			class ProcessMutex
			{
				std::atomic<std::int32_t> futex{};

				void FutexWait(std::int32_t expected);  // 平台相关实现放 .cpp
				void FutexWake(int count);
			public:
				void ReadLock();
				void ReadUnlock();
				void WriteLock();
				void WriteUnlock();
				void ExecLock();
				void ExecUnlock();
			};
			ProcessMutex mutex{};
		};
		struct PageNode
		{
			alignas(4096);
			PageNode() = delete;
			PageNode(PageNode const&) = delete;
			PageNode& operator=(PageNode const&) = delete;
			PageNode(PageNode&&) = delete;
			PageNode& operator=(PageNode&&) = delete;
		};
	}

	enum class OSP
	{
		Windows,
		Linux
	};

	/*共享内存锁*/


	template<OSP os>
	class SharedMemory
	{
		detail::Node* node;
		std::uintptr_t fd;//Windows: HANDLE, Linux: int
		Permission permission;
		SharedMemory(detail::Node* value, Permission permission, std::uintptr_t fd) :node(value), permission(permission), fd(fd)
		{
			if (node == nullptr)
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
			Destroy();
			node = other.node;
			permission = other.permission;
			fd = other.fd;
			other.node = nullptr;
			other.fd = 0;
			return *this;
		}

		void Destroy();
		operator bool() const noexcept { return node != nullptr; }

		friend SharedMemory SharedCreator(std::string_view name, std::size_t size);
		friend SharedMemory SharedObserver(std::string_view name, Permission permission);
		template<typename T>
		T* Write()
		{
			if (permission & Permission::Write)
			{
				return static_cast<T*>(static_cast<detail::PageNode*>(node) + PageSize / sizeof(detail::PageNode);
			}
			throw std::runtime_error("No write permission");
		}

		template<typename T>
		const T* Read() const
		{
			if (permission & Permission::Read)
			{
				return static_cast<const T*>(static_cast<const detail::PageNode*>(node) + PageSize / sizeof(detail::PageNode);
			}
			throw std::runtime_error("No read permission");
		}

		template<typename F, typename... Args>
		decltype(auto) Executed(Args&&... args)
		{
			static_assert(std::is_invocable_v<F&, Args...>,
				"F must be invocable with the given arguments");
			if (permission & Permission::Execute)
			{
				auto* func = static_cast<F*>(static_cast<void*>(
					static_cast<detail::PageNode*>(node) + PageSize / sizeof(detail::PageNode)));
				return (*func)(std::forward<Args>(args)...);
			}
			throw std::runtime_error("No execute permission");
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
#ifdef WIN32
	consteval OSP os_ = OSP::Windows;
#else 
	consteval OSP os_ = OSP::Linux;
#endif
	SharedMemory<os_> SharedCreator(std::string_view name, std::size_t size);
	SharedMemory<os_> SharedObserver(std::string_view name, Permission permission);

	/*
	*  注意：
	* Node产生的内容为单独一页，并且不会暴露给外部。可能会产生不必要的内存浪费。
	*/
}