#include "demangle.hpp"
#include <vector>

#ifdef __GNUG__

#include <execinfo.h>
#include <cxxabi.h>
#include <memory>

__attribute__((unused))
std::string demangledName (const std::type_info& info)
{
	int status = -4; // some arbitrary value to eliminate the compiler warning

	const char* name = info.name();

	// enable c++11 by passing the flag -std=c++11 to g++
	std::unique_ptr<char, void(*)(void*)> res{
	   abi::__cxa_demangle(name, NULL, 0, &status),
			 std::free
	};

	return (0 == status) ? res.get() : name;
}

#elif defined(_MSC_VER)

#include <Windows.h>
#include <DbgHelp.h>

std::string demangledName(const std::type_info& info)
{
	std::string buffer;
	DWORD size = 256;
	buffer.resize(size);
	while (UnDecorateSymbolName(info.name(), buffer.data(), size, UNDNAME_COMPLETE) == 0
		&& GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		size *= 2;
		buffer.resize(size);
	}
	buffer.shrink_to_fit();
	return buffer;
}

#else

std::string demangledName(const std::type_info& info)
{
	return info.name();
}
#endif
