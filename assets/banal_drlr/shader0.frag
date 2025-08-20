#version 450

layout(location = 0) in vec2 coords;
layout(location = 0) out vec4 red;
layout(location = 2) out vec4 green;
layout(location = 3) out vec4 blue;

void main()
{
	red   = vec4(1, 0, 0, 1);
	green = vec4(0, 1, 0, 1);
	blue  = vec4(0, 0, 1, 1);
}

