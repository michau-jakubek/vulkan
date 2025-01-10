#version 450

layout(location = 0) in vec2 pos;
layout(location = 0) out flat uint instance;

void main()
{
	gl_Position = vec4(pos, 0, 1);
	instance = gl_InstanceIndex;
}

