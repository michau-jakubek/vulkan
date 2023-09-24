#include "vtfMatrix.hpp"

namespace vtf
{

template<> void translate<Mat3> (Mat3& mat, typename MatE<Mat3>::VecY const& vec)
{
	mat = {	1, 0, vec[0],
			0, 1, vec[1],
			0, 0,     1 };
}
template<> void translate<Mat4> (Mat4& mat, typename MatE<Mat4>::VecY const& vec)
{
	mat = {	1, 0, 0, vec[0],
			0, 1, 0, vec[1],
			0, 0, 1, vec[2],
			0, 0, 0,     1 };
}
template<> void scale<Mat3> (Mat3& mat, typename MatE<Mat3>::VecY const& s)
{
	mat = {	s[0],	0,	  0,
			  0,  s[1],	  0,
			  0,	0,	  1 };
}
template<> void scale<Mat4> (Mat4& mat, typename MatE<Mat4>::VecY const& s)
{
	mat = {	s[0],	0,	  0,	0,
			  0,  s[1],	  0,	0,
			  0,	0,	s[2],	0,
			  0,	0,	  0,	1 };
}
template<> void rotate<Mat3> (Mat3& mat, typename MatE<Mat3>::VecY const& angle)
{
	typedef typename MatE<Mat3>::type T;
	auto sz = std::sin(angle[2] * PI<T>);
	auto cz = std::cos(angle[2] * PI<T>);
	auto sy = std::sin(angle[1] * PI<T>);
	auto cy = std::cos(angle[1] * PI<T>);
	auto sx = std::sin(angle[0] * PI<T>);
	auto cx = std::cos(angle[0] * PI<T>);

	Mat3 matZ({
		 cz,  -sz,   0,
		 sz,   cz,   0,
		  0,    0,   1
	});
	Mat3 matY({
		 cy,   0,   sy,
		  0,   1,    0,
		-sy,   0,   cy
	});
	Mat3 matX({
		 1,    0,    0,
		 0,   cx,  -sx,
		 0,   sx,   cx
	});

	mat = matZ * matY * matX;
}
template<> void rotate<Mat4> (Mat4& mat, typename MatE<Mat4>::VecY const& angle)
{
	typedef typename MatE<Mat3>::type T;
	auto sz = std::sin(angle[2] * PI<T>);
	auto cz = std::cos(angle[2] * PI<T>);
	auto sy = std::sin(angle[1] * PI<T>);
	auto cy = std::cos(angle[1] * PI<T>);
	auto sx = std::sin(angle[0] * PI<T>);
	auto cx = std::cos(angle[0] * PI<T>);

	Mat4 matZ({
		 cz,  -sz,   0,   0,
		 sz,   cz,   0,   0,
		 0,     0,   1,   0,
		 0,     0,   0,   1
	});
	Mat4 matY({
		 cy,   0,   sy,   0,
		  0,   1,    0,   0,
		-sy,   0,   cy,   0,
		  0,   0,    0,   1
	});
	Mat4 matX({
		 1,    0,    0,   0,
		 0,   cx,  -sx,   0,
		 0,   sx,   cx,   0,
		 0,    0,    0,   1
	});

	mat = matZ * matY * matX;
}

} // namespace vtf
