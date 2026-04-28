#version 450

layout(push_constant) uniform PC { vec2 iResolution; };
layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);

    // Centered, aspect-correct coordinates: y in [-1, 1]
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / min(iResolution.x, iResolution.y) * 1.5;

    vec3 bg = vec3(0.10, 0.10, 0.14);
    vec3 yellow = vec3(1.00, 0.85, 0.10);
    vec3 ink = vec3(0.08);

    vec3 col = bg;

    // Face
    float face = length(uv) - 0.6;
    col = mix(yellow, col, smoothstep(-fwidth(face), fwidth(face), face));

    // Face outline
    float outline = abs(face + 0.015) - 0.012;
    col = mix(ink, col, smoothstep(-fwidth(outline), fwidth(outline), outline));

    // Eyes
    float eyeL = length(uv - vec2(-0.20, 0.15)) - 0.055;
    float eyeR = length(uv - vec2(0.20, 0.15)) - 0.055;
    float eyes = min(eyeL, eyeR);
    col = mix(ink, col, smoothstep(-fwidth(eyes), fwidth(eyes), eyes));

    // Mouth: lower arc of a circle
    vec2 m = uv - vec2(0.0, -0.05);
    float arc = abs(length(m) - 0.30) - 0.025;    // keep only the lower half
    if (m.y > 0.0) arc = 1.0;
    col = mix(ink, col, smoothstep(-fwidth(arc), fwidth(arc), arc));

    fragColor = vec4(col, 1.0);
}

