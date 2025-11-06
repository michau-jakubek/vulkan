#version 450

layout(push_constant) uniform PC { float clear; } pc;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput depthInput;
layout(location = 0) out vec4 color;

void main()
{
	float depth = subpassLoad(depthInput).x;
	if (depth > pc.clear)
		color = vec4(1,0,0,1);
	else color = vec4(0,0,0,1);
}

