#ifndef __VTF_VECTOR_HPP_INCLUDED__
#define __VTF_VECTOR_HPP_INCLUDED__

#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <cmath>

namespace vtf
{

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
		z[k] = operator[](i);
	}

	template<class VecZ, class I, class... J>
	void swizzle_impl (VecZ& z, size_t k, I i, J... j) const
	{
		z[k] = operator[](i);
		swizzle_impl<VecZ, J...>(z, k+1, j...);
	}

public:

	VecX()
	{
		for (size_t i = 0; i < N; ++i)
			data[i] = T{};
	}

	template<class X>
	VecX(const X& x)
	{
		for (size_t i = 0; i < N; ++i)
			data[i] = static_cast<T>(x);
	}

    template<class X, class... Y>
    VecX(const X& x, const Y&... y) : VecX(tag(0), x, y...)
    {
    }

	template<class U, size_t M>
	VecX(const VecX<U, M>& other)
	{
		for (size_t i = 0; i < N && i < M; ++i)
			data[i] = other[i];
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

	T angle (const VecX& other) const
	{
		const T product = dot(other);
		const T theta = product / length() / other.length();
		return std::acos(theta);
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

	template<class U>
	VecX<U,N> cast () const
	{
		VecX<U,N> v;
		for (size_t i = 0; i < N; ++i)
			v[i] = static_cast<U>(operator[](i));
		return v;
	}

	template<class U, size_t M>
	VecX<U,M> cast () const
	{
		VecX<U,M> v;
		for (size_t i = 0; i < N && i < M; ++i)
			v[i] = static_cast<U>(operator[](i));
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

	static constexpr size_t count () { return N; }
	static constexpr size_t size () { return N * sizeof(T); }
    template<class U, size_t I> VecX<T,N>& operator=(const VecX<U,I>&);

	template<class U, size_t M> bool operator==(const VecX<U,M>&) const;
	template<class U, size_t M> bool operator!=(const VecX<U,M>& other) const {
		return ! this->operator==(other);
	}

	template<class U, size_t M, class V, size_t Q>
		bool equalToll(const VecX<U,M>&, const VecX<V,Q>&) const;

	const T& x() const  { verifyIndex(0); return (*this)[0]; }
	const T& y() const  { verifyIndex(1); return (*this)[1]; }
	const T& z() const  { verifyIndex(2); return (*this)[2]; }
	const T& w() const  { verifyIndex(3); return (*this)[3]; }
	const T& r() const  { return x(); }
	const T& g() const  { return y(); }
	const T& b() const  { return z(); }
	const T& a() const  { return w(); }

	VecX& x(const T& v)    { verifyIndex(0); (*this)[0] = v; return *this; }
	VecX& y(const T& v)    { verifyIndex(1); (*this)[1] = v; return *this; }
	VecX& z(const T& v)    { verifyIndex(2); (*this)[2] = v; return *this; }
	VecX& w(const T& v)    { verifyIndex(3); (*this)[3] = v; return *this; }
	VecX& a(const T& v)    { x(v); }
	VecX& b(const T& v)    { y(v); }
	VecX& c(const T& v)    { z(v); }
	VecX& d(const T& v)    { w(v); }

};

template<class> struct vecx_info;
template<class T, size_t N>
struct vecx_info<VecX<T,N>>
{
	typedef T type;
	static const size_t count = N;
};
template<class V> using vecx_type = typename vecx_info<V>::type;
template<class V> constexpr size_t vecx_count = vecx_info<V>::count;

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
template<class U, size_t I>
inline VecX<T,N>& VecX<T,N>::operator=(const VecX<U,I>& s)
{
    size_t ni = 0;
    for ( ; ni < N && ni < I; ++ni)
        data[ni] = s.data[ni];
    for ( ; ni < N; ++ni)
        data[ni] = T{};
   return *this;
}

template<class T, size_t N>
template<class U, size_t M>
inline bool VecX<T,N>::operator==(const VecX<U,M>& p) const
{
	bool ok = false;
	if (N == M)
	{
		ok = true;
		for (size_t i = 0; ok && i < N; ++i)
			ok = data[i] == p.data[i];
	}
	return ok;
}

template<class T, size_t N>
template<class U, size_t M, class V, size_t Q>
inline bool VecX<T,N>::equalToll(const VecX<U,M>&, const VecX<V,Q>&) const
{
	// not implemented yet 2022-02-14, 3.35 AM CET
	return false;
}

template<class T, size_t N>
inline std::ostream& operator<<(std::ostream& s, const VecX<T,N>& p)
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

template<class T, class... Others>
struct VectorAccess2D
{
	typedef std::vector<T, Others...> vector_type;
	vector_type&	vector;
	const uint32_t	colCount;
	VectorAccess2D (vector_type& v, uint32_t width) : vector(v), colCount(width) {}
	uint32_t makeFlatIndex (uint32_t col, uint32_t row) {
		const uint32_t flatIndex = row * colCount + col;
		return flatIndex;
	}
	T& operator ()(uint32_t col, uint32_t row) {
		const uint32_t flatIndex = makeFlatIndex(col, row);
		return vector.at(flatIndex);
	}
	const T& operator ()(uint32_t col, uint32_t row) const {
		const uint32_t flatIndex = makeFlatIndex(col, row);
		return vector.at(flatIndex);
	}
};

template<class T, class... Others>
VectorAccess2D<T,Others...> makeVectorAccess2D (std::vector<T, Others...>& vec, uint32_t width)
{
	return VectorAccess2D<T, Others...>(vec, width);
}

} // namespace vtf

#endif // __VTF_VECTOR_HPP_INCLUDED__
