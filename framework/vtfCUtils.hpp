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

#define ARRAY_LENGTH(a__) (std::extent<decltype(a__)>::value)
#define ARRAY_LENGTH_CAST(a__, cast__) (static_cast<cast__>(ARRAY_LENGTH(a__)))

#ifndef UNREF
#define UNREF(x__) static_cast<void>(x__)
#endif
#define SIDE_EFFECT(x__) UNREF(x__)

#ifdef _MSC_VER
#define UNUSED [[maybe_unused]]
#else
#define UNUSED __attribute__((unused))
#endif

namespace vtf
{

typedef std::vector<std::string> strings;
typedef std::map<std::string, std::string> string_to_string_map;

struct Boolean { bool value, yesno; };
Boolean boolean (bool value, bool yesno = false);
std::ostream& operator<< (std::ostream& str, const Boolean& value);

// template string variables must be in the form ${VARIABLE}
// map of variables must contain key-value pair in the form { "VARIABLE", "value" }
std::string	subst_variables (const std::string& templateStr, const vtf::string_to_string_map& variables,
							 bool validateVariableExistence = true);

std::string getRealPath (const char* path, bool& status);
// if status is null or points to false then throw on error, otherwise opening status is returned
std::string	readFile (const std::string& filename, bool* status = nullptr);
uint32_t	readFile (add_cref<fs::path> path, add_ref<std::vector<uint32_t>> buffer);
uint32_t	readFile (add_cref<fs::path> path, add_ref<std::vector<uint8_t>> buffer);
std::string captureSystemCommandResult (const char* cmd, bool& status, const char LF = '\0');

bool		containsString (const std::string& s, const vtf::strings& list);
bool		containsAllStrings (const vtf::strings& range, const vtf::strings& all);
bool		containsAnyString (const vtf::strings& range, const vtf::strings& include);
uint32_t	removeStrings (const vtf::strings& strs, vtf::strings& list);
strings		mergeStrings (const strings& a, const strings& b);
strings		mergeStringsDistinct (const strings& a, const strings& b);
strings		splitString (const std::string& delimitedString, char delimiter = ',');

std::string	toLower (add_cref<std::string> s);
std::string	toUpper (add_cref<std::string> s);
void		toLower (add_ref<std::string> inplace);
void		toUpper (add_ref<std::string> inplace);
void		toLower (add_ref<std::string> out, add_cref<std::string> src);
void		toUpper (add_ref<std::string> out, add_cref<std::string> src);
bool		startswith (add_cptr<char> s, char c);
template<class CharType> bool startswith (std::basic_string<CharType> s, CharType c);

// Be careful, source strings must be alive after to_strings() is invoked.
template<template<class T, class... U> class C, class T, class... U>
void to_cstrings (const C<T, U...>& iss, std::vector<const char*>& oss)
{
	oss.resize(iss.size());
	std::transform(iss.begin(), iss.end(), oss.begin(), [](const std::string& s) { return s.data(); });
}
template<template<class T, class... U> class C, class T, class... U>
std::vector<const char*> to_cstrings (const C<T, U...>& iss)
{
	std::vector<const char*> oss;
	to_cstrings(iss, oss);
	return oss;
}

template<class Creator, class... CreatorArgs>
auto createVector (uint32_t count, Creator&& creator, CreatorArgs&&... args)
	-> std::vector<typename std::invoke_result<Creator, CreatorArgs...>::type>
{
	std::vector<typename std::invoke_result<Creator, CreatorArgs...>::type> v;
	for (uint32_t i = 0; i < count; ++i) v.push_back(creator(std::forward<CreatorArgs>(args)...));
	return v;
}

template<class X, class S, template<class, class...> class C, class... Y>
inline X select_if (const C<X, Y...>& c, const X& defaultResult, S&& s)
{
	auto x = std::find_if(c.begin(), c.end(), s);
	return x != c.end() ? *x : defaultResult;
}

template<template<class, class...> class Ctr, class T, class... ImplCppSpecDontCare>
VkDeviceSize data_byte_length (const Ctr<T, ImplCppSpecDontCare...>& ctr)
{
	return static_cast<VkDeviceSize>(ctr.size() * sizeof(T));
}

template<template<class, class...> class Ctr, class T, class... ImplCppSpecDontCare>
uint32_t data_count (const Ctr<T, ImplCppSpecDontCare...>& ctr)
{
	return static_cast<uint32_t>(ctr.size());
}

template<template<class, class...> class Ctr, class T, class... ImplCppSpecDontCare>
add_ptr<T> data_or_null (Ctr<T, ImplCppSpecDontCare...>& ctr)
{
	return ctr.size() ? ctr.data() : nullptr;
}

template<template<class, class...> class Ctr, class T, class... ImplCppSpecDontCare>
add_cptr<T> data_or_null (const Ctr<T, ImplCppSpecDontCare...>& ctr)
{
	return ctr.size() ? ctr.data() : nullptr;
}

template<template<class, class...> class Container, class value_type_t, class value_type, class... ImplCppSpecDontCare>
void container_push_back_more (Container<value_type_t, ImplCppSpecDontCare...>& cntr, std::initializer_list<value_type> values)
{
	for (value_type value : values)
		cntr.push_back(value);
}

template<class X> struct collection_element;
template<template<class, class...> class coll__, class X, class... Y>
struct collection_element<coll__<X, Y...>>
{
	typedef X type;
};
template<class coll__> using collection_element_t = typename collection_element<coll__>::type;


template<template<class, class...> class coll__, class T, class... Aux>
uint32_t elem_byte_length (const coll__<T, Aux...>&)
{
	return uint32_t(sizeof(T));
}

template<class key__, class val__>
inline bool mapHasKey (const key__& key, const std::map<key__, val__>& mp)
{
	return mp.find(key) != mp.end();
}

template<class Key_, class Val_, class... X_>
inline bool mapHasKey (const std::map<Key_, Val_, X_...> map, const Key_& key)
{
	return map.end() != map.find(key);
}

template<class convFrom_, class convTo_> convTo_
concise_convert (const convFrom_&, const convTo_& to)
{
	return static_cast<convTo_>(to);
}

// used in conjunction with std::transform()
// for expressions such as constructor or identity transformation
struct transform_identity
{
	template<class U> auto&& operator ()(U&& u)
	{
		return std::forward<U>(u);
	}
};

template<class X, class SX = typename std::make_signed<X>::type>
constexpr const SX make_signed (X& x)
{
	return static_cast<SX>(x);
}

template<class X, class SX = typename std::make_signed<X>::type>
constexpr SX make_signed (const X& x)
{
	return static_cast<SX>(x);
}

template<class X, class UX = typename std::make_unsigned<X>::type>
constexpr UX make_unsigned (const X& x)
{
	return static_cast<UX>(x);
}

template<class X, class NCX = add_ref<typename std::remove_const<X>::type>>
constexpr NCX remove_const (X& x)
{
	return const_cast<NCX>(x);
}

template<class X> constexpr add_cref<X> add_const_ref (X& x)
{
	return add_cref<X>(x);
}

template<class X> constexpr add_cref<X> add_const_ref (const X& x)
{
	return add_cref<X>(x);
}

template<class T, class N, class P = T(*)[1], class R = decltype(std::begin(*std::declval<P>()))>
static auto makeStdBeginEnd (add_ptr<void> p, N&& n) -> std::pair<R, R>
{
	auto tmp = std::begin(*P(p));
	auto begin = tmp;
	std::advance(tmp, std::forward<N>(n));
	return { begin, tmp };
}

template<class T, class N, class P = const T(*)[1], class R = decltype(std::cbegin(*std::declval<const P>()))>
static auto makeStdBeginEnd(add_cptr<void> p, N&& n) -> std::pair<R, R>
{
	auto tmp = std::cbegin(*P(p));
	auto begin = tmp;
	std::advance(tmp, std::forward<N>(n));
	return { begin, tmp };
}

template<class ListItem, std::size_t N>
void copy_initializer_list (const std::initializer_list<ListItem>& src, ListItem(&dst)[N])
{
	typename std::initializer_list<ListItem>::size_type j = 0;
	for (auto begin = src.begin(), i = begin; i != src.end() && j < N; ++i, ++j)
	{
		dst[j] = *i;
	}
}

#define STRINGIZE(value__) #value__

#define BEGIN_SWITCH_STR(variable_lhs_, variable_rhs_, switch__) {	\
	const char* variable_rhs_ = nullptr;							\
	const char*& variable_rhs_ref_ = variable_rhs_;					\
	const char* variable_lhs_ = (switch__); if (false) {
#define CASE_STR(variable_lhs_, some_str_)	} else					\
	if (variable_rhs_ref_ = (some_str_);							\
		std::strncmp(variable_lhs_, variable_rhs_ref_,				\
		std::strlen(variable_rhs_ref_)) == 0) {
#define CASE_STR_DEFAULT(variable_lhs_) } else {
#define END_SWITCH_STR(variable_lhs_)	\
	} static_cast<void>(variable_lhs_); }

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

// externals are defined for types: int32, uin32, uint64, float, std::string, bool (true, false, 0 && !0 logic)
template<class T> T fromText (const std::string& text, const T& defResult, bool& status);

template<class T> bool between (const T& paramValue, const T& paramMin, const T& paramMax);

static const auto string_to_c_str = [](const std::string& s) -> const char* { return s.c_str(); };


// Primary routine_signature_template
template<class> struct routine_signature;
// routine_template specialization for routine pointers
template<class R, class... Args>
struct routine_signature<R(*)(Args...)>
{
	typedef R Result;
	typedef void Behalf;
	typedef std::tuple<Args...> ArgList;
	typedef R(*Pointer)(Args...);
	constexpr bool is_method() {
		return false;
	}
};
// routine_template specialization for routine reference
template<class R, class... Args>
struct routine_signature<R(Args...)> : routine_signature<R(*)(Args...)>
{
	// All the members are available from the base type
};
// routine_template specialization for method pointers
template<class R, class C, class... Args>
struct routine_signature<R(C::*)(Args...)>
{
	typedef R Result;
	typedef C Behalf;
	typedef std::tuple<Args...> ArgList;
	typedef R(C::* Pointer)(Args...);
	typedef std::remove_pointer_t<Pointer> Reference;
	constexpr bool is_method() {
		return true;
	}
};
// routine_template specialization for const methodpointers
template<class C, class R, class... Args>
struct routine_signature<R(C::*)(Args...) const>
{
	typedef R Result;
	typedef const C Behalf;
	typedef std::tuple<Args...> ArgList;
	typedef R(C::* Pointer)(Args...) const;
	typedef std::remove_pointer_t<Pointer> Reference;
	constexpr bool is_method() {
		return true;
	}
};
template<class routine_type__, std::size_t at__>
using routine_arg_t = std::tuple_element_t<at__, typename routine_signature<routine_type__>::ArgList>;
// Example: typedef routine_arg_t<decltype(std::setw), 0> sted_setw_param0_t;

template<class routine_type__>
using routine_res_t = typename routine_signature<routine_type__>::Result;
// Example: typedef routine_res_t<decltype(std::setw)> sted_setw_result_t;

template<class T, class E> struct expander
{
	/*
	* don't forget abuout promote operator=
	* using expander<ZImageEx, ZImage>::operator=;
	*/
	T* self() { return static_cast<T*>(this); }
	const T* self() const { return static_cast<const T*>(this); }
	bool operator!=(const E& expanded) const {
		return !dynamic_cast<const E*>(self())->operator==(expanded);
	}
	T& operator=(const E& expanded) {
		dynamic_cast<E*>(self())->operator=(expanded);
		return *self();
	}
};

template<class Y> add_cptr<Y> makeQuickPtr(Y&& y)
{
	return &static_cast<add_cref<Y>>(std::forward<Y>(y));
}

template<class Y> add_cref<Y> makeQuickRef(Y&& y)
{
	return static_cast<add_cref<Y>>(std::forward<Y>(y));
}

struct TriLogicInt
{
				TriLogicInt	() : m_value{}, m_hasValue(false) {}
				TriLogicInt (int value) : m_value(value), m_hasValue(true) {}
	// definition of implicit copy assignment operator for 'TriLogicInt'
	// is deprecated because it has a user - provided copy constructor
	// TriLogicInt (add_cref<TriLogicInt> other) = default;
	inline bool	hasValue	() const { return m_hasValue; }
	inline int	value		() const { ASSERTMSG(m_hasValue, "TriLogicInt has no value"); return m_value; }
	inline void	reset()		{ m_hasValue = false;}
	inline bool operator==	(int value) const { return m_hasValue ? (m_value == value) : false; }
private:
	int		m_value;
	bool	m_hasValue;
};

} // namespace vtf

#endif // __VTF_C_UTILS_HPP_INCLUDED__
