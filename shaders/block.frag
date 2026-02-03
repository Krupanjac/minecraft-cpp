#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in vec2 vCellOrigin;
flat in uint vMaterial;
in float vAO;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform float uFogDist;
uniform vec3 uSkyColor;
uniform sampler2D uTexture;
uniform sampler2D uShadowMap;
uniform sampler2D uRayTracingMap;  // Ray tracing output: R=shadow, G=sky, B=bounce, A=ao
uniform int uUseShadows;
uniform int uShadowMethod;         // 0 = Shadow Map, 1 = Ray Traced
uniform int uUseRTShadows;         // Use RT shadows when available
uniform float uAOStrength;
uniform int uFireLightCount;
uniform vec3 uFireLightPos[16];

// Debug uniforms
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

in vec4 vFragPosLightSpace;
in vec4 vCurrentClip;
in vec4 vPrevClip;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // CRITICAL: Check if surface faces AWAY from light (back-face)
    // Surfaces facing away from the sun should ALWAYS be in shadow
    // This is the key fix for cave interiors!
    float NdotL = dot(normal, lightDir);
    if (NdotL <= 0.0) {
        // Surface faces away from light - always in shadow
        return 1.0;
    }
    
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map bounds - assume shadow for safety in caves
    // (rather than no shadow, which causes light leaks)
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.5; // Partial shadow outside bounds
    }
    if(projCoords.z > 1.0) {
        return 0.0; // Beyond far plane - no shadow
    }
    
    // Calculate bias based on surface angle to light
    // Less aggressive bias since we handle back-faces separately
    float slopeFactor = sqrt(1.0 - NdotL * NdotL); // sin(angle)
    
    // Depth-dependent bias - farther objects need more bias
    float depthBias = projCoords.z * 0.0005;
    
    // Combined bias: base + slope-dependent + depth-dependent
    // Reduced base bias since back-faces are handled separately
    float bias = 0.0005 + 0.002 * slopeFactor + depthBias;
    
    // PCF with larger kernel for softer shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float currentDepth = projCoords.z;
    
    // 5x5 PCF for smoother shadows
    const int pcfRadius = 2;
    float pcfSamples = 0.0;
    
    for(int x = -pcfRadius; x <= pcfRadius; ++x) {
        for(int y = -pcfRadius; y <= pcfRadius; ++y) {
            vec2 samplePos = projCoords.xy + vec2(x, y) * texelSize;
            
            // Skip samples outside shadow map
            if(samplePos.x < 0.0 || samplePos.x > 1.0 || 
               samplePos.y < 0.0 || samplePos.y > 1.0) {
                continue;
            }
            
            float pcfDepth = texture(uShadowMap, samplePos).r;
            
            // Shadow test with bias
            float occluder = currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            shadow += occluder;
            pcfSamples += 1.0;
        }    
    }
    
    if (pcfSamples > 0.0) {
        shadow /= pcfSamples;
    }
    
    // Fade shadows at shadow map edges to prevent hard cutoffs
    float edgeFade = 1.0;
    float edgeDist = min(min(projCoords.x, 1.0 - projCoords.x), 
                         min(projCoords.y, 1.0 - projCoords.y));
    edgeFade = smoothstep(0.0, 0.05, edgeDist);
    
    // Boost shadow strength for grazing angles (surfaces nearly parallel to light)
    // This helps with edges of blocks catching unwanted light
    float grazingBoost = smoothstep(0.3, 0.0, NdotL) * 0.5;
    shadow = min(1.0, shadow + grazingBoost);
    
    return shadow * edgeFade;
}

void main() {
    // Tiling logic:
    // vTexCoord contains (0..w, 0..h)
    // vCellOrigin contains the atlas UV of the top-left of the texture
    float cellSize = 1.0 / 16.0;
    vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
    
    // Inset UVs by 1 texel to prevent atlas bleeding (gaps between block faces)
    vec2 localUV = fract(vTexCoord);
    vec2 uv = vCellOrigin + localUV * (vec2(cellSize) - 2.0 * texel) + texel;
    
    vec4 texColor = textureLod(uTexture, uv, 0.0);
    
    // Force snow to pure white so atlas misalignment doesn't matter
    if (vMaterial == 8u) {
        texColor = vec4(1.0);
    }
    if (uDebugNoTexture == 0) {
        if (texColor.a < 0.1) discard;
    }

    vec3 baseColor = (uDebugNoTexture == 1) ? vec3(1.0, 1.0, 1.0) : texColor.rgb;

    // If debugging normals, visualize normal and return immediately
    if (uDebugShowNormals == 1) {
        vec3 normalColor = normalize(vNormal) * 0.5 + 0.5;
        FragColor = vec4(normalColor, 1.0);
        return;
    }
    
    // Material-based coloring for blocks without textures
    // GRASS = 1, SAND = 4, SNOW = 8, ICE = 9, GRAVEL = 10, SANDSTONE = 11
    if (vMaterial == 1u && vNormal.y > 0.5) { // Grass Top
        baseColor *= vec3(0.4, 0.8, 0.3);
    } else if (vMaterial == 7u) { // Leaves
        baseColor *= vec3(0.3, 0.7, 0.3);
    } else if (vMaterial == 13u) { // Tall Grass
        baseColor *= vec3(0.4, 0.8, 0.3);
    } else if (vMaterial == 4u) { // Sand
        baseColor *= vec3(0.93, 0.87, 0.69);
    } else if (vMaterial == 8u) { // Snow
        baseColor *= vec3(0.95, 0.95, 0.98);
    } else if (vMaterial == 9u) { // Ice
        baseColor *= vec3(0.7, 0.85, 0.95);
    } else if (vMaterial == 10u) { // Gravel
        baseColor *= vec3(0.55, 0.52, 0.50);
    } else if (vMaterial == 11u) { // Sandstone
        baseColor *= vec3(0.85, 0.87, 0.69);
    } else if (vMaterial == 15u) { // Bedrock
        baseColor *= vec3(0.2, 0.2, 0.2);
    }
    
    // Simple lighting
    vec3 lightDir = normalize(uLightDir);
    vec3 normal = normalize(vNormal);
    
    // Ensure light doesn't leak from below
    // If lightDir.y is negative (sun below horizon), diffuse should be 0
    float diffuse = 0.0;
    if (lightDir.y >= 0.0) {
        diffuse = max(dot(normal, lightDir), 0.0);
    }
    
    // Ambient light
    // Use sky color brightness as ambient intensity
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114)); // Luminance
    float ambient = clamp(skyBrightness * 0.6, 0.05, 0.4);
    
    // Calculate Shadow
    float shadow = 0.0;
    
    if (uUseShadows != 0) {
        if (uShadowMethod == 1 && uUseRTShadows != 0) {
            // Pure ray traced shadows
            vec2 screenUV = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
            vec4 rtData = texture(uRayTracingMap, screenUV);
            shadow = 1.0 - rtData.r;  // R channel is direct light (0=shadow, 1=lit)
        } else {
            // Shadow map only
            shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
        }
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
    
    // CINEMATIC LIGHTING - increased contrast and light intensity
    // Boost direct light contribution for more dramatic look
    float directLightStrength = 1.0;  // Increased from 0.7
    float shadowContrast = shadow * 1.2;  // Deeper shadows
    shadowContrast = clamp(shadowContrast, 0.0, 1.0);
    
    // Add slight warm tint to lit areas, cool tint to shadows
    vec3 warmTint = vec3(1.05, 1.0, 0.95);  // Slight warm
    vec3 coolTint = vec3(0.95, 0.97, 1.05); // Slight cool
    vec3 tint = mix(warmTint, coolTint, shadowContrast);
    
    vec3 lighting = vec3(ambient + (1.0 - shadowContrast) * diffuse * directLightStrength) * aoFactor;

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
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    FragColor = vec4(color, 1.0);
    
    // Velocity Calculation
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
