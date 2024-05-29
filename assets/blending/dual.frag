#version 450

layout(push_constant) uniform PC
{
	vec4 background;
	vec4 inColor0;
	vec4 inColor1;
};
layout(location = 0, index = 0) out vec4 outColor0;
layout(location = 0, index = 1) out vec4 outColor1;

void main()
{
	outColor0 = inColor0;
	outColor1 = inColor1;
}

