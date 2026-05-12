#version 450

void main()
{
    vec2 offset = vec2((gl_VertexIndex & 1) == 0 ? -1.0 : 1.0, (gl_VertexIndex & 2) == 0 ? -1.0 : 1.0);
	gl_Position = vec4(offset * 0.5, 0.0, 1.0);
}

