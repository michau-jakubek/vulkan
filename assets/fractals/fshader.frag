#version 450

layout(location = 0) out vec4 color;
layout(std430, push_constant) uniform PC
{
	float width;
	float height;
	float xMin;
	float xMax;
	float yMin;
	float yMax;
	float xSeed;
	float ySeed;
	int   iterCount;
	uint  method;
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

int fserie(in float x0, in float y0, in float x, in float y)
{
	for (int i = 0; i < pc.iterCount; ++i)
	{
		// Z^2 = (a + bi)(a + bi) = (a^2 - b^2) + 2abi;
		float re = (x0 * x0 - y0 * y0) + x;
		float im = (2.0 * x0 * y0) + y;
		if ((re*re + im*im) >= 4.0) return i;
		x0 = re;
		y0 = im;
	}
	return pc.iterCount;
}

void main()
{
	const float x = pc.xMin + (gl_FragCoord.x / pc.width) * (pc.xMax - pc.xMin);
	const float y = pc.yMin + ((pc.height - gl_FragCoord.y) / pc.height) * (pc.yMax - pc.yMin);
	const int s = (pc.method == 0) ? fserie(pc.xSeed, pc.ySeed, x, y) : fserie(x, y, pc.xSeed, pc.ySeed);
	const float c = float(s) / float(pc.iterCount);
	const float r = red(c);
	const float g = green(c);
	const float b = blue(c);
	color = vec4(r, g, b, 1.0);
}
