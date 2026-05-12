#version 450
// Writes
layout(location = 0) out uint color0;
layout(location = 1) out uint color1;
layout(location = 2) out uint color2;
layout(location = 3) out uint color3;
layout(push_constant) uniform PC { uint width; };
void main() {
  uvec2 k = uvec2(gl_FragCoord);
  color0 = k.y * width + k.x + 100;
  color1 = k.y * width + k.x + 200;
  color2 = k.y * width + k.x + 300;
  color3 = k.y * width + k.x + 400;
}
