#version 450

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor0;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inColor1;
layout(binding = 2) buffer OutBuffer0 { uint outData0[]; };
layout(binding = 3) buffer OutBuffer1 { uint outData1[]; };
layout(push_constant) uniform PC { uint width; };

void main()
{
	uvec4 v0 = uvec4(subpassLoad(inColor0));
	uvec4 v1 = uvec4(subpassLoad(inColor1));
	uvec2 xy = uvec2(gl_FragCoord.xy);
	outData0[xy.y * width + xy.x] = v0.x + 1;
	outData1[xy.y * width + xy.x] = v1.x + 2;
}

