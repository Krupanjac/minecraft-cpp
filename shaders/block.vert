#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aNormal;
layout(location = 2) in uint aMaterial;
layout(location = 3) in uint aUV;
layout(location = 4) in uint aAO;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
flat out vec2 vCellOrigin;
flat out uint vMaterial;
out float vAO;

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
    
    // Unpack UV (now contains block dimensions for tiling)
    float u_dim = float((aUV >> 8u) & 0xFFu);
    float v_dim = float(aUV & 0xFFu);
    
    // Pass local UV for tiling (0..w, 0..h)
    vTexCoord = vec2(u_dim, v_dim);
    
    // Fix rotation for X-faces (Normal 0 and 1)
    // For X-faces, u_dim corresponds to Y (height) and v_dim to Z (width).
    // We want U to be horizontal (Z) and V to be vertical (Y).
    // So we swap them.
    if (aNormal == 0u || aNormal == 1u) {
        vTexCoord = vec2(v_dim, u_dim);
    }
    
    // Texture Atlas Mapping
    // Assume 16x16 atlas
    float atlasSize = 16.0;
    float cellSize = 1.0 / atlasSize;
    
    // Map material to texture index
    uint textureIndex = aMaterial - 1u; // Default mapping
    
    if (aMaterial == 1u) { // Grass
        if (aNormal == 2u) textureIndex = 0u;      // Top
        else if (aNormal == 3u) textureIndex = 2u; // Bottom (Dirt)
        else textureIndex = 3u;                    // Side
    }
    else if (aMaterial == 2u) textureIndex = 2u;   // Dirt
    else if (aMaterial == 3u) textureIndex = 1u;   // Stone
    else if (aMaterial == 4u) textureIndex = 18u;  // Sand
    else if (aMaterial == 6u) textureIndex = 4u;   // Wood (Plank)
    else if (aMaterial == 7u) textureIndex = 52u;  // Leaves
    else if (aMaterial == 12u) { // Log
        if (aNormal == 2u || aNormal == 3u) textureIndex = 21u; // Top/Bottom
        else textureIndex = 20u; // Side
    }
    else if (aMaterial == 13u) textureIndex = 39u; // Tall Grass
    else if (aMaterial == 14u) textureIndex = 12u; // Rose
    
    float col = float(textureIndex % 16u);
    float row = float(textureIndex / 16u);
    
    // Invert row because index 0 is usually top-left, but OpenGL 0 is bottom
    row = 15.0 - row;
    
    // Pass the origin of the texture cell in the atlas
    vCellOrigin = vec2(col * cellSize, row * cellSize);
    
    vMaterial = aMaterial;
    vAO = float(aAO) / 3.0;
}
