#ifndef __VTF_VECTOR_HPP_INCLUDED__
#define __VTF_VECTOR_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfCUtils.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <cmath>

namespace vtf
{

template<class T, size_t N>
class VecX;

template<class> struct vecx_info;
template<class T, size_t N>
struct vecx_info<VecX<T,N>>
{
	typedef T type;
	enum { count = N };
};
template<class V> using vecx_type = typename vecx_info<V>::type;
template<class V> constexpr size_t vecx_count = vecx_info<V>::count;

template<class T>
T ftrunc (T v, uint32_t digits)
{
	T raise = std::pow(T(10), T(digits));
	T vint, rint, vfrac = std::modf(v, &vint);
	std::modf(vfrac * raise, &rint);
	T result = vint + rint / raise;
	return result;
}

template<class T, size_t N>
class VecX
{
    struct tag {
        size_t x;
        explicit tag(size_t x) : x(x) {}
    };

    T data[N];

    VecX(tag z) {
        for (size_t i = z.x; i < N; ++i) data[i] = T{};
    }

    template<class X, class... Y>
    VecX(tag z, const X& x, const Y&... y) : VecX(tag(z.x+1), y...) {
		if (z.x < N) data[z.x] = static_cast<T>(x);
    }

protected:

	template<class VecZ, class I>
	void swizzle_impl (VecZ& z, size_t k, I i) const
	{
		z[k] = operator[](make_unsigned(i));
	}
	template<class VecZ, class I, class... J>
	void swizzle_impl (VecZ& z, size_t k, I i, J... j) const
	{
		z[k] = operator[](make_unsigned(i));
		swizzle_impl<VecZ, J...>(z, k+1, j...);
	}

	template<class VecZ>
	void cast_complete_impl (VecZ&, size_t) const
	{
	}
	template<class VecZ, class Complete>
	void cast_complete_impl (VecZ& z, size_t i, Complete const& value) const
	{
		if (i < z.count()) z[i] = static_cast<vecx_type<VecZ>>(value);
	}
	template<class VecZ, class Complete0, class... Completes>
	void cast_complete_impl (VecZ& z, size_t i, Complete0 const& value, Completes const&... completeValues) const
	{
		if (i < z.count()) z[i] = static_cast<vecx_type<VecZ>>(value);
		cast_complete_impl(z, i+1, completeValues...);
	}
	template<class VecZ, class... Completes>
	void cast_complete (VecZ& z, size_t i, Completes const&... completeValues) const
	{
		cast_complete_impl(z, i, completeValues...);
	}

public:

	VecX()
	{
		for (size_t i = 0; i < N; ++i)
			data[i] = T{};
	}

	template<class X, typename std::enable_if<std::is_arithmetic<X>::value, int>::type = 1>
	VecX(const X& initValueForAllElements)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] = static_cast<T>(initValueForAllElements);
	}

	template<class X, class... Y,
			 typename std::enable_if<std::is_arithmetic<X>::value, int>::type = 3>
    VecX(const X& x, const Y&... y) : VecX(tag(0), x, y...)
    {
		/* delegate work to the protected constructor */
    }

	template<class U>
	explicit VecX(const VecX<U, N>& other)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] = static_cast<T>(other[i]);
	}

	template<class U, size_t M>
	VecX& assign(const VecX<U, M>& other)
	{
		for (size_t i = 0; i < N && i < M; ++i)
			data[i] = static_cast<T>(other[i]);
		return *this;
	}

	template<class X, size_t M>
	VecX(X const (&a)[M], const T def = T{})
	{
		size_t i = 0;
		const size_t c = N<M ? N : M;
		for (; i < c; ++i) data[i] = a[i];
		for (; i < N; ++i) data[i] = def;
	}

	void verifyIndex (size_t index) const
	{
		if (index >= N) throw std::runtime_error("out of bounds");
	}

    T& operator[](size_t i)
    {
		verifyIndex(i);
        return data[i];
    }

    const T& operator[](size_t i) const
    {
		verifyIndex(i);
        return data[i];
    }

    template<class U>
	VecX operator+(const VecX<U,N>& v) const
    {
		VecX result;
        for (size_t i = 0; i < N; ++i)
            result[i] = data[i] + T(v[i]);
        return result;
    }

	template<class U>
	VecX& operator+=(const VecX<U,N>& v)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] += static_cast<T>(v[i]);
		return *this;
	}

	template<class U>
	VecX operator-(const VecX<U,N>& v) const
    {
		VecX result;
        for (size_t i = 0; i < N; ++i)
            result[i] = data[i] - T(v[i]);
        return result;
    }

	template<class U>
	VecX& operator-=(const VecX<U,N>& v)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] -= static_cast<T>(v[i]);
		return *this;
	}

    template<class U>
	VecX operator*(const VecX<U,N>& v) const
    {
		VecX result;
        for (size_t i = 0; i < N; ++i)
            result[i] = data[i] * v[i];
        return result;
    }

	template<class U>
	VecX operator*(const U& u) const
	{
		VecX<T,N> result;
		for (size_t i = 0; i < N; ++i)
			result[i] = data[i] * u;
		return result;
	}

	template<class U>
	VecX& operator*=(const U& u)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] *= u;
		return *this;
	}

	template<class U>
	VecX operator/(const VecX<U,N>& v) const
	{
		VecX<T,N> result;
		for (size_t i = 0; i < N; ++i)
			result[i] = data[i] / static_cast<T>(v[i]);
		return result;
	}

	template<class U>
	VecX operator/(const U& u) const
	{
		VecX result;
		for (size_t i = 0; i < N; ++i)
			result[i] = data[i] / static_cast<T>(u);
		return result;
	}

	template<class U>
	VecX& operator/=(const U& u)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] /= static_cast<T>(u);
		return *this;
	}

	/**
	 * @brief Compute a Magnitude of a vector
	 *         -->
	 *       || v ||
	 */
	T length () const
	{
		T sum = static_cast<T>(0);
		for (size_t i = 0; i < N; ++i)
			sum += data[i] * data[i];
		return std::sqrt(sum);
	}

	/**
	 * @brief Perform normalization
	 *              -->
	 *       ^       u
	 *       u = ---------
	 *              -->
	 *            || u ||
	 */
	void normalize ()
	{
		T value = length();
		if (value != static_cast<T>(0))
			operator/=(value);
	}

	/**
	 * @note The dot product of two vectors is equal to
	 * the scalar product of their lengths times
	 * the cosine of the angle between them.
	 */
	T dot (const VecX& other) const
	{
		T product = static_cast<T>(0);
		for (size_t i = 0; i < N; ++i)
			product += data[i] * other.data[i];
		return product;
	}

	T prod () const
	{
		T p = static_cast<T>(1);
		for (size_t i = 0; i < N; ++i)
			p *= data[i];
		return p;
	}

	T angle (const VecX& other) const
	{
		const T product = dot(other);
		const T theta = product / length() / other.length();
		return std::acos(theta);
	}

	void round (uint32_t digits)
	{
		for (int i = 0; i < N; ++i)
			data[i] = fround(data[i], digits);
	}

	template<size_t M, typename std::enable_if<(M==3 && N==M), bool>::type = true>
	VecX<T,3> cross (const VecX<T,M>& other) const
	{
		return VecX<T,3>
				(
					data[1] * other.data[2] - data[2] * other.data[1],
					data[2] * other.data[0] - data[0] * other.data[2],
					data[0] * other.data[1] - data[1] * other.data[0]
				);
	}

	template<class I, class... J>
	auto swizzle (I i, J... j) const -> VecX<T, sizeof...(J)+1>
	{
		static_assert(N != 0, "");
		VecX<T, sizeof...(J)+1> z;
		swizzle_impl<VecX<T, sizeof...(J)+1>, I, J...>(z, 0, i, j...);
		return z;
	}

	template<class V, class U = vecx_type<V>, size_t M = vecx_count<V>, class... Completes>
	auto cast (Completes const&... completeValues) const -> VecX<U,M>
	{
		VecX<U,M> v;
		v.assign(*this);
		cast_complete<VecX<U,M>, Completes...>(v, N, completeValues...);
		return v;
	}

	template<class V, class U = vecx_type<V>, size_t M = vecx_count<V>>
	auto bitcast () const -> VecX<U,M>
	{
		union {
			U u;
			T t;
		} x;
		VecX<U,M> v;
		for (size_t i = 0; i < N && i < M; ++i) {
			x = {};
			x.t = operator[](i);
			v[i] = x.u;
		}
		x.t = T(0);
		for (size_t i = N; i < M; ++i)
			v[i] = x.u;
		return v;
	}

	template<size_t M>
	VecX<T, M> trunc () const
	{
		static_assert(M <= N && M != 0, "");
		VecX<T,M> v;
		for (size_t i = 0; i < N && i < M; ++i)
			v[i] = operator[](i);
		return v;
	}

	static VecX fromText (add_cref<std::string> text, add_cref<VecX> def,
						  add_ref<std::array<bool, N>> status, add_ptr<bool> all = nullptr)
	{
		VecX				result;
		bool				allStatus	= true;
		const strings		chunks		= splitString(text);
		for (std::size_t i = 0u; i < N && i < data_count(chunks); ++i)
		{
			result[i] = ::vtf::template fromText(chunks.at(i), def[i], status.at(i));
			allStatus &= status.at(i);
		}
		if (all) *all = allStatus;
		return result;
	}

	typedef T type;
	static constexpr size_t count () { return N; }
	static constexpr size_t size () { return N * sizeof(T); }
	template<class U> VecX<T,N>& operator= (const VecX<U,N>&);

	template<class U, size_t M> bool operator== (const VecX<U,M>&) const;
	template<class U, size_t M> bool operator!= (const VecX<U,M>& other) const {
		return ! this->template operator==(other);
	}
	template<class U, size_t M> bool comparePartially (const VecX<U,M>&, T tol) const;

	T& x()	{ verifyIndex(0); return (*this)[0]; }
	T& y()	{ verifyIndex(1); return (*this)[1]; }
	T& z()	{ verifyIndex(2); return (*this)[2]; }
	T& w()	{ verifyIndex(3); return (*this)[3]; }
	T& r()	{ verifyIndex(0); return (*this)[0]; }
	T& g()	{ verifyIndex(1); return (*this)[1]; }
	T& b()	{ verifyIndex(2); return (*this)[2]; }
	T& a()	{ verifyIndex(3); return (*this)[3]; }

	const T& x() const  { return (*this)[0]; }
	const T& y() const  { return (*this)[1]; }
	const T& z() const  { return (*this)[2]; }
	const T& w() const  { return (*this)[3]; }
	const T& r() const  { return (*this)[0]; }
	const T& g() const  { return (*this)[1]; }
	const T& b() const  { return (*this)[2]; }
	const T& a() const  { return (*this)[3]; }

	VecX& x(const T& v)    { verifyIndex(0); (*this)[0] = v; return *this; }
	VecX& y(const T& v)    { verifyIndex(1); (*this)[1] = v; return *this; }
	VecX& z(const T& v)    { verifyIndex(2); (*this)[2] = v; return *this; }
	VecX& w(const T& v)    { verifyIndex(3); (*this)[3] = v; return *this; }
	VecX& r(const T& v)    { verifyIndex(0); (*this)[0] = v; return *this; }
	VecX& g(const T& v)    { verifyIndex(1); (*this)[1] = v; return *this; }
	VecX& b(const T& v)    { verifyIndex(2); (*this)[2] = v; return *this; }
	VecX& a(const T& v)    { verifyIndex(3); (*this)[3] = v; return *this; }

};

typedef VecX<float,1> Vec1;
typedef VecX<float,2> Vec2;
typedef VecX<float,3> Vec3;
typedef VecX<float,4> Vec4;

typedef VecX<int8_t,1> BVec1;
typedef VecX<int8_t,2> BVec2;
typedef VecX<int8_t,3> BVec3;
typedef VecX<int8_t,4> BVec4;

typedef VecX<uint8_t,1> UBVec1;
typedef VecX<uint8_t,2> UBVec2;
typedef VecX<uint8_t,3> UBVec3;
typedef VecX<uint8_t,4> UBVec4;

typedef VecX<int16_t,1> WVec1;
typedef VecX<int16_t,2> WVec2;
typedef VecX<int16_t,3> WVec3;
typedef VecX<int16_t,4> WVec4;

typedef VecX<uint16_t,1> UWVec1;
typedef VecX<uint16_t,2> UWVec2;
typedef VecX<uint16_t,3> UWVec3;
typedef VecX<uint16_t,4> UWVec4;

typedef VecX<int32_t,1> IVec1;
typedef VecX<int32_t,2> IVec2;
typedef VecX<int32_t,3> IVec3;
typedef VecX<int32_t,4> IVec4;

typedef VecX<uint32_t,1> UVec1;
typedef VecX<uint32_t,2> UVec2;
typedef VecX<uint32_t,3> UVec3;
typedef VecX<uint32_t,4> UVec4;

template<class T, size_t N>
template<class U>
inline VecX<T,N>& VecX<T,N>::operator= (const VecX<U,N>& s)
{
	for (size_t i = 0; i < N; ++i)
		data[i] = static_cast<T>(s.data[i]);
	return *this;
}

template<class T, size_t N>
template<class U, size_t M>
bool VecX<T,N>::operator== (const VecX<U,M>& p) const
{
	for (size_t k = 0, i = 0; k < N && i < M; ++k, ++i)
	{
		if (data[i] != p[i])
			return false;
	}
	return true;
}
template<class T, size_t N>
template<class U, size_t M>
bool VecX<T,N>::comparePartially (const VecX<U,M>& p, T tol) const
{
	for (size_t i = 0; i < N && i < M; ++i)
	{
		if (!(T(p[i]) >= data[i] - tol && T(p[i]) <= data[i] + tol))
			return false;
	}
	return true;
}

template<class T, size_t N>
inline std::ostream& operator<< (std::ostream& s, const VecX<T,N>& p)
{
    s << '[';
    for (size_t i = 0; i < N; ++i) {
        if (i) s << ',';
        s <<
            static_cast<
                typename std::conditional<
					std::is_same<float,T>::value || std::is_same<double,T>::value,
                        T, int>::type>(p[i]);
    }
    s << ']';
    return s;
}

} // namespace vtf

#endif // __VTF_VECTOR_HPP_INCLUDED__
