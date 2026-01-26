#version 450 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 vCurrentClip;
in vec4 vPrevClip;
in vec4 vFragPosLightSpace;

uniform sampler2D uAlbedoMap;
uniform bool uHasTexture;
uniform vec4 uBaseColor;

uniform sampler2D uEmissiveMap;
uniform bool uHasEmissive;

// Shadow mapping
uniform sampler2D uShadowMap;
uniform int uUseShadows;

// Debug uniforms
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uAlphaMultiplier; // For death fade effect

// Shadow calculation with PCF
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if outside shadow map
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 0.0; // Not in shadow
    }
    
    float currentDepth = projCoords.z;
    
    // Bias based on surface angle to light
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    // PCF (Percentage-Closer Filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    vec4 albedo = uBaseColor;
    if (uDebugNoTexture == 0) {
        if (uHasTexture) {
            vec4 texColor = texture(uAlbedoMap, TexCoord);
            if (texColor.a < 0.1) discard;
            albedo *= texColor;
        }
    }
    else {
        // override albedo to base color when no-texture debug is active
        albedo = uBaseColor;
    }
    
    vec3 emission = vec3(0.0);
    if (uHasEmissive) {
        vec4 emColor = texture(uEmissiveMap, TexCoord);
        emission = emColor.rgb;
        // If the main texture is missing but we have emission, use emission alpha for discard
        if (!uHasTexture && emColor.a < 0.1) discard;
    }

    // If showing normals, output normal color and skip lighting
    if (uDebugShowNormals == 1) {
        vec3 normalColor = normalize(Normal) * 0.5 + 0.5;
        FragColor = vec4(normalColor, 1.0);
        vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
        vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
        Velocity = a - b;
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Calculate shadow
    float shadow = 0.0;
    if (uUseShadows != 0) {
        shadow = ShadowCalculation(vFragPosLightSpace, norm, lightDir);
    }
    
    // Lighting with shadow
    vec3 ambient = 0.4 * albedo.rgb;  // Slightly reduced ambient for better shadow contrast
    vec3 diffuse = diff * albedo.rgb * (1.0 - shadow * 0.7);  // Shadow reduces diffuse but not completely
    
    vec3 finalColor = ambient + diffuse + emission;
    
    float finalAlpha = albedo.a * uAlphaMultiplier;
    if (finalAlpha < 0.01) discard;
    FragColor = vec4(finalColor, finalAlpha);
    
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
