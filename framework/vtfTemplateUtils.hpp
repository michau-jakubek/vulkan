#ifndef __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
#define __VTF_TEMPLATE_UTILS_HPP_INCLUDED__

#include "vtfVector.hpp"

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

} // namesapce vtf

#endif // __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
