#ifndef __VTF_FLOAT16_HPP_INCLUDED__
#define __VTF_FLOAT16_HPP_INCLUDED__

#include <cstdint>

namespace vtf
{

class Float16;

float float16ToFloat32 (const Float16&);
Float16 float32ToFloat16 (float f32);

#ifdef _MSC_VER
// warning C4324: '': structure was padded due to alignment specifier
#pragma warning(disable: 4324)
#endif

class Float16
{
	uint16_t m_data;
	Float16(uint16_t data) : m_data(data) {}
public:
	Float16 () : m_data{} {}
	Float16 (float f32);

	auto getData () const -> uint16_t { return m_data; }
	Float16 setData (uint16_t data) { m_data = data; return *this; }
	static Float16 construct (uint16_t data) { return Float16(data); }

	Float16& operator=(float f32) {
		m_data = float32ToFloat16(f32).getData();
		return *this;
	}

	operator float() const {
		return float16ToFloat32(m_data);
	}
};

struct BrainFloat16
{
	BrainFloat16() {}
	BrainFloat16(float, int = 0) {}
	float asFloat() const {
		return 0.0f;
	}
};

#ifdef _MSC_VER
// warning C4324: '': structure was padded due to alignment specifier
#pragma warning(default: 4324)
#endif

} // namespace vtf

#endif // __VTF_FLOAT16_HPP_INCLUDED__
