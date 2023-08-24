#version 450

layout(std430, push_constant) uniform PC
{
	float centerX;
	float centerY;
	float centerZ;
	float angleX;
	float angleY;
	float angleZ;
	float radius;
	float zoom;
} pc;

layout(location = 0) out vec4 color;
layout(location = 0) in vec3 normal;
layout(set=0, binding=0) uniform samplerCube csam;

void main()
{
	color = texture(csam, normal);
}
