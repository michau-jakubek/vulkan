#version 450

layout(r32ui, set = 0, binding = 4) uniform uimage2D inStorageImage;
layout(location = 0) out uint color;

void main()
{
	color = imageLoad(inStorageImage, ivec2(gl_FragCoord)).x;
}

