#version 450

layout(location = 0) in vec4 pos;
layout(location = 0) out vec4 outPos;
layout(set = 0, binding = 0) uniform
MVP { mat4 model1, model2, view, proj; } mvp;

void main()
{
	gl_PointSize = 1.0;
	if (gl_InstanceIndex == 0)
	{
		gl_Position = mvp.proj * mvp.view * mvp.model1 * pos;
		outPos = mvp.proj * mvp.view * mvp.model1 * vec4(pos.xy, 1, 1);
	}
	else
	{
		gl_Position = mvp.proj * mvp.view * mvp.model2 * pos;
		outPos = mvp.proj * mvp.view * mvp.model2 * vec4(pos.xy, 1, 1);
	}
}

