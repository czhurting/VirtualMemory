#ifndef MAP
#define MAP

#include <cstdint>
#include <utility>
enum class OSP
{
	Windows,
	Linux
};
template<OSP>
struct Map;
#ifdef _WIN32
#include <Windows.h>
void close(int n) { }
#else
#include <unistd.h>
using HANDLE = void*;
void CloseHandle(HANDLE n) {}
#endif


template<>
struct Map<OSP::Windows>
{
	HANDLE handle;
	Map(HANDLE hand) :handle(hand) {}
	Map(Map&& other):handle(std::move(other.handle))
	{
		other.handle = nullptr;
	}
	Map& operator=(Map&& other)
	{
		if (this == &other) return *this;
		if (handle) CloseHandle(handle);
		handle = other.handle;
		other.handle = nullptr;
		return *this;
	}
	~Map() { if (handle) CloseHandle(handle); }
};

template<>
struct Map<OSP::Linux>
{
	int fd;
	Map(int fd = -1) :fd(fd) {}
	Map(Map&& other) :fd(std::move(other.fd))
	{
		other.fd = -1;
	}
	Map& operator=(Map&& other)
	{
		if (this == &other) return *this;
		if (fd >= 0) close(fd);
		fd = other.fd;
		other.fd = -1;
		return *this;
	}
	~Map() { if (fd >= 0) close(fd); }
};
#endif