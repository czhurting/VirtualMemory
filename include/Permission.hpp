#pragma once

#include <cstdint>

/*内存保护权限*/
enum class Permission : std::uint8_t
{
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	None = 0
};
Permission operator|(Permission lhs, Permission rhs)
{
	return static_cast<Permission>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}
Permission operator&(Permission lhs, Permission rhs)
{
	return static_cast<Permission>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}
