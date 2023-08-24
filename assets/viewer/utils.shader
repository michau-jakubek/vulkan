const float PI = atan(1.0) * 4.0;

mat3 rotation3dX (float angle) {
  float s = sin(angle * PI);
  float c = cos(angle * PI);

  return mat3(
	1.0, 0.0, 0.0,
	0.0,  c,  -s,
	0.0,  s,   c
  );
}

vec3 rotate3dX (vec3 point, float angle, vec3 center)
{
	return (rotation3dX(angle) * (point - center)) + center;
}

mat3 rotation3dY (float angle) {
  float s = sin(angle * PI);
  float c = cos(angle * PI);

  return mat3(
	 c,  0.0,  s,
	0.0, 1.0, 0.0,
	-s,  0.0,  c
  );
}

vec3 rotate3dY (vec3 point, float angle, vec3 center)
{
	return (rotation3dY(angle) * (point - center)) + center;
}

mat3 rotation3dZ (float angle) {
  float s = sin(angle * PI);
  float c = cos(angle * PI);

  return mat3(
	 c,  -s,  0.0,
	 s,   c,  0.0,
	0.0, 0.0, 1.0
  );
}

vec3 rotate3dZ (vec3 point, float angle, vec3 center)
{
	return (rotation3dZ(angle) * (point - center)) + center;
}

vec3 rotate3dAll (vec3 point, vec3 angle, vec3 center)
{
	return (rotation3dX(angle.x) * rotation3dY(angle.y) * rotation3dZ(angle.z) * (point - center)) + center;
}

vec3 normalize (vec3 a, vec3 b, float l)
{
	return normalize(b-a) * l + a;
}
