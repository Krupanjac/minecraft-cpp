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
uniform float uAOStrength;

// Debug
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec4 vCurrentClip;
in vec4 vPrevClip;

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

    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(0.5) * spec;
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    
    // Ambient light based on sky color
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114));
    float ambient = clamp(skyBrightness * 0.6, 0.1, 0.5);
    
    // Apply AO
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float minAO = max(0.0, mix(1.0, 0.5, uAOStrength));
    float aoFactor = mix(minAO, 1.0, aoCurve); // Less AO on water
    
    vec3 lighting = vec3(ambient + diffuse * 0.8) * aoFactor;
    vec3 color = baseColor * lighting + specular;
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    // Transparency
    float alpha = 0.7;
    
    FragColor = vec4(color, alpha);

    // Velocity output (screen space UV movement)
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
