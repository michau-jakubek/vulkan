#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 pl;

void main()
{
    pl = vec3(pl.x, 14, pl.y);
}

