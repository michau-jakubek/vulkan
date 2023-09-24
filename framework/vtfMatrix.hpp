#ifndef __VTF_MATRIX_HPP_INCLUDED__
#define __VTF_MATRIX_HPP_INCLUDED__

#include "vtfVkUtils.hpp"
#include "vtfVector.hpp"
#include <cmath>
#include <initializer_list>
#include <ostream>
#include <type_traits>

/**
 * @copybrief: https://www.khronos.org/opengl/wiki/Data_Type_(GLSL)
 * @copydetails: 2023-08-27
 *
 * In addition to vectors, there are also matrix types. All matrix types are floating-point,
 * either single-precision or double-precision. Matrix types are as follows, where n and m can be the numbers 2, 3, or 4:
 *	* matnxm: A matrix with n columns and m rows (examples: mat2x2, mat4x3). Note that this is backward from convention in mathematics!
 *  * matn: Common shorthand for matnxn: a square matrix with n columns and n rows.
 * Double-precision matrices (GL 4.0 and above) can be declared with a dmat instead of mat (example: dmat4x4).
 * OpenGL stores matrices in column-major format—that is, elements are stored contiguously in columns.
 * Swizzling does not work with matrices. You can instead access a matrix's fields with array syntax:
 * mat3 theMatrix;
 * theMatrix[1] = vec3(3.0, 3.0, 3.0); // Sets the second column to all 3.0s
 * theMatrix[2][0] = 16.0; // Sets the first entry of the third column to 16.0.
 * However, the result of the first array accessor is a vector, so you can swizzle that:
 *    mat3 theMatrix;
 *    theMatrix[1].yzx = vec3(3.0, 1.0, 2.0);
 */
namespace vtf
{

template<class> std::bad_typeid PI;
template<> inline const double PI<double> = std::atan(1.0) * 4.0;
template<> inline const float PI<float> = std::atan(1.0f) * 4.0f;

template<class MatY> struct MatE;
template<class T, size_t Ncols, size_t Mrows> class MatX;
template<class T, size_t Ncols, size_t Mrows>
struct MatE<MatX<T, Ncols, Mrows>>
{
	typedef VecX<T, Ncols> VecY;
	typedef VecX<T, 3> VecOp;
	typedef T type;
};

template<class MatY> void translate (MatY&, typename MatE<MatY>::VecY const& vec);
template<class MatY> void rotate (MatY&, typename MatE<MatY>::VecY const& axis);
template<class MatY> void scale (MatY&, typename MatE<MatY>::VecY const& s);

template<class T, size_t Ncols, size_t Mrows>
class MatX
{
	typedef VecX<T, Mrows> ColVec;
	typedef VecX<T, Ncols> RowVec;
	ColVec data[Ncols];
	void verifyColIndex (size_t colIndex) const
	{
		if (colIndex >= Ncols) throw std::runtime_error("column index out of bounds");
	}
	void verifyRowIndex (size_t rowIndex) const
	{
		if (rowIndex >= Mrows) throw std::runtime_error("row index out of bounds");
	}
public:
	typedef T CellType;
	enum class Enums : size_t
	{
		Diagonal,
		Sequential,
		ColCount = Ncols,
		RowCount = Mrows,
	};

	MatX () : data() {}
	MatX (Enums e, T value = T(1))
	{
		switch (e)
		{
		case Enums::Diagonal:
			*this = MatX::diagonal(value);
			break;
		case Enums::Sequential:
			*this = MatX::sequential(value);
			break;
		default:
			throw std::runtime_error("Improper enum");
		}
	}
	MatX (const MatX& other) { *this = other; }
	// construct matrix initialized Row-By-Row
	MatX (std::initializer_list<T> list)
	{
		size_t n = 0;
		MatX& me = *this;
		auto i = list.begin();
		for (size_t row = 0; row < Mrows; ++row)
		for (size_t col = 0; col < Ncols && n < list.size(); ++col, ++n)
		{
			me[col][row] = *i++;
		}
	}
	// constructs single column matrix
	template<size_t ElementCount, typename std::enable_if<ElementCount == Mrows && Ncols == 1u, int>::type = 5>
		MatX (VecX<T, ElementCount> const& vec)
	{
		data[0] = vec;
	}
	// returns first column if it is a single column matrix
	template<size_t ElementCount, typename std::enable_if<ElementCount == Mrows && Ncols == 1u, int>::type = 7>
		operator VecX<T, ElementCount> () const
	{
		return data[0];
	}
	VecX<T, Mrows>& operator[] (size_t colIndex)
	{
		verifyColIndex(colIndex);
		return data[colIndex];
	}
	VecX<T, Mrows> const& operator[] (size_t colIndex) const
	{
		verifyColIndex(colIndex);
		return data[colIndex];
	}
	MatX& operator= (const MatX& other)
	{
		for (size_t col = 0; col < Ncols; ++col)
			data[col] = other.data[col];
		return *this;
	}
	bool operator== (const MatX& other) const
	{
		for (size_t row = 0; row < Mrows; ++row)
		for (size_t col = 0; col < Ncols; ++col)
		{
			if (data[col][row] != other[col][row])
				return false;
		}
		return true;
	}
	bool operator!= (const MatX& other) const
	{
		return (false == operator==(other));
	}
	RowVec row (size_t rowIndex) const
	{
		verifyRowIndex(rowIndex);
		RowVec			row	{};
		MatX	const&	mat	(*this);
		for (size_t col = 0; col < Ncols; ++col)
		{
			row[col] = mat[col][rowIndex];
		}
		return row;
	}
	ColVec& col (size_t colIndex)
	{
		verifyColIndex(colIndex);
		return data[colIndex];
	}
	ColVec const& col (size_t colIndex) const
	{
		verifyColIndex(colIndex);
		return data[colIndex];
	}
	friend ColVec operator* (const MatX& mat, const RowVec& vec)
	{
		// MatX<T, 1, Mrows> m = (mat * MatX<T, 1u, Ncols>(vec));
		return (mat * MatX<T, 1u, Mrows>(vec));
	}
	template<size_t OtherCols, size_t OtherRows, class MatY = MatX<T, OtherCols, Mrows>,
			 typename std::enable_if<OtherRows == Ncols, int>::type = 11>
	friend MatY operator* (MatX const& left, MatX<T, OtherCols, OtherRows> const& right)
	{
		MatY res;
		for (size_t yrow = 0; yrow < Mrows; ++yrow)
		{
			for (size_t ycol = 0; ycol < OtherCols; ++ycol)
			{
				T val{};
				for (size_t lcol = 0; lcol < Ncols; ++lcol)
				{
					const T l = left[lcol][yrow];
					const T r = right[ycol][lcol];
					// wise compiler should optimize above temporary variables
					val += l * r;
				}
				res[ycol][yrow] = val;
			}
		}
		return res;
	}
	template<size_t QuadDim> using OpsCondition = std::integral_constant<bool,
		QuadDim == Mrows && QuadDim == Ncols && (QuadDim == 3 || QuadDim == 4)>;
	template<size_t QuadDim, class MatY = MatX<T, QuadDim, QuadDim>,
			 typename std::enable_if<OpsCondition<QuadDim>::value, int>::type = 13>
	static MatY translate (VecX<T, QuadDim> const& vec)
	{
		MatY mat;
		::vtf::translate<MatY>(mat, vec);
		return mat;
	}
	template<size_t QuadDim, class MatY = MatX<T, QuadDim, QuadDim>,
			 typename std::enable_if<OpsCondition<QuadDim>::value, int>::type = 17>
	static MatY rotate (VecX<T, QuadDim> const& angle)
	{
		MatY mat;
		::vtf::rotate<MatY>(mat, angle);
		return mat;
	}
	template<size_t QuadDim, class MatY = MatX<T, QuadDim, QuadDim>,
			 typename std::enable_if<OpsCondition<QuadDim>::value, int>::type = 19>
	static MatY scale (const VecX<T, QuadDim> & s)
	{
		MatY mat;
		::vtf::scale<MatY>(mat, s);
		return mat;
	}
	bool isZero () const
	{
		for (size_t row = 0; row < Mrows; ++row)
		for (size_t col = 0; col < Ncols; ++col)
		{
			if (data[col][row] != T(0))
				return false;
		}
		return true;
	}
	bool isDiagonal () const
	{
		if (Ncols == Mrows)
		{
			MatX const& me = *this;
			for (size_t row = 0; row < Mrows; ++row)
			for (size_t col = 0; col < Ncols; ++col)
			{
				if ((col == row) ? me[col][row] == T(0) : me[col][row] != T(0))
					return false;
			}
		}
		return true;
	}
	static MatX diagonal (T value = T(1))
	{
		MatX u;
		for (size_t c = 0; c < Ncols && c < Mrows; ++c)
			u[c][c] = value;
		return u;
	}
	static MatX sequential (T startValue = T(1))
	{
		MatX u;
		for (size_t row = 0; row < Mrows; ++row)
		for (size_t col = 0; col < Ncols; ++col)
			u[col][row] = startValue + T(row * Ncols + col);
		return u;
	}
	friend std::ostream& operator<< (std::ostream& str, MatX const& mat)
	{
		str << '\n';
		for (size_t row = 0; row < Mrows; ++row)
		{
			str << "| ";
			for (size_t col = 0; col < Ncols; ++col)
			{
				if (col) str << ", ";
				str << mat[col][row];
			}
			str << " |\n";
		}
		return str;
	}
};

typedef MatX<float, 4, 1> Mat4x1;
typedef MatX<float, 4, 2> Mat4x2;
typedef MatX<float, 4, 3> Mat4x3;
typedef MatX<float, 4, 4> Mat4x4;

typedef MatX<float, 3, 1> Mat3x1;
typedef MatX<float, 3, 2> Mat3x2;
typedef MatX<float, 3, 3> Mat3x3;
typedef MatX<float, 3, 4> Mat3x4;

typedef MatX<float, 2, 1> Mat2x1;
typedef MatX<float, 2, 2> Mat2x2;
typedef MatX<float, 2, 3> Mat2x3;
typedef MatX<float, 2, 4> Mat2x4;

typedef MatX<float, 1, 1> Mat1x1;
typedef MatX<float, 1, 2> Mat1x2;
typedef MatX<float, 1, 3> Mat1x3;
typedef MatX<float, 1, 4> Mat1x4;

typedef Mat4x4 Mat4;
typedef Mat3x3 Mat3;
typedef Mat2x2 Mat2;
typedef Mat1x1 Mat1;

static_assert(sizeof(Mat1x1) == size_t(Mat1::Enums::ColCount) * size_t(Mat1::Enums::RowCount) * sizeof(Mat1x1::CellType), "");
static_assert(sizeof(Mat2x2) == size_t(Mat2::Enums::ColCount) * size_t(Mat2::Enums::RowCount) * sizeof(Mat2x2::CellType), "");
static_assert(sizeof(Mat3x3) == size_t(Mat3::Enums::ColCount) * size_t(Mat3::Enums::RowCount) * sizeof(Mat3x3::CellType), "");
static_assert(sizeof(Mat4x4) == size_t(Mat4::Enums::ColCount) * size_t(Mat4::Enums::RowCount) * sizeof(Mat4x4::CellType), "");

} // namespace vtf

#endif // __VTF_MATRIX_HPP_INCLUDED__
