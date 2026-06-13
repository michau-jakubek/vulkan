#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT uint value;

void main()
{
	if (value != 1u)
		value = 2u;
}
