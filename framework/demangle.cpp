#include "demangle.hpp"

#ifndef __GNUG__
std::string demangledName(const std::type_info& info)
{
	return info.name();
}
#endif
