#version 460 core

layout(location = 0) in vec2 coords;
layout(location = 1) in flat uint model;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform InData1 { uint inData1[64]; };
layout(binding = 1) uniform texture2D sampledImage;
layout(binding = 2) buffer OutData1 { uint outData1[64]; };
layout(binding = 3) uniform sampler2D imageAndSampler;
layout(binding = 4) buffer OutData2 { uint outData2[64]; };
layout(binding = 5) uniform sampler onlySampler;
layout(binding = 6) uniform InData2 { uint inData2[64]; };

layout(set = 0, binding = 7, r32ui) uniform writeonly uimage2D storageImage1;
layout(set = 1, binding = 0, r32ui) uniform writeonly uimage2D storageImage2;

void main()
{
    if (inData2[2] - 12 == inData1[2]
     && inData2[1] - 12 == inData1[1]
     && inData2[0] - 12 == inData1[0])
	{
        if (model == 0)
            color = texture(imageAndSampler, coords);
        else
            color = texture(sampler2D(sampledImage, onlySampler), coords);
    }
    else color = vec4(1);

    //if (int(gl_FragCoord.x) == 0 && int(gl_FragCoord.y) == 0)
    //{
        imageStore(storageImage1, ivec2(0,1), uvec4(200));
        imageStore(storageImage2, ivec2(1,2), uvec4(inData2[2]));
    //}
}

