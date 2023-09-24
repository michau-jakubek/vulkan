#version 450

layout(push_constant) uniform PC
{
	mat4  offset;
	mat4  scale;
	vec4  color;
	float pointSize;
};
layout(location = 0) in vec2 pos;

void main()
{
	gl_Position = scale * offset * vec4(pos, 0, 1) * vec4(2,2,0,1) - vec4(1,1,0,0);
	gl_PointSize = pointSize;
}
