#ifndef DEMANGLE_HPP
#define DEMANGLE_HPP

#include <string>
#include <execinfo.h>
#include <typeinfo>
#include <ostream>

#ifdef __GNUG__

#include <cxxabi.h>
#include <memory>

__attribute__((unused))
static std::string demangledName(const std::type_info& info)
{
   int status = -4; // some arbitrary value to eliminate the compiler warning

   const char* name = info.name();

   // enable c++11 by passing the flag -std=c++11 to g++
   std::unique_ptr<char, void(*)(void*)> res {
	  abi::__cxa_demangle(name, NULL, 0, &status),
            std::free
   };

   return (0 == status) ? res.get() : name;
}

#else

static std::string demangledName(const std::type_info& info)
{
   return info.name();
}

#endif // __GNUG__

/**
 * @note This function does not require unused attribute
 *       because it calls another function just with that attribute.
 */
template<class T>
inline static std::string demangledName() {
   return demangledName(typeid(T));
}

template<class T>
inline static std::string demangledName(T&&) {
   return demangledName(typeid(T));
}

#endif // DEMANGLE_HPP
