#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in vec2 vCellOrigin;
flat in uint vMaterial;
flat in int vFace;
in float vAO;
in float vSkyLight;  // Sky light level (0.0 = underground, 1.0 = full sky access)
in vec3 vTangent;
in vec3 vBitangent;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform float uFogDist;
uniform vec3 uSkyColor;

// Traditional atlas texture
uniform sampler2D uTexture;

// PBR Resource Pack texture arrays
uniform sampler2DArray uAlbedoArray;
uniform sampler2DArray uNormalArray;
uniform sampler2DArray uSpecularArray;
uniform int uUsePBRResourcePack;
const int kBlockTypeCount = 117;
// Texture layer indices for PBR resource pack (per block type, per face)
uniform int uTextureIndices[kBlockTypeCount * 6];

// Vegetation variant indices for randomization
uniform int uGrassVariants[8];
uniform int uGrassVariantCount;
uniform int uFlowerVariants[12];
uniform int uFlowerVariantCount;

// Biome color tinting (computed per-chunk or interpolated)
uniform vec3 uBiomeGrassColor;   // Grass/vegetation tint
uniform vec3 uBiomeFoliageColor; // Leaves/foliage tint
uniform int uUseBiomeColors;     // Enable biome-based coloring

uniform sampler2D uShadowMap;
uniform sampler2D uRayTracingMap;
uniform int uUseShadows;
uniform int uShadowMethod;
uniform int uUseRTShadows;
uniform float uAOStrength;
uniform int uFireLightCount;
uniform vec3 uFireLightPos[16];

// Debug uniforms
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

// Parallax mapping settings
uniform int uEnableParallax;
uniform float uParallaxScale;
uniform int uParallaxSteps;

in vec4 vFragPosLightSpace;
in vec4 vCurrentClip;
in vec4 vPrevClip;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    float NdotL = dot(normal, lightDir);
    if (NdotL <= 0.0) {
        return 1.0;
    }
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.5;
    }
    if(projCoords.z > 1.0) {
        return 0.0;
    }
    
    float slopeFactor = sqrt(1.0 - NdotL * NdotL);
    float depthBias = projCoords.z * 0.0005;
    float bias = 0.0005 + 0.002 * slopeFactor + depthBias;
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float currentDepth = projCoords.z;
    
    const int pcfRadius = 2;
    float pcfSamples = 0.0;
    
    for(int x = -pcfRadius; x <= pcfRadius; ++x) {
        for(int y = -pcfRadius; y <= pcfRadius; ++y) {
            vec2 samplePos = projCoords.xy + vec2(x, y) * texelSize;
            
            if(samplePos.x < 0.0 || samplePos.x > 1.0 || 
               samplePos.y < 0.0 || samplePos.y > 1.0) {
                continue;
            }
            
            float pcfDepth = texture(uShadowMap, samplePos).r;
            float occluder = currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            shadow += occluder;
            pcfSamples += 1.0;
        }    
    }
    
    if (pcfSamples > 0.0) {
        shadow /= pcfSamples;
    }
    
    float edgeFade = 1.0;
    float edgeDist = min(min(projCoords.x, 1.0 - projCoords.x), 
                         min(projCoords.y, 1.0 - projCoords.y));
    edgeFade = smoothstep(0.0, 0.05, edgeDist);
    
    float grazingBoost = smoothstep(0.3, 0.0, NdotL) * 0.5;
    shadow = min(1.0, shadow + grazingBoost);
    
    return shadow * edgeFade;
}

// Apply normal map in tangent space
vec3 applyNormalMap(vec3 normalMapValue, vec3 normal, vec3 tangent, vec3 bitangent) {
    // Convert from [0,1] to [-1,1]
    vec3 tangentNormal = normalMapValue * 2.0 - 1.0;
    
    // Build TBN matrix
    mat3 TBN = mat3(normalize(tangent), normalize(bitangent), normalize(normal));
    
    return normalize(TBN * tangentNormal);
}

// Parallax Occlusion Mapping - gives 3D depth effect to textures
// Uses normal map to derive height - steep angles = crevices, flat = raised
float getHeight(vec2 uv, float layer) {
    vec3 normalSample = texture(uNormalArray, vec3(uv, layer)).rgb;

    // Use XY of normal map as a height proxy for stronger relief
    // Flat normals (~0.5, 0.5, 1.0) still yield mid-height for visible effect
    float height = (normalSample.r + normalSample.g) * 0.5;
    height = clamp(height * 1.5 - 0.25, 0.0, 1.0);
    return height;
}

vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDirTangent, float layer) {
    // View direction should point toward camera (into surface in tangent space)
    vec3 viewDir = normalize(viewDirTangent);
    
    // Flip if needed - we want positive Z pointing out of surface
    if (viewDir.z < 0.0) viewDir.z = -viewDir.z;
    
    // Fewer layers for performance and less blur
    float numLayers = mix(float(uParallaxSteps), 8.0, viewDir.z);
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    // UV offset direction - scale by parallax amount
    vec2 P = viewDir.xy * uParallaxScale;
    vec2 deltaTexCoords = P / numLayers;
    
    vec2 currentTexCoords = texCoords;
    float heightValue = getHeight(currentTexCoords, layer);
    float currentDepthMapValue = 1.0 - heightValue;
    
    // Simple steep parallax - march until we hit surface
    for (int i = 0; i < 32; i++) {
        if (currentLayerDepth >= currentDepthMapValue) break;
        
        currentTexCoords -= deltaTexCoords;
        heightValue = getHeight(currentTexCoords, layer);
        currentDepthMapValue = 1.0 - heightValue;
        currentLayerDepth += layerDepth;
    }
    
    // Simple interpolation for smoother result
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterHeight = currentDepthMapValue - currentLayerDepth;
    float beforeHeight = (1.0 - getHeight(prevTexCoords, layer)) - currentLayerDepth + layerDepth;
    float weight = afterHeight / (afterHeight - beforeHeight + 0.001);
    
    vec2 result = mix(currentTexCoords, prevTexCoords, clamp(weight, 0.0, 1.0));
    
    // Clamp to valid UV range with small margin to prevent edge bleeding
    float margin = 0.001;
    result = clamp(result, vec2(margin), vec2(1.0 - margin));
    
    return result;
}

void main() {
    vec3 baseColor;
    vec3 normal = normalize(vNormal);
    float roughness = 0.8;
    float metallic = 0.0;
    
    int textureLayer = 0;
    if (uUsePBRResourcePack == 1) {
        int lookupIdx = int(vMaterial) * 6 + vFace;
        if (lookupIdx >= 0 && lookupIdx < (kBlockTypeCount * 6)) {
            textureLayer = uTextureIndices[lookupIdx];
        }
        // Vegetation randomization based on world position
        // Tall grass (13) uses grass variants
        if (vMaterial == 13u && uGrassVariantCount > 0) {
            ivec3 blockPos = ivec3(floor(vWorldPos));
            int hash = blockPos.x * 73856093 ^ blockPos.y * 19349663 ^ blockPos.z * 83492791;
            hash = abs(hash);
            int variantIdx = hash % uGrassVariantCount;
            int variantLayer = uGrassVariants[variantIdx];
            if (variantLayer >= 0) textureLayer = variantLayer;
        }
        // Flowers (14) use flower variants
        else if (vMaterial == 14u && uFlowerVariantCount > 0) {
            ivec3 blockPos = ivec3(floor(vWorldPos));
            int hash = blockPos.x * 73856093 ^ blockPos.y * 19349663 ^ blockPos.z * 83492791;
            hash = abs(hash);
            int variantIdx = hash % uFlowerVariantCount;
            int variantLayer = uFlowerVariants[variantIdx];
            if (variantLayer >= 0) textureLayer = variantLayer;
        }
    }

    if (uUsePBRResourcePack == 1) {
        // Sample from texture arrays with texel inset to prevent edge bleeding
        vec2 uv = fract(vTexCoord);
        
        // Apply half-texel inset to prevent sampling outside the texture
        // This fixes the small gaps/seams between block faces
        float texSize = 128.0;  // Texture array resolution
        float texelInset = 0.5 / texSize;
        uv = uv * (1.0 - 2.0 * texelInset) + texelInset;
        
        // Calculate distance for LOD-based effects
        float distToCamera = length(uCameraPos - vWorldPos);
        
        // Apply parallax occlusion mapping if enabled
        // Works on all block faces using tangent space
        if (uEnableParallax == 1 && distToCamera < 48.0) {
            // Build TBN matrix for this face
            vec3 T = normalize(vTangent);
            vec3 B = normalize(vBitangent);
            vec3 N = normalize(vNormal);
            
            // Transform view direction to tangent space
            vec3 viewDirWorld = normalize(uCameraPos - vWorldPos);
            vec3 viewDirTangent = vec3(
                dot(viewDirWorld, T),
                dot(viewDirWorld, B),
                dot(viewDirWorld, N)
            );
            
            vec2 parallaxUV = parallaxOcclusionMapping(uv, viewDirTangent, float(textureLayer));
            
            // Fade out at distance
            float parallaxFade = 1.0 - smoothstep(24.0, 48.0, distToCamera);
            uv = mix(uv, parallaxUV, parallaxFade);
        }
        
        // Sample albedo with better filtering for vegetation
        vec4 albedo = texture(uAlbedoArray, vec3(uv, float(textureLayer)));
        
        // For vegetation (tall grass, flowers), use stricter alpha test to remove edge artifacts
        if (vMaterial == 13u || vMaterial == 14u) {
            if (albedo.a < 0.5) discard;  // Stricter threshold for vegetation
        } else {
            if (albedo.a < 0.1) discard;
        }
        baseColor = albedo.rgb;
        
        // Apply biome tinting for grayscale textures
        // NOTE: Grass block (material 1) textures are pre-tinted during loading
        // Only apply runtime tinting for leaves and vegetation that might need it
        
        // Leaves (material 7) - apply biome foliage color
        if (vMaterial == 7u) {
            // Check if the texture looks grayscale (R ≈ G ≈ B)
            float grayTest = abs(baseColor.r - baseColor.g) + abs(baseColor.g - baseColor.b);
            if (grayTest < 0.1) {
                // Apply biome foliage color if enabled, otherwise use default
                vec3 foliageTint = (uUseBiomeColors == 1) ? uBiomeFoliageColor : vec3(0.45, 0.75, 0.35);
                baseColor *= foliageTint;
            }
        }
        // Tall grass vegetation (material 13) - apply biome grass color
        else if (vMaterial == 13u) {
            float grayTest = abs(baseColor.r - baseColor.g) + abs(baseColor.g - baseColor.b);
            if (grayTest < 0.1) {
                // Apply biome grass color if enabled, otherwise use default
                vec3 grassTint = (uUseBiomeColors == 1) ? uBiomeGrassColor : vec3(0.5, 0.85, 0.4);
                baseColor *= grassTint;
            }
        }
        // Grass block top (material 1, top face) - apply biome grass color
        else if (vMaterial == 1u && vNormal.y > 0.5) {
            // For grass blocks, apply subtle biome tinting on top
            if (uUseBiomeColors == 1) {
                // Blend with biome color (subtle effect since texture is pre-tinted)
                baseColor = mix(baseColor, baseColor * uBiomeGrassColor * 2.0, 0.3);
            }
        }
        // Rose/flowers (material 14) - no tint, keep original colors
        
        // Sample and apply normal map
        vec3 normalMapValue = texture(uNormalArray, vec3(uv, float(textureLayer))).rgb;
        if (length(normalMapValue) > 0.1) {
            normal = applyNormalMap(normalMapValue, vNormal, vTangent, vBitangent);
        }
        
        // Sample specular/roughness map
        vec4 specularData = texture(uSpecularArray, vec3(uv, float(textureLayer)));
        roughness = specularData.r;  // R channel = roughness
        metallic = specularData.g;   // G channel = metallic
    } else {
        // Traditional atlas sampling
        float cellSize = 1.0 / 16.0;
        vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
        
        vec2 localUV = fract(vTexCoord);
        vec2 uv = vCellOrigin + localUV * (vec2(cellSize) - 2.0 * texel) + texel;
        
        vec4 texColor = textureLod(uTexture, uv, 0.0);
        
        // Force snow to pure white
        if (vMaterial == 8u) {
            texColor = vec4(1.0);
        }
        if (uDebugNoTexture == 0) {
            if (texColor.a < 0.1) discard;
        }
        
        baseColor = (uDebugNoTexture == 1) ? vec3(1.0, 1.0, 1.0) : texColor.rgb;
        
        // Material-based coloring for blocks
        if (vMaterial == 1u && vNormal.y > 0.5) {
            baseColor *= vec3(0.4, 0.8, 0.3);
        } else if (vMaterial == 7u) {
            baseColor *= vec3(0.3, 0.7, 0.3);
        } else if (vMaterial == 13u) {
            baseColor *= vec3(0.4, 0.8, 0.3);
        } else if (vMaterial == 4u) {
            baseColor *= vec3(0.93, 0.87, 0.69);
        } else if (vMaterial == 8u) {
            baseColor *= vec3(0.95, 0.95, 0.98);
        } else if (vMaterial == 9u) {
            baseColor *= vec3(0.7, 0.85, 0.95);
        } else if (vMaterial == 10u) {
            baseColor *= vec3(0.55, 0.52, 0.50);
        } else if (vMaterial == 11u) {
            baseColor *= vec3(0.85, 0.87, 0.69);
        } else if (vMaterial == 15u) {
            baseColor *= vec3(0.2, 0.2, 0.2);
        }
    }
    
    // Debug: show normals
    if (uDebugShowNormals == 1) {
        vec3 normalColor = normal * 0.5 + 0.5;
        FragColor = vec4(normalColor, 1.0);
        return;
    }
    
    // Lighting calculation
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    
    // Diffuse
    float diffuse = 0.0;
    if (lightDir.y >= 0.0) {
        diffuse = max(dot(normal, lightDir), 0.0);
    }
    
    // SKY LIGHT: Scale direct sunlight by sky light factor
    // vSkyLight is 0.0 when underground (can't see sky), 1.0 when above ground
    // This prevents sunlight from reaching caves/underground areas
    diffuse *= vSkyLight;
    
    // Specular (Blinn-Phong with roughness) - also affected by sky light
    float specular = 0.0;
    if (diffuse > 0.0 && uUsePBRResourcePack == 1) {
        vec3 halfDir = normalize(lightDir + viewDir);
        float shininess = mix(256.0, 8.0, roughness);
        specular = pow(max(dot(normal, halfDir), 0.0), shininess) * (1.0 - roughness) * 0.5 * vSkyLight;
    }
    
    // Ambient
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114));
    float baseAmbient = clamp(skyBrightness * 0.6, 0.05, 0.4);
    
    // Underground areas get significantly reduced ambient light
    // But not zero - there's always some minimal ambient from bounce light
    float minCaveAmbient = 0.08;  // Minimum ambient in caves
    float ambient = mix(minCaveAmbient, baseAmbient, vSkyLight);
    
    // Shadow
    float shadow = 0.0;
    
    // Skip shadow calculation for underground blocks (they're already dark)
    if (uUseShadows != 0 && vSkyLight > 0.0) {
        if (uShadowMethod == 1 && uUseRTShadows != 0) {
            vec2 screenUV = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
            vec4 rtData = texture(uRayTracingMap, screenUV);
            shadow = 1.0 - rtData.r;
        } else {
            shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
        }
        // Scale shadow effect by sky light (partially lit areas get partial shadow)
        shadow *= vSkyLight;
    } else if (vSkyLight == 0.0) {
        // Underground blocks are always in complete shadow from the sun
        shadow = 1.0;
    }
    
    // Apply AO with improved blending
    // vAO is 0-1 where 0 = fully occluded corner, 1 = no occlusion
    // Apply a softer curve that preserves detail without harsh darkening
    float aoBase = vAO;
    
    // Use a power curve for more natural AO falloff
    // Lower power = softer shadows, higher = harder
    float aoPower = mix(0.5, 1.5, uAOStrength); // Soft at low strength, harder at high
    float aoCurve = pow(aoBase, aoPower);
    
    // Clamp minimum brightness to prevent overly dark corners
    // At strength 0, no darkening. At strength 1, minimum is 0.4 (not pitch black)
    float minBrightness = mix(1.0, 0.4, uAOStrength);
    float aoFactor = mix(minBrightness, 1.0, aoCurve);
    
    // Final lighting
    float directLightStrength = 1.0;
    float shadowContrast = clamp(shadow * 1.2, 0.0, 1.0);
    
    vec3 warmTint = vec3(1.05, 1.0, 0.95);
    vec3 coolTint = vec3(0.95, 0.97, 1.05);
    vec3 tint = mix(warmTint, coolTint, shadowContrast);
    
    vec3 lighting = vec3(ambient + (1.0 - shadowContrast) * (diffuse + specular) * directLightStrength) * aoFactor;

    float fireLight = 0.0;
    for (int i = 0; i < uFireLightCount; ++i) {
        vec3 toLight = uFireLightPos[i] - vWorldPos;
        float d = length(toLight);
        vec3 ldir = (d > 0.001) ? (toLight / d) : vec3(0.0, 1.0, 0.0);
        float ndl = clamp(dot(normal, ldir), 0.0, 1.0);
        float att = clamp(1.0 - d / 4.0, 0.0, 1.0);
        fireLight += att * att * (0.35 + 0.65 * ndl);
    }
    fireLight = clamp(fireLight, 0.0, 0.28);
    vec3 fireContribution = vec3(1.0, 0.55, 0.18) * fireLight;

    vec3 color = baseColor * lighting * tint + fireContribution;
    
    // Apply metallic
    if (metallic > 0.0 && uUsePBRResourcePack == 1) {
        vec3 F0 = mix(vec3(0.04), baseColor, metallic);
        float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 5.0);
        vec3 specColor = mix(F0, vec3(1.0), fresnel);
        color = mix(color, color * specColor, metallic);
    }
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    FragColor = vec4(color, 1.0);
    
    // Velocity
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
