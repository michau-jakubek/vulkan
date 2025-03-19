#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 inCoords;
layout(location = 0) out vec2 outCoords;
layout(location = 1) out uint model;
layout(binding = 0) uniform InData1 { uint inData1[64]; };
layout(binding = 6) uniform InData2 { uint inData2[64]; };

layout(set = 0, binding = 7, r32ui) uniform uimage2D storageImage1;
layout(set = 1, binding = 0, r32ui) uniform uimage2D storageImage2;

layout(set = 1, binding = 1) uniform Mats {
	mat4 model1, model2;
	mat4 view, projection;
	float rotationAngle;
};

void main()
{
	if (gl_InstanceIndex == 0)
		gl_Position = projection * view * model1 * vec4(pos, 1);
	else
		gl_Position = projection * view * model2 * vec4(pos, 1);

	outCoords = vec2(inCoords.x, 1 - inCoords.y);
	model = gl_InstanceIndex;

	if (gl_VertexIndex == 0)
	{
		imageStore(storageImage1, ivec2(0,1), uvec4(inData1[1]));
		imageStore(storageImage2, ivec2(1,1), uvec4(inData2[1]));
	}
}

