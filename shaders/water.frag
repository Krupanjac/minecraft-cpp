#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in vec2 vCellOrigin;
flat in uint vMaterial;
flat in uint vLevel;
in float vAO;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform float uFogDist;
uniform vec3 uSkyColor;
uniform float uTime;
uniform sampler2D uTexture;
uniform sampler2D uSceneColor;    // Scene color for reflections
uniform sampler2D uSceneDepth;    // Scene depth for reflections
uniform float uAOStrength;
uniform int uEnableReflections;   // Enable screen-space reflections
uniform mat4 uView;
uniform mat4 uProjection;

// Debug
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec4 vCurrentClip;
in vec4 vPrevClip;

// Screen-space reflection
vec3 getReflection(vec3 worldPos, vec3 normal, vec3 viewDir) {
    // Calculate proper reflection direction
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Add slight wave distortion based on time
    float wave1 = sin(worldPos.x * 2.0 + uTime * 2.0) * 0.015;
    float wave2 = cos(worldPos.z * 2.0 + uTime * 1.5) * 0.015;
    reflectDir.x += wave1;
    reflectDir.z += wave2;
    reflectDir = normalize(reflectDir);
    
    // Get starting screen position
    vec4 startClip = uProjection * uView * vec4(worldPos, 1.0);
    vec2 startScreen = (startClip.xy / startClip.w) * 0.5 + 0.5;
    
    // Get end position along reflection ray
    vec3 endWorldPos = worldPos + reflectDir * 50.0;
    vec4 endClip = uProjection * uView * vec4(endWorldPos, 1.0);
    vec2 endScreen = (endClip.xy / endClip.w) * 0.5 + 0.5;
    
    // Ray direction in screen space
    vec2 rayDir = endScreen - startScreen;
    float rayLength = length(rayDir);
    
    if (rayLength < 0.001) {
        return uSkyColor;
    }
    
    rayDir /= rayLength;
    
    // March along the ray in screen space
    const int maxSteps = 64;
    float stepSize = min(rayLength, 1.0) / float(maxSteps);
    
    vec2 currentUV = startScreen;
    float startDepth = startClip.z / startClip.w;
    float endDepth = endClip.z / endClip.w;
    float depthStep = (endDepth - startDepth) / float(maxSteps);
    float currentDepth = startDepth;
    
    for (int i = 1; i <= maxSteps; i++) {
        currentUV += rayDir * stepSize;
        currentDepth += depthStep;
        
        // Check bounds
        if (currentUV.x < 0.0 || currentUV.x > 1.0 || currentUV.y < 0.0 || currentUV.y > 1.0) {
            break;
        }
        
        // Sample scene depth
        float sceneDepth = texture(uSceneDepth, currentUV).r;
        float rayDepth = currentDepth * 0.5 + 0.5;
        
        // Check for intersection (ray went behind geometry)
        float thickness = 0.02;
        if (rayDepth > sceneDepth && rayDepth < sceneDepth + thickness && sceneDepth < 0.999) {
            // Found intersection - sample scene color
            vec3 reflectedColor = texture(uSceneColor, currentUV).rgb;
            
            // Fade based on distance and screen edge
            float edgeFade = 1.0 - smoothstep(0.0, 0.1, min(min(currentUV.x, 1.0 - currentUV.x), 
                                                            min(currentUV.y, 1.0 - currentUV.y)));
            edgeFade = 1.0 - edgeFade;
            
            float distFade = 1.0 - float(i) / float(maxSteps);
            float fade = edgeFade * distFade;
            
            return mix(uSkyColor, reflectedColor, fade * 0.85);
        }
    }
    
    // No hit - return sky color with horizon blend
    float horizon = max(0.0, reflectDir.y);
    vec3 skyReflect = mix(uSkyColor * 0.7, uSkyColor, horizon);
    return skyReflect;
}

void main() {
    float cellSize = 1.0 / 16.0;
    
    // Animate water UVs
    vec2 animatedTexCoord = vTexCoord;
    
    if (vMaterial != 9u && vLevel > 0u) { // Not ICE and Not Source Block
        animatedTexCoord.y += uTime * 0.5; // Scroll vertically
    }
    
    vec2 uv = vCellOrigin + fract(animatedTexCoord) * cellSize;

    vec4 texColor = textureLod(uTexture, uv, 0.0);
    vec3 baseColor = texColor.rgb * vec3(0.6, 0.8, 1.0); // Tint blue
    
    if (vMaterial == 9u) { // ICE
        baseColor = texColor.rgb; // No tint
    }

    // Debug overrides
    if (uDebugNoTexture == 1) {
        baseColor = vec3(0.2, 0.4, 0.8); // solid debug tint
    }

    if (uDebugShowNormals == 1) {
        vec3 normalColor = normalize(vNormal) * 0.5 + 0.5;
        FragColor = vec4(normalColor, 1.0);
        vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
        vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
        Velocity = a - b;
        return;
    }

    // Restore normal and light direction (required for lighting computations)
    vec3 lightDir = normalize(uLightDir);
    vec3 normal = normalize(vNormal);
    
    // Add subtle wave normal distortion
    float waveTime = uTime * 1.5;
    float waveX = sin(vWorldPos.x * 3.0 + waveTime) * 0.05;
    float waveZ = cos(vWorldPos.z * 3.0 + waveTime * 0.8) * 0.05;
    vec3 waveNormal = normalize(normal + vec3(waveX, 0.0, waveZ));

    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    
    // Fresnel effect - more reflective at grazing angles
    float fresnel = pow(1.0 - max(dot(viewDir, waveNormal), 0.0), 3.0);
    fresnel = mix(0.2, 0.9, fresnel);  // Range from 20% to 90% reflective
    
    // Get reflection color
    vec3 reflectionColor = uSkyColor;
    if (uEnableReflections == 1 && vMaterial != 9u) {  // Not ice
        reflectionColor = getReflection(vWorldPos, waveNormal, viewDir);
    }
    
    // Specular highlight
    vec3 reflectDir = reflect(-lightDir, waveNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular = vec3(1.0) * spec;
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    
    // Ambient light based on sky color
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114));
    float ambient = clamp(skyBrightness * 0.6, 0.1, 0.5);
    
    // Water should not have AO - it's transparent and shouldn't receive corner shadows
    vec3 lighting = vec3(ambient + diffuse * 0.5);
    vec3 waterColor = baseColor * lighting;
    
    // Blend water color with reflection based on fresnel
    vec3 color = mix(waterColor, reflectionColor, fresnel) + specular;
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    // Transparency - less transparent with reflections
    float alpha = mix(0.7, 0.85, fresnel);
    
    FragColor = vec4(color, alpha);

    // Velocity output (screen space UV movement)
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
