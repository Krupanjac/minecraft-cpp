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

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
flat out vec2 vCellOrigin;
flat out uint vMaterial;
out float vAO;
out float vSkyLight;  // Sky light level (0.0 = underground, 1.0 = full sky access)
out vec4 vFragPosLightSpace;
out vec4 vCurrentClip;
out vec4 vPrevClip;
flat out uint vFaceRot; // Face rotation for UV rotation in fragment shader

const int kBlockTypeCount = 117;
uniform int uAtlasIndices[kBlockTypeCount * 6];

// Face rotation remap table for Y-axis rotation of side faces
// Game normals: 0=X+, 1=X-, 2=Y+, 3=Y-, 4=Z+, 5=Z-
// For each rotation, maps rendered face -> source texture face (side faces only)
// rot=0: identity
// rot=1 (90° CW from above):  X+→Z+, X-→Z-, Z+→X-, Z-→X+
// rot=2 (180°):                X+→X-, X-→X+, Z+→Z-, Z-→Z+
// rot=3 (270° CW):            X+→Z-, X-→Z+, Z+→X+, Z-→X-
const int faceRemapTable[4][6] = int[4][6](
    int[6](0, 1, 2, 3, 4, 5), // rot=0: identity
    int[6](4, 5, 2, 3, 1, 0), // rot=1: X+←Z+, X-←Z-, Z+←X-, Z-←X+
    int[6](1, 0, 2, 3, 5, 4), // rot=2: X+←X-, X-←X+, Z+←Z-, Z-←Z+
    int[6](5, 4, 2, 3, 0, 1)  // rot=3: X+←Z-, X-←Z+, Z+←X+, Z-←X-
);

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
    
    // Extract face rotation from data: bits 4-5
    uint faceRot = (aData >> 4u) & 3u;
    vFaceRot = faceRot;
    
    // Determine effective face index for texture lookup
    int faceIdx = int(aNormal);
    int texFace = faceIdx;
    if (faceRot != 0u) {
        texFace = faceRemapTable[faceRot][faceIdx];
    }
    
    // Use atlas indices uniform for texture lookup
    int atlasLookup = int(aMaterial) * 6 + texFace;
    uint textureIndex = 0u;
    if (atlasLookup >= 0 && atlasLookup < (kBlockTypeCount * 6)) {
        textureIndex = uint(uAtlasIndices[atlasLookup]);
    }
    if (int(textureIndex) < 0) textureIndex = 0u;
    
    float col = float(textureIndex % 16u);
    float row = float(textureIndex / 16u);
    
    // Invert row because index 0 is usually top-left, but OpenGL 0 is bottom
    row = 15.0 - row;
    
    // Pass the origin of the texture cell in the atlas
    vCellOrigin = vec2(col * cellSize, row * cellSize);
    
    vMaterial = aMaterial;
    vAO = float(aAO) / 3.0;
    
    // Extract sky light from data field (lower 4 bits for non-water blocks)
    // Bits 4-5 contain face rotation (already extracted above)
    // For water blocks, this will be 0 (water uses data for level/flags) but water 
    // is rendered separately and always gets daylight
    vSkyLight = float(aData & 0xFu) / 15.0;
}
