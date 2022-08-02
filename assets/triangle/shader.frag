#version 460

layout(location = 0) out vec4 attachment;
layout(location = 0) in vec3 color;

void main()
{
	attachment = vec4(color, 1);
}
