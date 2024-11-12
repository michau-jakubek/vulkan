#version 450

layout(push_constant) uniform PC
{
	vec4 fColor0;
	vec4 fColor1;
	uvec4 uColor0;
	uvec4 uColor1;
};
layout(location = 0, index = 0) out vec4 outColor0;
layout(location = 0, index = 1) out vec4 outColor1;

void main()
{
	outColor0 = fColor0;
	outColor1 = fColor1;
}

