#ifndef __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
#define __VTF_TEMPLATE_UTILS_HPP_INCLUDED__

#include "vtfVector.hpp"
#include <array>

namespace vtf
{

template<class T_In, class T_Out>
void transformDistance (T_In inMin, T_In inMax, T_In in, T_Out outMin, T_Out outMax, add_ref<T_Out> out, bool mirror)
{
	T_In lsrc = std::abs(make_signed(inMax) - make_signed(inMin));
	T_Out ldst = T_Out(std::abs(make_signed(outMax) - make_signed(outMin)));
	T_In sdist = std::abs(make_signed(in) - make_signed(inMin));
	T_Out ddist = T_Out((double(sdist) * ldst) / lsrc);
	out = mirror ? (outMax - ddist) : (outMin + ddist);
}

template<class X, size_t N>
VecX<X, N> barycenter (
	const VecX<X, N>& a,
	const VecX<X, N>& b,
	const VecX<X, N>& c)
{
	VecX<X, N> p;
	p.x(X((a.x() + b.x() + c.x()) / 3.0));
	p.y(X((a.y() + b.y() + c.y()) / 3.0));
	if constexpr (N == 3)
	{
		p.z(X((a.z() + b.z() + c.z()) / 3.0));
	}
	return p;
}

template<class X>
VecX<X, 2> barycenter (
	const VecX<X, 2>& a,
	const VecX<X, 2>& b,
	const VecX<X, 2>& c,
	const VecX<X, 2>& d)
{
	return VecX<X, 2>(
		X((a.x() + b.x() + c.x() + d.x()) / 4.0),
		X((a.y() + b.y() + c.y() + d.y()) / 4.0)
	);
}

namespace enumerator
{

template<typename X>
class Enumerator {
public:
    using value_type = typename X::value_type;
    Enumerator (X& container) : m_container(container) {}

    class Iterator {
    public:
        Iterator (typename X::iterator it, size_t index) : m_it(it), m_index(index) {}

        std::pair<size_t, add_ref<value_type>> operator*() {
            return { m_index, *m_it };
        }

        Iterator& operator++ () {
            ++m_it;
            ++m_index;
            return *this;
        }

        bool operator!= (const Iterator& other) const {
            return m_it != other.m_it;
        }

    private:
        typename X::iterator    m_it;
        size_t                  m_index;
    };

    Iterator begin () {
        return Iterator(m_container.begin(), 0);
    }

    Iterator end () {
        return Iterator(m_container.end(), m_container.size());
    }

private:
    X& m_container;
};

template<typename X>
Enumerator<X> enumerate (X& container) { return Enumerator<X>(container); }

} // namespace enumerator

namespace subset
{

template<class UserData> struct Subset {
	uint32_t start_index;
	uint32_t length;
	UserData user_data;
	Subset () : start_index(0), length(0), user_data() {}
	Subset (uint32_t start, uint32_t len) : start_index(start), length(len), user_data() {}
	Subset (uint32_t start, uint32_t len, UserData user) : start_index(start), length(len), user_data(user) {}
};
template<class UserData> bool areDisjoint (add_cref<Subset<UserData>> a, add_cref<Subset<UserData>>b) {
	return (a.start_index + a.length <= b.start_index) || (b.start_index + b.length <= a.start_index);
}
template<class UserData> bool checkSubsetsDisjoint (add_cref<std::vector<Subset<UserData>>> subsets) {
	for (auto i = 0u; i < subsets.size(); ++i) {
		for (auto j = i + 1u; j < subsets.size(); ++j) {
			if (!areDisjoint<UserData>(subsets[i], subsets[j])) {
				return false;
			}
		}
	}
	return true;
}

} // namespace subset

namespace filter
{

template <typename It, typename Tp, typename Pred>
struct FilterIterator
{
	using iterator_category = std::forward_iterator_tag;
	using value_type		= Tp;
	using difference_type	= std::ptrdiff_t;
	using pointer			= Tp*;
	using reference			= Tp&;

	FilterIterator(It it, It end, Pred pred)
		: m_it(it), m_end(end), m_pred(pred)
	{
		skipInvalid();
	}

	auto& operator*() const { return *m_it; }
	auto* operator->() const { return &(*m_it); }

	difference_type operator-(const FilterIterator& other) const {
		return std::distance(other.m_it, m_it);
	}

	FilterIterator& operator++() {
		++m_it;
		skipInvalid();
		return *this;
	}
	FilterIterator operator++(int) {
		FilterIterator tmp = *this;
		++(*this);
		return tmp;
	}

		bool operator==(const FilterIterator& other) const {
		return m_it == other.m_it;
	}
	bool operator!=(const FilterIterator& other) const {
		return !(*this == other);
	}

private:
	void skipInvalid() {
		while (m_it != m_end && !m_pred(*m_it))
			++m_it;
	}

	It m_it;
	It m_end;
	Pred m_pred;
};

template <class Iter, class Pred>
class FilterRange {
public:
	using iterator = FilterIterator<Iter, typename std::iterator_traits<Iter>::value_type, Pred>;

	FilterRange(Iter first, Iter last, Pred pred)
		: m_begin(first), m_end(last), m_pred(pred) {
	}

	auto begin() const { return iterator(m_begin, m_end, m_pred); }
	auto end()   const { return iterator(m_end, m_end, m_pred); }

private:
	Iter m_begin;
	Iter m_end;
	Pred m_pred;
};

/*
* USAGE
* std::vector<int> v(100);
* std::iota(v.begin(), v.end(), 1);
* auto filter17 = filter::FilterRange(v.begin(), v.end(), [](int k) { return k % 17 == 0; });
* for (auto k : v) std::cout << k << ", ";
* > 17, 34, 51, ...
*/
} // namepsace filter

namespace span
{

template<class X>
class span {
	using pointer = std::conditional_t<std::is_const_v<X>, add_cptr<X>, add_ptr<X>>;
	using reference = std::conditional_t<std::is_const_v<X>, add_cref<X>, add_ref<X>>;
	pointer ptr;
public:
	const uint32_t start;
	const std::size_t count;
	template<class> friend class span;

	template<class Y,
		std::enable_if_t<
			std::is_same_v<std::remove_const_t<Y>, std::remove_const_t<X>>
			&& std::is_const_v<X> && !std::is_const_v<Y>, int> = 13>
	span(const span<Y>& other)
		: ptr(other.ptr), start(other.start), count(other.count) {}
	span(pointer p, uint32_t s, std::size_t n)
		: ptr(p), start(s), count(n) {}
	reference operator[](std::size_t index) const {
		ASSERTMSG(index < count, "Index (", index, ") of bounds (", count, ')');
		return ptr[index]; }
	std::size_t size() const { return count; }
	pointer data() const { return ptr; }
};
template<template<class, class...> class Y, class X, class... Z>
span<X> make_span(Y<X, Z...> & y, std::size_t n) {
	ASSERTION(n <= y.size());
	return span<X>(y.data(), 0u, n);
}
template<template<class, class...> class Y, class X, class... Z>
span<std::add_const_t<X>> make_span(Y<X, Z...> const& y, std::size_t n) {
	ASSERTION(n <= y.size());
	return span<X>(y.data(), 0u, n);
}
template<template<class, class...> class Y, class X, class... Z>
span<X> make_span(Y<X, Z...>& y, uint32_t start, std::size_t n) {
	ASSERTION(start + n <= y.size());
	return span<X>(y.data() + start, start, n);
}
template<template<class, class...> class Y, class X, class... Z>
span<std::add_const_t<X>> make_span(Y<X, Z...> const& y, uint32_t start, std::size_t n) {
	ASSERTION(start + n <= y.size());
	return span<std::add_const_t<X>>(y.data() + start, start, n);
}
template<class SPAN> struct _span_type_extractor;
template<class SPAN> struct _span_type_extractor<span<SPAN>> {
	using type = SPAN;
};
template<class SPAN> using span_type = typename _span_type_extractor<SPAN>::type;
} // namespace span
#define SPAN_TYPE(span__) span::span_type<std::remove_reference_t<decltype(span__)>>

template<class ARR> struct StedArraySize;
template<class _Ty, size_t _Size> struct StedArraySize<std::array<_Ty, _Size>> {
	static inline const size_t size = _Size;
};
template<class ARR> constexpr size_t sted_array_size = StedArraySize<ARR>::size;
#define STED_ARRAY_SIZE(a__) sted_array_size<std::remove_const_t<std::remove_reference_t<decltype(a__)>>>
/* USAGE
	constexpr size_t size = 33;
	std::array<int,size> a;
	static_assert(sted_array_size<std::remove_reference_t<decltype(a)>>::size == size, "???");
*/

// EPDM, Torpol

} // namesapce vtf

#endif // __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
