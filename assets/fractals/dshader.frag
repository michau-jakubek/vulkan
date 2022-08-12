#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_float64 : require

layout(location = 0) out vec4 color;
layout(std430, push_constant) uniform PC
{
	double	width;
	double	height;
	double	xMin;
	double	xMax;
	double	yMin;
	double	yMax;
	int		iterCount;
	int		mode;
} pc;

const float w1 = 50.0;
const float w2 = 50.0;
const float w3 = 50.0;

const float p1 = 2.2;
const float p2 = 2.2;
const float p3 = 2.2;

const float c1 = 0.25;
const float c2 = 0.50;
const float c3 = 0.75;

float red	(in float x) { return exp(-w1 * pow(abs(x-c1), p1)); }
float green	(in float x) { return exp(-w2 * pow(abs(x-c2), p2)); }
float blue	(in float x) { return exp(-w3 * pow(abs(x-c3), p3)); }

int dserie(in double x0, in double y0, in double x, in double y)
{
	for (int i = 0; i < pc.iterCount; ++i)
	{
		// Z^2 = (a + bi)(a + bi) = (a^2 - b^2) + 2abi;
		double re = (x0 * x0 - y0 * y0) + x;
		double im = (2.0 * x0 * y0) + y;
		if ((re*re + im*im) >= 4.0) return i;
		x0 = re;
		y0 = im;
	}
	return pc.iterCount;
}

void main()
{
	const double x = pc.xMin + (double(gl_FragCoord.x) / pc.width) * (pc.xMax - pc.xMin);
	const double y = pc.yMin + ((pc.height - double(gl_FragCoord.y)) / pc.height) * (pc.yMax - pc.yMin);
	const int s = dserie(0.0, 0.0, x, y);
	const float c = float(s) / float(pc.iterCount);
	const float r = red(c);
	const float g = green(c);
	const float b = blue(c);
	color = vec4(r, g, b, 1.0);
}
