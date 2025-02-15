#include "vtfFloat16.hpp"

#include <cmath>

namespace vtf
{

template<class Y, class X>
Y bit_cast (const X& x)
{
    static_assert(sizeof(X) == sizeof(Y), "???");
    return *((Y*)(&x));
}

Float16::Float16 (float f32) : m_data(float32ToFloat16(f32).getData())
{
}

float float16ToFloat32(const Float16& f16)
{
    const uint16_t h = f16.getData();

    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exponent = (h & 0x7C00) >> 10;
    uint32_t mantissa = h & 0x03FF;

    if (exponent == 0) {
        if (mantissa == 0) {
            return bit_cast<float>(sign);
        }
        else {
            while ((mantissa & 0x0400) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            exponent++;
            mantissa &= ~0x0400u;
        }
    }
    else if (exponent == 0x1F) {
        if (mantissa == 0) {
            return bit_cast<float>(sign | 0x7F800000);
        }
        else {
            return bit_cast<float>(sign | 0x7F800000 | (mantissa << 13));
        }
    }

    exponent = exponent + (127 - 15);
    mantissa = mantissa << 13;

    uint32_t result = sign | (exponent << 23) | mantissa;
    return bit_cast<float>(result);
}

Float16 float32ToFloat16 (float f)
{
    uint32_t bits = bit_cast<uint32_t>(f);
    uint32_t sign = (bits & 0x80000000) >> 16;
    uint32_t exponent = (bits & 0x7F800000) >> 23;
    uint32_t mantissa = bits & 0x007FFFFF;

    if (exponent == 0xFF) {
        if (mantissa == 0) {
            return Float16::construct(uint16_t(sign | 0x7C00));
        }
        else {
            return Float16::construct(uint16_t(sign | 0x7C00 | (mantissa >> 13)));
        }
    }
    else if (exponent > 0x70) {
        return Float16::construct(uint16_t(sign | 0x7C00));
    }
    else if (exponent < 0x71) {
        return Float16::construct(uint16_t(sign));
    }

    exponent = exponent - (127 - 15);
    mantissa = mantissa >> 13;

    return Float16::construct(uint16_t(sign | (exponent << 10) | mantissa));
}

} // namespace vtf
