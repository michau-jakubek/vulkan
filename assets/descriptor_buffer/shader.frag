#version 460

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
layout(binding = 7, r32ui) uniform uimage2D storageImage;

//layout(set=1, binding = 0, r32ui) uniform uimage2D storageImage2;

void main()
{
    if (model == 0)
         color = texture(imageAndSampler, coords);
    else
        color = texture(sampler2D(sampledImage, onlySampler), coords);

    //if (int(gl_FragCoord.x) == 0 && int(gl_FragCoord.y) == 0)
    {
        imageStore(storageImage, ivec2(0,2), uvec4(inData1[2]));
        imageStore(storageImage, ivec2(1,2), uvec4(inData2[2]));
    }
}

