#ifndef __VTF_THREAD_POOL_HELPERS_HPP_INCLUDED__
#define __VTF_THREAD_POOL_HELPERS_HPP_INCLUDED__

#include "vtfCUtils.hpp"
#include <algorithm>
#include <cstddef>
#include <tuple>
#include <typeindex>

namespace vtf
{
namespace tph
{

template<class X> struct is_pointer_reference : std::false_type {};
template<class Y> struct is_pointer_reference<Y*&> : std::true_type {};
template<class Y> struct is_pointer_reference<Y const*&> : std::true_type {};
template<class Y> constexpr bool is_pointer_reference_v = is_pointer_reference<Y>::value;
template<class X> struct pointer_reference { typedef X type; };
template<class Y> struct pointer_reference<Y*&> { typedef Y* type; };
template<class Y> struct pointer_reference<Y const*&> { typedef const Y* type; };
template<class Y> using pointer_reference_t = typename pointer_reference<Y>::type;


template<class X, class Tp> struct wrap_tuple;
template<class Y, class... X> struct wrap_tuple<Y, std::tuple<X...>>
{
	typedef std::tuple<std::conditional_t<std::is_lvalue_reference_v<X>,
		std::conditional_t<is_pointer_reference_v<X>,
			pointer_reference_t<X>, std::reference_wrapper<std::remove_reference_t<X>>>,
				std::conditional_t<std::is_rvalue_reference_v<X>&& std::is_same_v<std::remove_reference_t<X>, Y>,
					std::remove_reference_t<X>, X>>... > type;
};


template<bool, class X> struct forward_tuple_element
{
	X&& x;
	forward_tuple_element(std::remove_reference_t<X>&& rx) : x(std::forward<X>(rx)) {}
	operator X && () { return std::forward<X>(x); }
};
template<class X> struct forward_tuple_element<true, X>
{
	typedef std::reference_wrapper<std::remove_reference_t<X>> RX;
	RX x;
	forward_tuple_element(std::remove_reference_t<X>&& rx) : x(rx) {}
	operator RX() { return x; }
};
template<class X> struct forward_tuple_element<true, X*&>
{
	X* x;
	forward_tuple_element(X* rx) : x(rx) {}
	operator X* () { return x; }
};
template<class X> struct forward_tuple_element<true, X const*&>
{
	const X* x;
	forward_tuple_element(X const* rx) : x(rx) {}
	operator const X* () { return x; }
};


template<class X, class Tp, std::size_t... I> auto make_wrapped_tuple_impl (Tp&& tp, std::index_sequence<I...>)
	-> typename wrap_tuple<X, Tp>::type
{
	return typename wrap_tuple<X, Tp>::type(forward_tuple_element<
		std::is_lvalue_reference_v< std::tuple_element_t<I, Tp> >, std::tuple_element_t<I, Tp>>(std::move(std::get<I>(tp)))...);
}
template<class Y, class... X> auto make_wrapped_tuple (std::tuple<X...>&& src)
	-> typename wrap_tuple<Y, std::tuple<X...>>::type
{
	return make_wrapped_tuple_impl<Y>(std::forward<std::tuple<X...>>(src), std::index_sequence_for<X...>());
}

template<class A, class B> struct update_wrapped_tuple_element
{
	static constexpr void work (std::size_t, add_ref<A>, add_ref<B>) { }
};
template<class A> struct update_wrapped_tuple_element<A, std::reference_wrapper<A>>
{
	static constexpr void work (std::size_t, add_ref<A> ref, add_ref<std::reference_wrapper<A>> value) { value = std::ref(ref); }
};
template<class A> struct update_wrapped_tuple_element<A, std::reference_wrapper<std::add_const_t<A>>>
{
	static constexpr void work (std::size_t, add_ref<A> ref, add_ref<std::reference_wrapper<std::add_const_t<A>>> value) { value = std::cref(ref); }
};
template<class A> struct update_wrapped_tuple_element<A, std::remove_reference_t<add_ptr<A>> >
{
	static constexpr void work (std::size_t, add_ref<A> ref, add_ref<add_ptr<A>> value) { value = &ref; }
};
template<class A> struct update_wrapped_tuple_element<A, add_cptr<A>>
{
	static constexpr void work (std::size_t, add_ref<A> ref, add_ref<add_cptr<A>> value) { value = &ref; }
};
template<class A> struct update_wrapped_tuple_element<A, std::reference_wrapper<std::add_const_t<add_ptr<A>>>>
{
	static_assert(!std::is_same_v<A, A>, "Cannot update const object");
	static constexpr void work(std::size_t, add_ref<A>, add_ref<std::reference_wrapper<std::add_const_t<add_ptr<A>>>>) { }
};
template<class A> struct update_wrapped_tuple_element<A, A>
{
	static constexpr void work(std::size_t, add_ref<A> ref, add_ref<A> value) { value = ref; }
};
template<class A> struct update_wrapped_tuple_element<A, std::add_const_t<A>>
{
	static_assert(!std::is_same_v<A, A>, "Cannot update const object");
	static constexpr void work(std::size_t, add_ref<A>, add_cref<A>) { }
};


template<class Tp, class Y, std::size_t... I> void update_wrapped_tuple_impl (Tp& tp, Y& ref, std::index_sequence<I...>)
{
	(update_wrapped_tuple_element<Y, std::tuple_element_t<I, Tp>>::work(I, ref, std::get<I>(tp)), ...);
}
template<class Y, class... X> void update_wrapped_tuple (std::tuple<X...>& tp, Y& ref)
{
	update_wrapped_tuple_impl(tp, ref, std::index_sequence_for<X...>());
}

// Trivial helper type for holding callable parameters.
template<class T> struct FakeMoveable
{
	bool has_data;
	uint8_t data[sizeof(T)];
	FakeMoveable () : has_data(false), data{} {}
	FakeMoveable (const FakeMoveable&) = delete;
	FakeMoveable (FakeMoveable&& other)
		: has_data(other.has_data), data{} {
		if (other.has_data) {
			assign(std::move(*other.get()));
		}
		other.has_data = false;
	}
	~FakeMoveable () { destroy(); }
	void assign (T&& other) {
		destroy();
		new (data) T(std::move(other));
		has_data = true;
	}
	add_ptr<T> get () {
		return reinterpret_cast<add_ptr<T>>(data);
	}
	operator add_ref<T> () {
		return *get();
	}
	void destroy () {
		if (has_data) {
			get()->~T();
			has_data = false;
		}
	}
};

} // tph
} // vtf

#endif // __VTF_THREAD_POOL_HELPERS_HPP_INCLUDED__