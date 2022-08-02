#ifndef __VTF_C_UTILS_HPP_INCLUDED__
#define __VTF_C_UTILS_HPP_INCLUDED__

#include <map>
#include <set>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <type_traits>
#include <fstream>

#include "vtfFilesystem.hpp"

namespace vtf
{
typedef std::vector<std::string> strings;
typedef std::map<std::string, std::string> string_to_string_map;

// variables must be in the form ${VARIABLE}
std::string	subst_variables (const std::string& templateStr, const vtf::string_to_string_map& variables);

std::string getRealPath (const char* path, bool& status);
std::string	readFile (const std::string& filename);
uint32_t	readFile(const fs::path& path, std::vector<unsigned char>& buffer);
std::string captureSystemCommandResult (const char* cmd, bool& status, const char LF = '\0');

bool		containsString (const std::string& ext, const vtf::strings& list);
bool		containsAllString (const vtf::strings& all, const vtf::strings& range);
uint32_t	removeStrings (const vtf::strings& strs, vtf::strings& list);
strings		mergeStrings (const strings& a, const strings& b);
strings		mergeStringsDistinct (const strings& a, const strings& b);

template<class T, class... U> inline
void emplace(T& t, U&&... u)
{
	new (&t) T(std::forward<U>(u)...);
}

template<class X, class S, template<class, class...> class C, class... Y>
inline X select_if(const C<X, Y...>& c, const X& defaultResult, S&& s)
{
	auto x = std::find_if(c.begin(), c.end(), s);
	return x != c.end() ? *x : defaultResult;
}

template<class key__, class val__>
inline bool mapHasKey (const key__& key, const std::map<key__, val__>& mp)
{
	return mp.find(key) != mp.end();
}

template<class convFrom_, class convTo_> convTo_
concise_convert(const convFrom_&, const convTo_& to)
{
	return static_cast<convTo_>(to);
}

// iterator_if

template<template<class, class...> class Container_, class... X_>
auto iterator_from_index (Container_<X_...>& c, uint32_t index)
{
	auto i = std::next(c.begin(), index);
	if (index >= c.size()) i = c.end();
	return i;
}

template<template<class, class...> class Container_, class... X_>
auto iterator_from_index (const Container_<X_...>& c, uint32_t index)
{
	auto i = std::next(c.begin(), index);
	if (index >= c.size()) i = c.end();
	return i;
}

template<class Key_, class Val_, class... X_>
bool mapHasKey (const std::map<Key_, Val_, X_...> map, const Key_& key)
{
	return map.end() != map.find(key);
}

// externals are defined for types: int, float, std::string, bool (true, false, 0 && !0 logic)
template<class T> T fromText (const std::string& text, const T& defResult, bool& status);

template<class T> bool between (const T& paramValue, const T& paramMin, const T& paramMax);
/*
template<class S, class V, class... Sy, class... Vy> inline
void setFromVector (std::set<S, Sy...>& s, const std::vector<V, Vy...>& vec)
{
	for (auto& v : vec) s.insert(v);
}

template<class S, class V, class Conv, class... Sy, class... Vy> inline
void setFromVector (std::set<S, Sy...>& s, const std::vector<V, Vy...>& vec, Conv&& conv)
{
	for (auto& v : vec) s.insert(conv(v));
}

template<class V, class S, class... Vy, class... Sy> inline
void vectorFromSet (std::vector<V, Vy...>& vec, std::set<S, Sy...>& s)
{
	for (auto& i : s) vec.emplace_back(i);
}

template<class V, class S, class Conv, class... Vy, class... Sy> inline
void vectorFromSet(std::vector<V, Vy...>& vec, std::set<S, Sy...>& s, Conv&& conv)
{
	for (auto& i : s) vec.emplace_back(conv(i));
}

static const auto c_str_compare = [](const char* s1, const char* s2) -> bool
{
	return std::strcmp(s1, s2) < 0;
};
typedef typename std::remove_reference<decltype(c_str_compare)>::type CStrComparer;
*/
static const auto string_to_c_str = [](const std::string& s) -> const char* { return s.c_str(); };

template<class X> struct collection_element
{
	typedef X type;
};
template<template<class,class...> class coll__, class X, class... Y>
struct collection_element<coll__<X, Y...>>
{
	typedef X type;
};
template<class coll__> using collection_element_t = typename collection_element<coll__>::type;

} // namespace vtf

#endif // __VTF_C_UTILS_HPP_INCLUDED__
