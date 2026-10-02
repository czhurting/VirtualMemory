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
#else
#include <unistd.h>
#endif


template<>
struct Map<OSP::Windows>
{
	HANDLE handle;
	Map(HANDLE hand) :handle(hand) {}
};

template<>
struct Map<OSP::Linux>
{
	int fd;
	Map(int fd) :fd(fd) {}
};
#endif