#version 450

layout(local_size_x_id = 3, local_size_y_id = 4, local_size_z_id = 5) in;
layout(set = 0, binding = 2) buffer Buffer { uint x[]; };
layout(push_constant) uniform PC
{
	uvec4 v3;
	int   y;
	uint  a;
	uvec2 v2;
	uint  b;
	uint d1, d2, d3, d4;
};
layout(constant_id = 0) const int c0 = 789;
layout(constant_id = 1) const int c1 = 789;
layout(constant_id = 2) const int c2 = 789;

void main()
{
	if (gl_LocalInvocationIndex == 0)
	{
		x[0] = gl_WorkGroupSize.x;
		x[1] = gl_WorkGroupSize.y;
		x[2] = gl_WorkGroupSize.z;
		x[3] = c0;
		x[4] = c1;
		x[5] = c2;
		x[6] = 333;
		x[7] = v3.x;
		x[8] = v3.y;
		x[9] = v3.z;
		x[10] = y;
		x[11] = a;
		x[12] = v2.x;
		x[13] = v2.y;
		x[14] = b;
	}
}
