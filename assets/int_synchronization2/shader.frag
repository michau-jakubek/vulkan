#version 450

layout(r32ui, set = 0, binding = 3) uniform uimage2D inImage;
layout(location = 0) out uint color;

void main()
{
    ivec2 width = imageSize(inImage);
	color = imageLoad(inImage, ivec2(gl_FragCoord)).x;
}

