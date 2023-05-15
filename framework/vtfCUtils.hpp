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
#include "vtfZDeletable.hpp"

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
strings		splitString(const std::string& delimitedString, char delimiter = ',');

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

template<template<class, class...> class Ctr, class T, class... Aux>
uint32_t data_byte_length(const Ctr<T, Aux...>& ctr)
{
	return uint32_t(ctr.size() * sizeof(T));
}

template<class X> struct collection_element;
template<template<class,class...> class coll__, class X, class... Y>
struct collection_element<coll__<X, Y...>>
{
	typedef X type;
};
template<class coll__> using collection_element_t = typename collection_element<coll__>::type;

template<template<class, class...> class coll__, class T, class... Aux>
uint32_t elem_byte_length(const coll__<T, Aux...>&)
{
	return uint32_t(sizeof(T));
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

template<class X, class SX = typename std::make_signed<X>::type>
const SX make_signed (const X& x)
{
	return static_cast<SX>(x);
}

template<class X, class SX = typename std::make_signed<X>::type>
SX make_signed (X& x)
{
	return static_cast<SX>(x);
}

template<class X, class UX = typename std::make_unsigned<X>::type>
UX make_unsigned (const X& x)
{
	return static_cast<UX>(x);
}

template<class X, class NCX = add_ref<typename std::remove_const<X>::type>>
NCX remove_const (X& x)
{
	return const_cast<NCX>(x);
}

template<class T, class P = T(*)[1], class R = decltype(std::begin(*std::declval<P>()))>
static auto makeStdBeginEnd(void* p, uint32_t n) -> std::pair<R, R>
{
	auto tmp = std::begin(*P(p));
	auto begin = tmp;
	std::advance(tmp, n);
	return { begin, tmp };
}

template<class ListItem, std::size_t N>
void copy_initializer_list(const std::initializer_list<ListItem>& src, ListItem(&dst)[N])
{
	typename std::initializer_list<ListItem>::size_type j = 0;
	for (auto begin = src.begin(), i = begin; i != src.end() && j < N; ++i, ++j)
	{
		dst[j] = *i;
	}
}

template<class X>
struct ExplicitWrapper
{
	X value;
	explicit ExplicitWrapper() : value() {}
	explicit ExplicitWrapper(const X& x) : value(x) {}
	ExplicitWrapper(const ExplicitWrapper& other) : value(other.value) {}
	operator X& () { return value; }
	operator const X& () const { return value; }
	X& operator()() { return value; }
	const X& operator()() const { return value; }
};
template<class X> ExplicitWrapper<X> makeExplicitWrapper (const X& x)
{
	return ExplicitWrapper<X>(x);
}

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

static const auto string_to_c_str = [](const std::string& s) -> const char* { return s.c_str(); };


template<class R, class... Args>
struct routine_signature
{
	typedef R Result;
	typedef std::tuple<Args...> ArgList;
};
template<class R, class... Args> struct routine_t;
template<class R, class... Args> struct routine_t<R(Args...)>
	: routine_signature<R, Args...> { };

template<class routine_type__, std::size_t at__>
using routine_arg_t = std::tuple_element_t<at__, typename routine_t<routine_type__>::ArgList>;

template<class routine_type__>
using routine_res_t = typename routine_t<routine_type__>::Result;

} // namespace vtf

#endif // __VTF_C_UTILS_HPP_INCLUDED__
