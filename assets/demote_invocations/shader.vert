#version 450

layout(location = 0) in vec2 pos;
layout(push_constant) uniform PC
{
	int		width;
	int		height;
	float	pointSize;
	int		primitiveStride;
	int		flags;
};

void main()
{
	gl_Position = vec4(pos, 0, 1);
	gl_PointSize = pointSize;
}
