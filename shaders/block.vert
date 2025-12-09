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
    
    // Unpack UV
    float u = float((aUV >> 8u) & 0xFFu) / 255.0;
    float v = float(aUV & 0xFFu) / 255.0;
    
    // Texture Atlas Mapping
    // Assume 16x16 atlas
    float atlasSize = 16.0;
    float cellSize = 1.0 / atlasSize;
    
    // Map material to texture index
    // This mapping depends on the specific layout of block_atlas.png
    // Standard Minecraft 1.12-ish terrain.png layout:
    // 0: Grass Top, 1: Stone, 2: Dirt, 3: Grass Side
    // 4: Wood Plank, 5: Stone Slab Side, 6: Stone Slab Top, 7: Brick
    // ...
    // We'll use a simplified mapping logic here.
    
    uint textureIndex = aMaterial - 1u; // Default mapping
    
    if (aMaterial == 1u) { // Grass
        if (aNormal == 2u) textureIndex = 0u;      // Top
        else if (aNormal == 3u) textureIndex = 2u; // Bottom (Dirt)
        else textureIndex = 3u;                    // Side
    }
    else if (aMaterial == 2u) textureIndex = 2u;   // Dirt
    else if (aMaterial == 3u) textureIndex = 1u;   // Stone
    else if (aMaterial == 4u) textureIndex = 11u;  // Sand (often at index 11 or 12)
    else if (aMaterial == 6u) textureIndex = 4u;   // Wood (Plank)
    else if (aMaterial == 7u) textureIndex = 52u;  // Leaves (often further down)
    
    // Calculate atlas coordinates
    // In OpenGL, (0,0) is bottom-left.
    // If the atlas is defined with (0,0) at top-left (standard image), we need to flip row.
    // But we flipped the texture on load (stbi_set_flip_vertically_on_load(1)).
    // So (0,0) is bottom-left of the image.
    // If the atlas indices start from top-left (0, 1, 2...), we need to invert row.
    
    float col = float(textureIndex % 16u);
    float row = float(textureIndex / 16u);
    
    // Invert row because index 0 is usually top-left, but OpenGL 0 is bottom
    row = 15.0 - row;
    
    vTexCoord = vec2((col + u) * cellSize, (row + v) * cellSize);
    
    vMaterial = aMaterial;
    vAO = float(aAO) / 3.0;
}
