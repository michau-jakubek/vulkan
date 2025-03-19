#version 450

layout(location = 0) in vec4 pos;
layout(location = 0) out vec4 outPos;
layout(set = 0, binding = 0) uniform
MVP { mat4 model, view, proj; } mvp;

void main()
{
	gl_Position = mvp.proj * mvp.view * mvp.model * pos;
	outPos = mvp.proj * mvp.view * mvp.model * vec4(pos.xy, 1, 1);
}

