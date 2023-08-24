#version 450
#extension GL_GOOGLE_include_directive : require
#include "utils.shader"

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
};

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 outNormal;

void main()
{
	gl_Position	= vec4((pos.x * zoom), (pos.y * zoom), 0, 1);
	outNormal	= rotation3dY(angleY) * rotation3dX(angleX) * inNormal;
}
