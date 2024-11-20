#version 460

layout(location = 0) out vec4 attachment;
layout(location = 0) in vec3 color;
layout(binding = 0, set = 0) buffer inOutBuffer { uint a[]; };

void main()
{
	if (uvec3(gl_FragCoord) == uvec3(0)) a[1] = 456;
	attachment = vec4(color, 1);
}
