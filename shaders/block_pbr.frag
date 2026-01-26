#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in vec2 vCellOrigin;
flat in uint vMaterial;
flat in int vTextureLayer;
in float vAO;
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

uniform sampler2D uShadowMap;
uniform sampler2D uRayTracingMap;
uniform int uUseShadows;
uniform int uShadowMethod;
uniform int uUseRTShadows;
uniform float uAOStrength;

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
// Since we don't have dedicated height maps, we derive height from normal map
// Normal maps encode surface orientation: flat = (0.5, 0.5, 1.0), tilted = deviation from that
float getHeightFromNormal(vec3 normalSample) {
    // Normal map is in [0,1], convert to [-1,1] tangent space
    vec3 n = normalSample * 2.0 - 1.0;
    // Use the Z component directly - high Z (pointing up) = raised surface
    // Low Z (tilted) = crevice/dip
    // This is smoother than using xy deviation magnitude
    float height = n.z * 0.5 + 0.5;  // Map from [-1,1] to [0,1]
    return height;
}

vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDirTangent, float layer) {
    // Ensure view direction points INTO the surface (positive Z in tangent space)
    vec3 viewDir = viewDirTangent;
    if (viewDir.z < 0.0) viewDir = -viewDir;
    
    // Number of layers - more at grazing angles for quality
    float minLayers = 8.0;
    float maxLayers = float(uParallaxSteps);
    float numLayers = mix(maxLayers, minLayers, abs(viewDir.z));
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    // Calculate UV offset per layer
    vec2 P = viewDir.xy / max(viewDir.z, 0.2) * uParallaxScale;
    vec2 deltaTexCoords = P / numLayers;
    
    vec2 currentTexCoords = texCoords;
    vec3 normalSample = texture(uNormalArray, vec3(currentTexCoords, layer)).rgb;
    float currentDepthMapValue = 1.0 - getHeightFromNormal(normalSample);  // Invert: high surface = low depth
    
    // March through layers from surface (depth 0) downward (depth 1)
    int maxIterations = int(numLayers) + 1;
    for (int i = 0; i < maxIterations; i++) {
        if (currentLayerDepth >= currentDepthMapValue) break;
        
        currentTexCoords -= deltaTexCoords;
        normalSample = texture(uNormalArray, vec3(fract(currentTexCoords), layer)).rgb;
        currentDepthMapValue = 1.0 - getHeightFromNormal(normalSample);
        currentLayerDepth += layerDepth;
    }
    
    // Interpolate between current and previous position for smooth result
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    vec3 prevNormalSample = texture(uNormalArray, vec3(fract(prevTexCoords), layer)).rgb;
    float prevDepthMapValue = 1.0 - getHeightFromNormal(prevNormalSample);
    float beforeDepth = prevDepthMapValue - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth + 0.0001);
    weight = clamp(weight, 0.0, 1.0);
    
    vec2 finalTexCoords = mix(currentTexCoords, prevTexCoords, weight);
    
    return fract(finalTexCoords);
}

void main() {
    vec3 baseColor;
    vec3 normal = normalize(vNormal);
    float roughness = 0.8;
    float metallic = 0.0;
    
    if (uUsePBRResourcePack == 1 && vTextureLayer >= 0) {
        // Sample from texture arrays
        vec2 uv = fract(vTexCoord);
        
        // Calculate distance for LOD-based effects
        float distToCamera = length(uCameraPos - vWorldPos);
        
        // Apply parallax occlusion mapping if enabled (works in both light and shadow)
        // Fade out parallax at distance to prevent artifacts and improve performance
        if (uEnableParallax == 1 && distToCamera < 32.0) {
            // Calculate view direction in tangent space for parallax
            mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
            mat3 TBN_inv = transpose(TBN);  // TBN is orthonormal, so transpose = inverse
            vec3 viewDir = normalize(uCameraPos - vWorldPos);
            vec3 viewDirTangent = normalize(TBN_inv * viewDir);
            
            vec2 parallaxUV = parallaxOcclusionMapping(uv, viewDirTangent, float(vTextureLayer));
            
            // Smoothly blend between parallax and non-parallax based on distance
            float parallaxFade = 1.0 - smoothstep(16.0, 32.0, distToCamera);
            uv = mix(uv, parallaxUV, parallaxFade);
        }
        
        // Sample albedo with better filtering for vegetation
        vec4 albedo = texture(uAlbedoArray, vec3(uv, float(vTextureLayer)));
        
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
        
        // Leaves (material 7) - may need tinting if texture is grayscale
        if (vMaterial == 7u) {
            // Check if the texture looks grayscale (R ≈ G ≈ B)
            float grayTest = abs(baseColor.r - baseColor.g) + abs(baseColor.g - baseColor.b);
            if (grayTest < 0.1) {
                baseColor *= vec3(0.45, 0.75, 0.35);  // Green leaves tint
            }
        }
        // Tall grass vegetation (material 13) - these use actual grass textures now
        // The grass.png, tall_grass_bottom.png textures have their own colors
        // but if they appear too gray, apply subtle tint
        else if (vMaterial == 13u) {
            float grayTest = abs(baseColor.r - baseColor.g) + abs(baseColor.g - baseColor.b);
            if (grayTest < 0.1) {
                baseColor *= vec3(0.5, 0.85, 0.4);  // Green grass tint
            }
        }
        // Rose/flowers (material 14) - no tint, keep original colors
        
        // Sample and apply normal map
        vec3 normalMapValue = texture(uNormalArray, vec3(uv, float(vTextureLayer))).rgb;
        if (length(normalMapValue) > 0.1) {
            normal = applyNormalMap(normalMapValue, vNormal, vTangent, vBitangent);
        }
        
        // Sample specular/roughness map
        vec4 specularData = texture(uSpecularArray, vec3(uv, float(vTextureLayer)));
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
    
    // Specular (Blinn-Phong with roughness)
    float specular = 0.0;
    if (diffuse > 0.0 && uUsePBRResourcePack == 1) {
        vec3 halfDir = normalize(lightDir + viewDir);
        float shininess = mix(256.0, 8.0, roughness);
        specular = pow(max(dot(normal, halfDir), 0.0), shininess) * (1.0 - roughness) * 0.5;
    }
    
    // Ambient
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114));
    float ambient = clamp(skyBrightness * 0.6, 0.05, 0.4);
    
    // Shadow
    float shadow = 0.0;
    if (uUseShadows != 0) {
        if (uShadowMethod == 1 && uUseRTShadows != 0) {
            vec2 screenUV = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
            vec4 rtData = texture(uRayTracingMap, screenUV);
            shadow = 1.0 - rtData.r;
        } else {
            shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
        }
    }
    
    // Apply AO
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float minAO = max(0.0, mix(1.0, 0.25, uAOStrength));
    float aoFactor = mix(minAO, 1.0, aoCurve);
    
    // Final lighting
    float directLightStrength = 1.0;
    float shadowContrast = clamp(shadow * 1.2, 0.0, 1.0);
    
    vec3 warmTint = vec3(1.05, 1.0, 0.95);
    vec3 coolTint = vec3(0.95, 0.97, 1.05);
    vec3 tint = mix(warmTint, coolTint, shadowContrast);
    
    vec3 lighting = vec3(ambient + (1.0 - shadowContrast) * (diffuse + specular) * directLightStrength) * aoFactor;
    vec3 color = baseColor * lighting * tint;
    
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
