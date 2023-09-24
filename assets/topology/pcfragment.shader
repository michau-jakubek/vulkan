#version 460

layout(push_constant) uniform PC
{
	mat4  offset;
	mat4  scale;
	vec4  color;
	float pointSize;
};
layout(location = 0) out vec4 attachment;

void main()
{
	attachment = color;
}
