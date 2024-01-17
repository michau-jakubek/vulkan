#ifndef __VTF_STRICT_TEMPLATES_INCLUDED__
#define __VTF_STRICT_TEMPLATES_INCLUDED__

#include <type_traits>

namespace vtf
{
namespace select_details
{
template<class X, bool = false> struct maybe_lambda_impl : std::false_type
{
	typedef X result_type;
};
template<class X> struct maybe_lambda_impl<X, true> : std::true_type
{
	typedef decltype((*std::declval<X*>())()) result_type;
	static_assert(std::is_invocable_r_v<result_type, X>, "???");
};
template<class X> struct maybe_lambda : maybe_lambda_impl<X, std::is_invocable_v<X>> {};
template<class X> using maybe_lambda_result_type = typename maybe_lambda<X>::result_type;
template<class X> auto select_impl(const X& x, const X*) -> maybe_lambda_result_type<X>
{
	return x;
}
template<class X> auto select_impl(const X& x, const void*) -> maybe_lambda_result_type<X>
{
	return x();
}
} // select_details
template<class X> constexpr bool is_lambda_param_v(X&& x)
	{ return select_details::maybe_lambda<std::decay_t<decltype(x)>>::value; }
template<class X> using lambda_result_type = typename select_details::maybe_lambda<X>::result_type;
template<class X> constexpr bool is_lambda_v = select_details::maybe_lambda<X>::value;

template<class Then, class Else>
auto select(bool pred, const Then& then_, const Else& else_)
	-> std::common_type_t<lambda_result_type<Then>,	lambda_result_type<Else>>
{
	if (pred)
		return	select_details::select_impl(then_,
					std::conditional_t<is_lambda_v<Then>, const void*, const Then*>(&then_));
	return		select_details::select_impl(else_,
					std::conditional_t<is_lambda_v<Else>, const void*, const Else*>(&else_));
}
template<class Pred, class Then, class Else,
	std::enable_if_t<is_lambda_v<Pred> && std::is_same_v<lambda_result_type<Pred>, bool>, bool> = true>
auto select(Pred pred, const Then& then_, const Else& else_)
	-> std::common_type_t<lambda_result_type<Then>,	lambda_result_type<Else>>
{
	return select<Then, Else>(pred(), then_, else_);
}

} // namespace vtf

#endif // __VTF_STRICT_TEMPLATES_INCLUDED__
