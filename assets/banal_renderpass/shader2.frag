#version 450

layout(input_attachment_index = 0, binding = 0) uniform usubpassInput inColor0;
layout(input_attachment_index = 1, binding = 1) uniform usubpassInput inColor1;
layout(binding = 2) buffer OutBuffer0 { uint outData0[]; };
layout(binding = 3) buffer OutBuffer1 { uint outData1[]; };
layout(push_constant) uniform PC { uint width; };

void main()
{
	ivec2 xy = ivec2(gl_FragCoord.xy);
	uvec4 v0 = (subpassLoad(inColor0));
	uvec4 v1 = (subpassLoad(inColor1));
	outData0[xy.y * width + xy.x] = v0.x + 4;
	outData1[xy.y * width + xy.x] = v1.x + 5;
}

