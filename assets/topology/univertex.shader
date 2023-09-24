#version 450

struct CAPS
{
	vec4  color;
	float pointSize;
};
layout(binding = 0) uniform PC
{
	uint  x;
	mat4  offset;
	mat4  scale;
	CAPS  caps[3];
};
layout(location = 0) in vec2 pos;
layout(location = 0) out flat int instanceIndex;

void main()
{
	gl_Position = scale * offset * vec4(pos, 0, 1) * vec4(2,2,0,1) - vec4(1,1,0,0);
	gl_PointSize = caps[gl_InstanceIndex].pointSize;
	instanceIndex = gl_InstanceIndex;
}
