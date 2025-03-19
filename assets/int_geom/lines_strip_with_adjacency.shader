#version 450

layout(lines_adjacency) in;
layout(line_strip, max_vertices = 3) out;

void main()
{
	vec4 a_beg = gl_in[0].gl_Position;
    vec4 l_beg = gl_in[1].gl_Position;
	vec4 l_end = gl_in[2].gl_Position;
	vec4 a_end = gl_in[3].gl_Position;

	gl_Position = a_beg;
	EmitVertex();

	gl_Position = l_beg;
	EmitVertex();

	gl_Position = l_end;
	EmitVertex();

	EndPrimitive();
}

