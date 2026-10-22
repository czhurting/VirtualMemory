/*
* VirtualMemory
* 主要为虚拟内存封装
* 并非智能指针
* 若想用智能指针请使用<memory>库或自研
* CopyRight by CZHurting(Bilibili:cz欠揍了)
*/

#ifndef VM_SHM_VIRTUAL_MEMOR_HPP
#define VM_SHAREDMEMORY_HPP

#include <atomic>
#include <cstdint>
#include <string_view>
#include <SystemInfo.hpp>
#include <Permission.hpp>
#include <Config.hpp>
#include <stdexcept>
#include <Mapping.hpp>
#include <stdexcept>
#include <iostream>
#include <Time.hpp>

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
			/*8*2*/
			const std::uint64_t magic = 0xDEADBEEF;
			const std::uint64_t version =
				(std::uint64_t(VIRTUAL_MEMORY__MAJOR_VERSION) << 32) |
				(std::uint64_t(VIRTUAL_MEMORY__MINOR_VERSION) << 16) |
				(std::uint64_t(VIRTUAL_MEMORY__PATCH_VERSION));
			struct ProcessInformation
			{
				std::uint32_t pid{};
				std::uint32_t create_time_high{};
				std::uint32_t create_time_low{};
				enum class State :std::uint8_t
				{
					Dead = 0,
					Alive = 1
				};
				State state{ State::Dead };
			};
			/* 16*128 */
			ProcessInformation pinfo[128];

			//8*2
			std::atomic<std::size_t> ref_count{ 1 };
			std::atomic<std::size_t> mlock_count{ 0 };
			//1*256
			char name[256];
			//8
			std::size_t size{};
			//不知道mutex是否为跨进程是否安全所以等会额外写跨进程锁
			class ProcessMutex
			{
				std::atomic<std::int16_t> read_count{};
				std::atomic<int> writer_pid{ 0 };

				void FutexWait(std::int32_t expected); 
				void FutexWake(int count);
			public:
				void ReadLock();
				void ReadUnlock();
				void WriteLock();
				void WriteUnlock();
			};
			//8
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
		SharedMemory(SharedMemory&& other) noexcept :node(other.node), permission(other.permission), map(std::move(other.map))
		{
			other.node = nullptr;

		}
		SharedMemory& operator=(SharedMemory&& other) noexcept
		{
			if (this == &other) return *this;
			Destroy();
			node = other.node;
			permission = other.permission;
			map = other.map;
			other.node = nullptr;
			other.map = 0;
			return *this;
		}

		void Destroy();
		operator bool() const noexcept { return node; }
		//需要写Magic成员变量
		static SharedMemory Create(std::string_view name, std::size_t size)
		{
			std::size_t count = ((size - 1) / Pagesize() + 2) * PageSize();
			if constexpr (os == OSP::Windows)
			{
				HANDLE hMap = OpenFileMappingW(
					FILE_MAP_READ,   // 想要的访问权限
					FALSE,           // 不继承句柄
					name             // 映射名称
				);
				DWORD err = GetLastError();
				if (hMap == nullptr) {
					if (err == ERROR_FILE_NOT_FOUND) 
					{
						HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
							PAGE_EXECUTE_READWRITE, 0, count, name);
						if (h == nullptr)//讲真的，为何不能用nullptr代替，我都写c++了
						{
							throw std::system_error{ name + " can not be create" };
						}
						Permission per=(Permission::Read|Permission::Write|Permission::Execute);
						void* ptr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, count);
						if (!ptr)
						{
							CloseHandle(h);
							throw std::runtime_error("MapViewOfFile failed");
						}
						new (ptr) detail::Node{};
						auto n = static_cast<detail::Node*>(ptr);
						strncpy(n->name, name.data(), 255);
						n->size = count;
						auto& p = n->pinfo[0];
						p.pid = StatePID();
						p.create_time = get_process_create_time();
						p.state = detail::Node::ProcessInformation::State::Alive;
						Map<os> map{ h };
						return SharedMemory(n, per,h);
					}
					throw std::runtime_error("OpenFileMapping failed, err=" + std::to_string(err));
				}
				std::cerr << name << "is created";
				return Observe(name, Permission::Read);
			}
			else if constexpr (os == OSP::Linux)
			{
				if (name.empty())
					throw std::invalid_argument("empty shm name");

				// string_view 不可变、不保证结尾，先规范化成 std::string
				std::string full;
				full.reserve(name.size() + 1);
				if (name.front() != '/')
					full.push_back('/');
				full.append(name.data(), name.size());

				std::string_view body(full.data() + 1, full.size() - 1);
				if (body.empty()
					|| body.find('/') != std::string_view::npos
					|| body == "." || body == "..")
					throw std::invalid_argument("invalid shm name: " + full);

				int fd = shm_open(body.data(), O_CREAT | O_EXCL | O_RDWR, 0666);
				if (fd < 0)
				{
					if (errno == EEXIST)
					{
						report_warning(std::string("共享内存 '") + name.data() + "' 已存在");
						return Observe(name, Permission::Read);
					}
					throw std::runtime_error("shm_open failed");
				}

				if (ftruncate(fd, count) < 0)
				{
					close(fd);
					shm_unlink(name.data());
					throw std::runtime_error("ftruncate failed");
				}

				void* ptr = mmap(nullptr, count, PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_SHARED, fd, 0);
				if (ptr == MAP_FAILED)
				{
					close(fd);
					shm_unlink(name.data());
					throw std::runtime_error("mmap failed");
				}

				// 构造 Node
				auto n = new (ptr) detail::Node{};
				strncpy(n->name, name.data(), 255);
				n->name[255] = '\0';
				n->size = count;
				n->ref_count.store(1);

				auto& p = n->pinfo[0];
				p.pid = StatePID();
				p.create_time = get_process_create_time();
				p.state = detail::Node::ProcessInformation::State::Alive;

				// fd 交给 Map 管理，mmap 映射不依赖 fd 存活
				return SharedMemory(n, Permission::Read | Permission::Write | Permission::Execute, Map<os>{fd});
			}
		}
		//需要验证magic成员变量
		static SharedMemory Observe(std::string_view name, Permission permission)
		{
			int per = static_cast<std::uint8_t>(permission);

			if constexpr (os == OSP::Windows)
			{
				HANDLE hMap = OpenFileMappingW(
					FILE_MAP_ALL_ACCESS,
					FALSE,
					std::wstring(name.begin(), name.end()).c_str()
				);

				if (hMap == nullptr)
				{
					if (err == ERROR_FILE_NOT_FOUND)
					{
						std::cerr << name << "is not created";
						return Create(name, PageSize());
					}
					throw std::runtime_error("OpenFileMapping failed: shared memory not found");
				}

				void* ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
				if (!ptr)
				{
					CloseHandle(hMap);
					throw std::runtime_error("MapViewOfFile failed");
				}

				auto n = static_cast<detail::Node*>(ptr);

				if (n->magic != 0xDEADBEEF)
					throw std::runtime_error("Magic mismatch: not a valid SharedMemory");

				std::uint64_t expected_version =
					(std::uint64_t(VIRTUAL_MEMORY__MAJOR_VERSION) << 32) |
					(std::uint64_t(VIRTUAL_MEMORY__MINOR_VERSION) << 16) |
					(std::uint64_t(VIRTUAL_MEMORY__PATCH_VERSION));

				if ((n->version >> 32) != (expected_version >> 32))
					throw std::runtime_error("Major version mismatch");

				n->ref_count.fetch_add(1, std::memory_order_acq_rel);

				int index = -1;
				bool is_done = false;

				// ★ 注册自己
				for (int i = 0;i < 128;i++)
				{
					if (n->pinfo[i] == detail::Node::ProcessInformation::State::Dead)
					{
						if(!is_done)
						{
							is_done = true;
							index = i;
						}
						continue;
					}
					std::uint32_t p = n->pinfo[i].pid;
					HANDLE hProcess = OpenProcess(PROCESS_SYNCHRONIZE, FALSE, p);
					if (!hProcess)
					{
						// OpenProcess 失败 → 进程已经不在了
						n->pinfo[i].state = detail::Node::ProcessInformation::State::Dead;
						if (!is_done)
						{
							is_done = true;
							index = i;
						}
						continue;
					}

					// 1. 检测是否退出
					DWORD wr = WaitForSingleObject(hProcess, 0);
					if (wr == WAIT_OBJECT_0)
					{
						n->pinfo[i].state = Dead;

						if (!is_done)
						{
							is_done = true;
							index = i;
						}
						CloseHandle(hProcess);
						continue;
					}

					// 2. 拿 creation time（防 PID 复用）
					FILETIME ftCreate{};
					GetProcessTimes(hProcess, &ftCreate, nullptr, nullptr, nullptr);
					std::uint64_t creation =
						(std::uint64_t(ftCreate.dwHighDateTime) << 32) | ftCreate.dwLowDateTime;

					if (creation != n->pinfo[i].create_time)
					{
						// PID 被回收了，不是原来那个进程
						n->pinfo[i].state = Dead;
						if (!is_done)
						{
							is_done = true;
							index = i;
						}
						CloseHandle(hProcess);
						continue;
					}

					CloseHandle(hProcess);
				}

				// hMap 交给 Map 管，映射靠 ptr 活着
				return SharedMemory(n, perm, Map<os>{hMap});
			}
			
		}
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