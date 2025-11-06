#version 450

layout(location = 0) in vec3 pos;
layout(set = 0, binding = 2) uniform MVP { mat4 model; };

void main()
{
	gl_Position = model * vec4(pos, 1);
}

