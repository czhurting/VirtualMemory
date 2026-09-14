#pragma once

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
