#version 450

layout(local_size_x_id = 3, local_size_y_id = 4, local_size_z_id = 5) in;
layout(set = 0, binding = 1) buffer Buffer { uint k[]; };
layout(push_constant) uniform PC
{
	uvec4 v3;
	int   y;
	uint  a;
	uvec2 v2;
	uint  b;
	uint d1, d2, d3, d4;
};
layout(constant_id = 0) const int c0 = 456;
layout(constant_id = 1) const int c1 = 456;
layout(constant_id = 2) const int c2 = 456;

void main2()
{
	if (gl_LocalInvocationIndex == 0)
	{
		k[0] = gl_WorkGroupSize.x;
		k[1] = gl_WorkGroupSize.y;
		k[2] = gl_WorkGroupSize.z;
		k[3] = c0;
		k[4] = c1;
		k[5] = c2;
		k[6] = 222;
		k[7] = v3.x;
		k[8] = v3.y;
		k[9] = v3.z;
		k[10] = y;
		k[11] = a;
		k[12] = v2.x;
		k[13] = v2.y;
		k[14] = b;
	}
}
