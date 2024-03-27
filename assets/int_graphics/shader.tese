#version 450

#extension GL_EXT_tessellation_shader : require

layout(triangles, cw) in;

void main()
{
	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y;
	float w = gl_TessCoord.z;
	vec4 pos0 = gl_in[0].gl_Position;
	vec4 pos1 = gl_in[1].gl_Position;
	vec4 pos2 = gl_in[2].gl_Position;
	gl_Position = u * pos0 + v * pos1 + w * pos2;
}
