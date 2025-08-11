#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 inCoords;
layout(location = 0) out vec2 outCoords;

void main()
{
    vec2 offset = vec2((gl_VertexIndex & 1) == 0 ? -1.0 : 1.0, (gl_VertexIndex & 2) == 0 ? -1.0 : 1.0);
	gl_Position = vec4(offset * 0.5, 0.0, 1.0);
	outCoords = inCoords;
}

