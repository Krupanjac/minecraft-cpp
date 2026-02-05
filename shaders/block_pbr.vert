#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aNormal;
layout(location = 2) in uint aMaterial;
layout(location = 3) in uint aUV;
layout(location = 4) in uint aAO;
layout(location = 5) in uint aData;  // Sky light for solid blocks, water flags for water

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
flat out int vFace;
out float vAO;
out float vSkyLight;  // Sky light level (0.0 = underground, 1.0 = full sky access)
out vec4 vFragPosLightSpace;
out vec4 vCurrentClip;
out vec4 vPrevClip;
out vec3 vTangent;
out vec3 vBitangent;

const int kBlockTypeCount = 117;

// Texture atlas indices for non-PBR mode (per block type, per face)
uniform int uAtlasIndices[kBlockTypeCount * 6];


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
    
    int faceIdx = int(aNormal);
    vFace = faceIdx;

    if (uUsePBRResourcePack == 1) {
        vCellOrigin = vec2(0.0);
        
    } else {
        // Original atlas-based texture mapping
        float atlasSize = 16.0;
        float cellSize = 1.0 / atlasSize;
        int atlasLookup = int(aMaterial) * 6 + faceIdx;
        int textureIndex = 0;
        if (atlasLookup >= 0 && atlasLookup < (kBlockTypeCount * 6)) {
            textureIndex = uAtlasIndices[atlasLookup];
        }
        if (textureIndex < 0) textureIndex = 0;

        float col = float(textureIndex % 16);
        float row = float(textureIndex / 16);
        row = 15.0 - row;
        vCellOrigin = vec2(col * cellSize, row * cellSize);
    }
    
    vAO = float(aAO) / 3.0;
    
    // Extract sky light from data field (lower 4 bits for non-water blocks)
    vSkyLight = float(aData & 0xFu) / 15.0;
}
