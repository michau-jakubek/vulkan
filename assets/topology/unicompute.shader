#version 450

struct CAPS
{
	vec4  color;
	float pointSize;
};
layout(binding = 0) uniform PC
{
	float  x;
	mat4  m1;
	mat4  m2;
};
layout(binding = 1) buffer XXX
{
	float xxx[100];
};
layout(local_size_x = 1) in;
void main()
{
	xxx[0] = x;
	xxx[1] = m1[0][0];
	xxx[2] = m2[0][0];
}
