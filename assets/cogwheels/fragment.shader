#version 450

layout(location = 0) flat in uint inInstance;
layout(location = 1) in vec4 inPos;
layout(location = 2) in vec3 inNorm;
layout(location = 0) out vec4 outColor;

const vec4 colors[4] =
{
	vec4(1,1,0,1),
	vec4(0,1,0,1),
	vec4(1,1,1,1),
	vec4(1,0,1,1)
};
vec3 lightPos = vec3(1,10,10);
vec3 lightColor = vec3(2,2,2);
vec3 ambientColor = vec3(0.5,0.5,0.5);
vec3 viewPos = vec3(0.0, 0.0, 5.0);
vec3 ambient = lightColor * ambientColor;
void main()
{
	const vec3 fragNormal = vec3(inNorm);
	const vec3 fragPosition = vec3(inPos);

	vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = spec * lightColor;

	// kolor obiektu
    vec3 ccc = ambient * (diffuse + specular) * vec3(colors[inInstance % 4]);
	outColor = vec4(ccc, 1.0);

	//outColor = vec4(normalize(inNorm) * 0.5 + 0.5, 1.0);
}

