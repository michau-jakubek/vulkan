#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform usampler2D heapImage;

void main()
{
	const ivec2 size  = textureSize(heapImage, 0);
	const ivec2 coord = clamp(ivec2(uv * vec2(size)), ivec2(0), size - ivec2(1));
	const uint  v     = texelFetch(heapImage, coord, 0).r;
	outColor = vec4(float(v % 64u) / 63.0, float(v % 16u) / 15.0, 0.25, 1.0);
}
