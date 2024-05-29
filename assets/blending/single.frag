#version 450

layout(push_constant) uniform PC
{
	vec4 background;
	vec4 inColor0;
	vec4 inColor1;
};
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = inColor0;
}

