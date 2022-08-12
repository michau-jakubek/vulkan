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
	int   iterCount;
	int   mode;
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

float fadd(in float a_man,	in int a_exp,
		   in float b_man,	in int b_exp,	out int exp, bool mode)
{
if (mode)
{
	float a = ldexp(a_man, a_exp);
	float b = ldexp(b_man, b_exp);
	return frexp(a+b, exp);
} else {
	if (a_exp >= b_exp)
	{
		exp = b_exp;
		return ldexp(a_man, (a_exp - b_exp)) + b_man;
	}
	else
	{
		exp = a_exp;
		return ldexp(b_man, (b_exp - a_exp)) + a_man;
	}
}
}

float fsub(in float a_man,	in int a_exp,
		   in float b_man,	in int b_exp,	out int exp, bool mode)
{
if (mode) {
	float a = ldexp(a_man, a_exp);
	float b = ldexp(b_man, b_exp);
	return frexp(a-b, exp);
} else {
	if (a_exp >= b_exp)
	{
		exp = b_exp;
		return ldexp(a_man, (a_exp - b_exp)) - b_man;
	}
	else
	{
		exp = a_exp;
		return a_man - ldexp(b_man, (b_exp - a_exp));
	}
}
}

float fmul(in float a_man,	in int a_exp,
		   in float b_man,	in int b_exp,	out int exp, bool mode)
{
if (mode) {
	float a = ldexp(a_man, a_exp);
	float b = ldexp(b_man, b_exp);
	exp = 0;
	//return 0;
	return frexp(a*b, exp);
} else {
	int t_exp;
	float t_man = frexp( (a_man * b_man), t_exp );
	exp = a_exp + b_exp + t_exp;
	return t_man;
}
}

float fmul2(in float t_man, in int t_exp, out int exp, bool mode)
{
if (mode) {
	float t = ldexp(t_man, t_exp);
	return frexp(t*t, exp);
} else {
	int u_exp;
	float u_man = frexp( t_man*t_man, u_exp );
	exp = u_exp + 2 * t_exp;
	return u_man;
}
}

float calc_re(in float x0_man,	in int x0_exp,
			  in float y0_man,	in int y0_exp,
			  in float x_man,	in int x_exp,	out int re_exp, bool mode)
{
	//float re = (x0 * x0 - y0 * y0) + x;
	int x0_2_exp, y0_2_exp, x0_2_sub_y0_2_exp;
	float x0_2_man = fmul2(x0_man, x0_exp, x0_2_exp, mode);
	float y0_2_man = fmul2(y0_man, y0_exp, y0_2_exp, mode);
	float x0_2_sub_y0_2_man = fsub(x0_2_man, x0_2_exp, y0_2_man, y0_2_exp, x0_2_sub_y0_2_exp, mode);
	return fadd(x0_2_sub_y0_2_man, x0_2_sub_y0_2_exp, x_man, x_exp, re_exp, mode);
}

float calc_im(in float x0_man,	in int x0_exp,
			  in float y0_man,	in int y0_exp,
			  in float y_man,	in int y_exp,	out int im_exp, bool mode)
{
	//float im = (2.0 * x0 * y0) + y;
	int x0_mul_y0_exp, x0_mul_y0_mul_2_exp;
	float x0_mul_y0_man = fmul(x0_man, x0_exp, y0_man, y0_exp, x0_mul_y0_exp, mode);
	float x0_mul_y0_mul_2_man = fadd(x0_mul_y0_man, x0_mul_y0_exp, x0_mul_y0_man, x0_mul_y0_exp, x0_mul_y0_mul_2_exp, mode);
	return fadd(x0_mul_y0_mul_2_man, x0_mul_y0_mul_2_exp, y_man, y_exp, im_exp, mode);
}

bool f_less_equal(float t_man, int t_exp, float v)
{
	int v_exp;
	float v_man = frexp(v, v_exp);
	return v_exp == t_exp && t_man <= v_man;
}

bool f_greater_equal(float t_man, int t_exp, float v, bool mode)
{
	int v_exp;
	float v_man = frexp(v, v_exp);
	return v_exp == t_exp && t_man >= v_man;
}

int iter(float x0_man, int x0_exp, float y0_man, int y0_exp,
		 float x_man, int x_exp, float y_man, int y_exp, bool mode)
{
	int re_exp, im_exp, re2_exp, im2_exp, re2_sum_im2_exp;
	for (int i = 0; i < pc.iterCount; ++i)
	{
		float re_man = calc_re(x0_man, x0_exp, y0_man, y0_exp, x_man, x_exp, re_exp, mode);
		float im_man = calc_im(x0_man, x0_exp, y0_man, y0_exp, y_man, y_exp, im_exp, mode);
		float re2_man = fmul2(re_man, re_exp, re2_exp, mode);
		float im2_man = fmul2(im_man, im_exp, im2_exp, mode);
		float re2_sum_im2_man = fadd(re2_man, re2_exp, im2_man, im2_exp, re2_sum_im2_exp, mode);
		if (f_greater_equal(re2_sum_im2_man, re2_sum_im2_exp, 4.0, mode)) return i;
		x0_man = re_man;
		x0_exp = re_exp;
		y0_man = im_man;
		y0_exp = im_exp;
	}
	return pc.iterCount;
}

int serie(in float x0, in float y0, in float x, in float y)
{
	for (int i = 0; i < pc.iterCount; ++i)
	{
		// (a + bi)(c + di) = (ac  - bd)  + (bc  + ad)i;
		// (a + bi)(a + bi) = (a^2 - b^2) + 2abi;

		float re = (x0 * x0 - y0 * y0) + x;
		float im = (2.0 * x0 * y0) + y;
		//if (length(vec2(re, im)) >= 2.0) return i;
		if ((re*re + im*im) >= 4.0) return i;
		x0 = re;
		y0 = im;
	}
	return pc.iterCount;
}

void main()
{
	int x_exp, y_exp;
	const float x = pc.xMin + (gl_FragCoord.x / pc.width) * (pc.xMax - pc.xMin);
	const float y = pc.yMin + ((pc.height - gl_FragCoord.y) / pc.height) * (pc.yMax - pc.yMin);
	float x_man = frexp(x, x_exp);
	float y_man = frexp(y, y_exp);
	int s = pc.mode == 0
			? serie(0, 0, x, y)
			: iter(0.0, 0, 0.0, 0, x_man, x_exp, y_man, y_exp, pc.mode==2);
	const float c = float(s) / float(pc.iterCount);
	const float r = red(c);
	const float g = green(c);
	const float b = blue(c);
	color = vec4(r, g, b, 1.0);
}


void main2()
{
	const float x = pc.xMin + (gl_FragCoord.x / pc.width) * (pc.xMax - pc.xMin);
	const float y = pc.yMin + ((pc.height - gl_FragCoord.y) / pc.height) * (pc.yMax - pc.yMin);
	const float c = float(serie(0.0, 0.0, x, y)) / float(pc.iterCount);
	const float r = red(c);
	const float g = green(c);
	const float b = blue(c);
	color = vec4(r, g, b, 1.0);
}
