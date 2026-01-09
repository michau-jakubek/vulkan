#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

void main()
{
    // gl_FrontFacingEXT powie nam, czy trafiliśmy w przód czy w tył
	// if (gl_HitKindEXT == gl_HitKindFrontFacingEXT)
	if (gl_HitKindEXT == 0xFE)
     {
        payload = vec3(11, 0.0, 0.0); // Zielony = Przód
    } else {
        payload = vec3(17, 0.0, 0.0); // Czerwony = Tył
    }

    //// payload = vec3(1.0, 0, 0); // white
}

