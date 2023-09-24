#version 450

#extension GL_EXT_device_group : require

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 in_clr;
layout(location = 0) out vec3 out_clr;
layout(binding = 0) readonly buffer Binding0Data
{
	mat4 rotateMatrix[32];
};
layout(binding = 1) readonly buffer Binding1Data
{
	mat4 translateMatrix, scaleMatrix;
};

void main()
{
	gl_Position = rotateMatrix[gl_DeviceIndex] * translateMatrix * scaleMatrix * vec4(pos, 0, 1);
	out_clr = in_clr;
}
