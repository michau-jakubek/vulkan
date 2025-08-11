#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor0;
layout(input_attachment_index = 2, binding = 1) uniform subpassInput inColor2;

layout(location = 0) in vec2 coords;
layout(location = 0) out vec4 red;
layout(location = 2) out vec4 blue;

void main()
{
	red = subpassLoad(inColor0);
	blue = subpassLoad(inColor2);
}

