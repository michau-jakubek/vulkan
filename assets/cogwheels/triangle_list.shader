#version 450

layout(triangles) in;
layout(location = 0) in vec4[] pos;
layout(triangle_strip, max_vertices = 9) out;

void main()
{
	vec4 a = gl_in[0].gl_Position;
    vec4 b = gl_in[1].gl_Position;
	vec4 c = gl_in[2].gl_Position;

	gl_Position = a;
	EmitVertex();

	gl_Position = b;
	EmitVertex();

	gl_Position = c;
	EmitVertex();

	EndPrimitive();


	
	gl_Position = a;
	EmitVertex();

	gl_Position = pos[0];
	EmitVertex();

	gl_Position = b;
	EmitVertex();

	gl_Position = pos[1];
	EmitVertex();

	gl_Position = c;
	EmitVertex();

	gl_Position = pos[2];
	EmitVertex();

	EndPrimitive();
}

