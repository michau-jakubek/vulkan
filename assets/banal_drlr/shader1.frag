#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor0;
layout(input_attachment_index = 2, binding = 1) uniform subpassInput inColor2;
layout(input_attachment_index = 3, binding = 2) uniform subpassInput inColor3;

layout(location = 0) in vec2 coords;
layout(location = 0) out vec4 red;
layout(location = 2) out vec4 green;
layout(location = 3) out vec4 blue;

void main()
{
	vec4 c0 = subpassLoad(inColor0);
	vec4 c2 = subpassLoad(inColor2);
	vec4 c3 = subpassLoad(inColor3);

	red		= (c2 + c3) ; //* c0.r;
	green	= (c0 + c3) ; //* c2.g;
	blue	= (c0 + c2) ; //* c3.b;
}

