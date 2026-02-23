#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 pl;

void main()
{
    // gl_FrontFacingEXT powie nam, czy trafiliśmy w przód czy w tył
	// if (gl_HitKindEXT == gl_HitKindFrontFacingEXT)
	if (gl_HitKindEXT == 0xFE)
    {
        pl = vec3(11, pl.y, pl.z); // Zielony = Przód
    } else {
        pl = vec3(17, pl.y, pl.z); // Czerwony = Tył
    }

    //// payload = vec3(1.0, 0, 0); // white
}

