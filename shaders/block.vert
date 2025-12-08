#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aNormal;
layout(location = 2) in uint aMaterial;
layout(location = 3) in uint aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
flat out uint vMaterial;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = uProjection * uView * worldPos;
    
    // Unpack normal
    vec3 normal = vec3(0.0);
    if (aNormal == 0u) normal = vec3(1.0, 0.0, 0.0);
    else if (aNormal == 1u) normal = vec3(-1.0, 0.0, 0.0);
    else if (aNormal == 2u) normal = vec3(0.0, 1.0, 0.0);
    else if (aNormal == 3u) normal = vec3(0.0, -1.0, 0.0);
    else if (aNormal == 4u) normal = vec3(0.0, 0.0, 1.0);
    else if (aNormal == 5u) normal = vec3(0.0, 0.0, -1.0);
    
    vNormal = normalize((uModel * vec4(normal, 0.0)).xyz);
    
    // Unpack UV
    float u = float((aUV >> 8u) & 0xFFu) / 255.0;
    float v = float(aUV & 0xFFu) / 255.0;
    vTexCoord = vec2(u, v);
    
    vMaterial = aMaterial;
}
