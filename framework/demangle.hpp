#ifndef DEMANGLE_HPP
#define DEMANGLE_HPP

#include <string>
#include <typeinfo>
#include <ostream>

std::string demangledName (const std::type_info& info);

template<class T>
inline static std::string demangledName () {
   return demangledName(typeid(T));
}

template<class T>
inline static std::string demangledName (T&&) {
   return demangledName(typeid(T));
}

#endif // DEMANGLE_HPP
