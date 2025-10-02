#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 color;

//layout(xfb_buffer = 0, xfb_stride = 20) out;

layout(location = 0, xfb_buffer = 0, xfb_offset = 0) out vec2 outPos;

void main()
{
	gl_Position = vec4(pos, 0, 1);
	outPos = pos;
}

