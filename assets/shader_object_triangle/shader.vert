#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 in_clr;
layout(location = 0) out vec3 out_clr;
layout(binding = 0, set = 0) buffer inOutBuffer { uint a[]; };

void main()
{
	if (gl_VertexIndex == 0) a[0] = 123;
	gl_Position = vec4(pos, 0, 1);
	out_clr = in_clr;
}
