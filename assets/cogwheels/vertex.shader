#version 450

layout(location = 0) in vec4 pos;
layout(location = 1) in vec3 norm;
layout(location = 0) out uint outInstance;
layout(location = 1) out vec4 outPos;
layout(location = 2) out vec3 outNorm;
layout(set = 0, binding = 0) uniform
MVP { mat4 models[4], view, proj; } mvp;

void main()
{
	gl_PointSize = 1.0;
	outInstance = gl_InstanceIndex;
	const mat4 model = mvp.models[outInstance % 4];

	gl_Position = mvp.proj * mvp.view * model * pos;
	outPos = model * pos;
	//outNorm = normalize(transpose(inverse(mat3(model))) * norm);
	outNorm = normalize(mat3(model) * norm);
}

