#version 450
layout(location = 0) in highp vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;
void main ()
{
    gl_PointSize = 1.0;
    const float yOffset = 0.0;
    gl_Position = vec4(in_position.x, in_position.y + yOffset, in_position.z, in_position.w);
    out_color = in_color;
}

