#version 450
// Writes and reads
layout(location = 0) out uvec4 color0;
layout(location = 1) out uvec4 color1;
layout(location = 2) out uvec4 color2;
layout(location = 3) out uvec4 color3;
layout(binding = 0, input_attachment_index = 0) uniform usubpassInput inputColor0;
layout(binding = 1, input_attachment_index = 1) uniform usubpassInput inputColor1;
layout(binding = 2, input_attachment_index = 2) uniform usubpassInput inputColor2;
layout(binding = 3, input_attachment_index = 3) uniform usubpassInput inputColor3;
layout(binding = 4) buffer InColor0 { uint inColor0[]; };
layout(binding = 5) buffer InColor1 { uint inColor1[]; };
layout(binding = 6) buffer InColor2 { uint inColor2[]; };
layout(binding = 7) buffer InColor3 { uint inColor3[]; };
layout(binding =  8) buffer OutColor0 { uint outColor0[]; };
layout(binding =  9) buffer OutColor1 { uint outColor1[]; };
layout(binding = 10) buffer OutColor2 { uint outColor2[]; };
layout(binding = 11) buffer OutColor3 { uint outColor3[]; };
layout(push_constant) uniform PC { uint width; };
void main() {
	const uint coord = uint(gl_FragCoord.y) * width + uint(gl_FragCoord);

    color0 = subpassLoad(inputColor0);
    inColor0[coord] = color0.x;
	color0.x += 1000;
	outColor0[coord] = color0.x;

    color1 = subpassLoad(inputColor1);
	inColor1[coord] = color1.x;
	color1.x += 2000;
	outColor1[coord] = color1.x;

	color2 = subpassLoad(inputColor2);
	inColor2[coord] = color2.x;
	color2.x += 3000;
	outColor2[coord] = color2.x;

	color3 = subpassLoad(inputColor3);
	inColor3[coord] = color3.x;
	color3.x += 4000;
	outColor3[coord] = color3.x;
}
