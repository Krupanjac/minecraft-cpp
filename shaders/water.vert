#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aNormal;
layout(location = 2) in uint aMaterial;
layout(location = 3) in uint aUV;
layout(location = 4) in uint aAO;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
flat out uint vMaterial;
out float vAO;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    
    // Wave effect
    // Only apply to top vertices of water? Or all?
    // If we apply to all, the whole block moves.
    // Ideally only top face moves.
    // But for simplicity, let's just move Y slightly based on X and Z.
    
    float wave = sin(worldPos.x * 2.0 + uTime) * 0.05 + cos(worldPos.z * 1.5 + uTime) * 0.05;
    // Only apply wave if normal is pointing up?
    // if (aNormal == 2u) 
    worldPos.y += wave * 0.5; // Reduce amplitude
    
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
    
    // Atlas mapping for water
    float atlasSize = 16.0;
    float cellSize = 1.0 / atlasSize;
    
    uint textureIndex = 205u; // Default water index (often around here in standard atlas)
    
    float col = float(textureIndex % 16u);
    float row = float(textureIndex / 16u);
    row = 15.0 - row;
    
    vTexCoord = vec2((col + u) * cellSize, (row + v) * cellSize);
    vTexCoord = vec2(u, v);
    
    vMaterial = aMaterial;
    vAO = float(aAO) / 3.0;
}
