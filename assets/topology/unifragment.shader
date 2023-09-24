#version 460

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
layout(location = 0) in flat int instanceIndex;
layout(location = 0) out vec4 attachment;

void main()
{
	attachment = caps[instanceIndex].color;
}
