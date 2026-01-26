#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aNormal;
layout(location = 2) in uint aMaterial;
layout(location = 3) in uint aUV;
layout(location = 4) in uint aAO;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

// TAA / Velocity Buffer uniforms
uniform mat4 uPrevView;
uniform mat4 uPrevProjection;
uniform vec3 uOriginDelta;

// Resource pack mode
uniform int uUsePBRResourcePack;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
flat out vec2 vCellOrigin;
flat out uint vMaterial;
flat out int vTextureLayer;  // For texture array
out float vAO;
out vec4 vFragPosLightSpace;
out vec4 vCurrentClip;
out vec4 vPrevClip;
out vec3 vTangent;
out vec3 vBitangent;

// Texture layer indices for PBR resource pack (per block type, per face)
// These are set from CPU based on ResourcePackManager
uniform int uTextureIndices[16 * 6];  // 16 block types * 6 faces

// Vegetation variant indices for randomization
uniform int uGrassVariants[8];
uniform int uGrassVariantCount;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vFragPosLightSpace = uLightSpaceMatrix * worldPos;
    gl_Position = uProjection * uView * worldPos;
    
    // Velocity Calculation
    vCurrentClip = gl_Position;
    // Previous position relative to previous origin
    vec4 prevWorldPos = vec4(worldPos.xyz + uOriginDelta, 1.0);
    vPrevClip = uPrevProjection * uPrevView * prevWorldPos;
    
    // Unpack normal
    vec3 normal = vec3(0.0);
    if (aNormal == 0u) normal = vec3(1.0, 0.0, 0.0);
    else if (aNormal == 1u) normal = vec3(-1.0, 0.0, 0.0);
    else if (aNormal == 2u) normal = vec3(0.0, 1.0, 0.0);
    else if (aNormal == 3u) normal = vec3(0.0, -1.0, 0.0);
    else if (aNormal == 4u) normal = vec3(0.0, 0.0, 1.0);
    else if (aNormal == 5u) normal = vec3(0.0, 0.0, -1.0);
    
    vNormal = normalize((uModel * vec4(normal, 0.0)).xyz);
    
    // Calculate tangent and bitangent for normal mapping
    vec3 tangent, bitangent;
    if (abs(normal.y) > 0.9) {
        // Horizontal face (top/bottom)
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, normal.y > 0.0 ? 1.0 : -1.0);
    } else if (abs(normal.x) > 0.9) {
        // X-facing face
        tangent = vec3(0.0, 0.0, normal.x > 0.0 ? -1.0 : 1.0);
        bitangent = vec3(0.0, 1.0, 0.0);
    } else {
        // Z-facing face
        tangent = vec3(normal.z > 0.0 ? 1.0 : -1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 1.0, 0.0);
    }
    vTangent = normalize((uModel * vec4(tangent, 0.0)).xyz);
    vBitangent = normalize((uModel * vec4(bitangent, 0.0)).xyz);
    
    // Unpack UV (now contains block dimensions for tiling)
    float u_dim = float((aUV >> 8u) & 0xFFu);
    float v_dim = float(aUV & 0xFFu);
    
    // Pass local UV for tiling (0..w, 0..h)
    vTexCoord = vec2(u_dim, v_dim);
    
    // Fix rotation for X-faces (Normal 0 and 1)
    if (aNormal == 0u || aNormal == 1u) {
        vTexCoord = vec2(v_dim, u_dim);
    }
    
    vMaterial = aMaterial;
    
    if (uUsePBRResourcePack == 1) {
        // Use texture array with layer index from uniform
        // Material ID * 6 + face direction gives the index into uTextureIndices
        int faceIdx = int(aNormal);
        int lookupIdx = int(aMaterial) * 6 + faceIdx;
        if (lookupIdx < 96) {  // 16 * 6
            vTextureLayer = uTextureIndices[lookupIdx];
        } else {
            vTextureLayer = 0;
        }
        
        // Vegetation randomization based on world position
        // For tall grass (13) and flowers (14), use position-based hash to pick variant
        if (aMaterial == 13u || aMaterial == 14u) {
            // Create a hash from block position for consistent randomization
            ivec3 blockPos = ivec3(floor(worldPos.xyz));
            int hash = blockPos.x * 73856093 ^ blockPos.y * 19349663 ^ blockPos.z * 83492791;
            hash = abs(hash);
            
            // Pick from grass variants
            if (uGrassVariantCount > 0) {
                int variantIdx = hash % uGrassVariantCount;
                vTextureLayer = uGrassVariants[variantIdx];
            }
        }
        
        vCellOrigin = vec2(0.0);  // Not used in PBR mode
    } else {
        // Original atlas-based texture mapping
        vTextureLayer = -1;  // Signal to use atlas
        
        // Texture Atlas Mapping (16x16 atlas)
        float atlasSize = 16.0;
        float cellSize = 1.0 / atlasSize;
        
        uint textureIndex = aMaterial - 1u;
        
        if (aMaterial == 1u) { // Grass
            if (aNormal == 2u) textureIndex = 0u;
            else if (aNormal == 3u) textureIndex = 2u;
            else textureIndex = 3u;
        }
        else if (aMaterial == 2u) textureIndex = 2u;
        else if (aMaterial == 3u) textureIndex = 1u;
        else if (aMaterial == 4u) textureIndex = 18u;
        else if (aMaterial == 10u) textureIndex = 19u;
        else if (aMaterial == 6u) textureIndex = 4u;
        else if (aMaterial == 7u) textureIndex = 52u;
        else if (aMaterial == 12u) {
            if (aNormal == 2u || aNormal == 3u) textureIndex = 21u;
            else textureIndex = 20u;
        }
        else if (aMaterial == 8u) textureIndex = 240u;
        else if (aMaterial == 13u) textureIndex = 39u;
        else if (aMaterial == 14u) textureIndex = 12u;
        else if (aMaterial == 11u) textureIndex = 192u;
        
        float col = float(textureIndex % 16u);
        float row = float(textureIndex / 16u);
        row = 15.0 - row;
        
        vCellOrigin = vec2(col * cellSize, row * cellSize);
    }
    
    vAO = float(aAO) / 3.0;
}
