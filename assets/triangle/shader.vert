#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 in_clr;
layout(location = 0) out vec3 out_clr;

void main()
{
	gl_Position = vec4(pos, 0, 1);
	out_clr = in_clr;
}
