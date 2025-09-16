#version 450

layout(push_constant) uniform PC { uint width; };
layout(location = 0) out uvec4 color0;
layout(location = 1) out uvec4 color1;

void main()
{
	color0 = uvec4(width + 1);
	color1 = uvec4(width + 2);
}

