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
uniform int uUseShadows;
uniform float uAOStrength;

// Debug uniforms
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

in vec4 vFragPosLightSpace;
in vec4 vCurrentClip;
in vec4 vPrevClip;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map bounds - no shadow
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0 ||
       projCoords.z > 1.0) {
        return 0.0;
    }
    
    // Calculate bias based on surface angle to light
    // More aggressive bias for surfaces nearly parallel to light direction
    float NdotL = dot(normal, lightDir);
    float slopeFactor = 1.0 - NdotL;
    slopeFactor = clamp(slopeFactor * slopeFactor, 0.0, 1.0);
    
    // Depth-dependent bias - farther objects need more bias
    float depthBias = projCoords.z * 0.001;
    
    // Combined bias: base + slope-dependent + depth-dependent
    float bias = 0.001 + 0.003 * slopeFactor + depthBias;
    
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
            
            // Soft shadow edge using smoothstep
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
    
    return shadow * edgeFade;
}

void main() {
    // Tiling logic:
    // vTexCoord contains (0..w, 0..h)
    // vCellOrigin contains the atlas UV of the top-left of the texture
    float cellSize = 1.0 / 16.0;
    
    // Use textureLod to avoid artifacts at tile boundaries due to discontinuous derivatives from fract()
    vec2 uv = vCellOrigin + fract(vTexCoord) * cellSize;
    
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
    // Only calculate shadow if surface is facing the light
    float shadow = 0.0;
    if (uUseShadows != 0 && diffuse > 0.0) {
        shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
    }
    
    // Apply AO
    // Use smoothstep for non-linear AO curve
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float minAO = max(0.0, mix(1.0, 0.25, uAOStrength));
    float aoFactor = mix(minAO, 1.0, aoCurve);
    
    vec3 lighting = vec3(ambient + (1.0 - shadow) * diffuse * 0.7) * aoFactor;
    vec3 color = baseColor * lighting;
    
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
