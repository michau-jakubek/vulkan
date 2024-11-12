#version 450

layout(push_constant) uniform PC
{
	vec4 fColor0;
	vec4 fColor1;
	uvec4 uColor0;
	uvec4 uColor1;
};
layout(location = 0) out uvec4 outColor;

void main()
{
	outColor = uColor0;
}

