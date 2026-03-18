#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(5.0, 5.0, 10.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    
	lightDir = normalize(vec3(0.0, 0.0, 1.0));
    norm = normalize(fragNormal);

    // Simple diffusion
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Basic Suzanne kolor from Blender gray/beige
    vec3 objectColor = vec3(0.7, 0.6, 0.5);
    objectColor = vec3(0.8, 0.7, 0.6);

    // Ambient 0.2 - avoid black holes
    vec3 result = (0.2 + diffuse) * objectColor; 
	result = (ambient + diffuse) * objectColor;
    outColor = vec4(result, 1.0);
}

