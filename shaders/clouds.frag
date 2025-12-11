#version 450 core
out vec4 FragColor;

in vec3 vWorldPos;

uniform vec3 uCameraPos;
uniform vec3 uSkyColor;
uniform float uFogDist;
uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;
uniform int uUseShadows;

float ShadowPCF(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float shadow = 0.0;
    float bias = 0.005;
    ivec2 texSize = textureSize(uShadowMap, 0);
    float texelSizeX = 1.0 / float(texSize.x);
    float texelSizeY = 1.0 / float(texSize.y);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x) * texelSizeX, float(y) * texelSizeY);
            float pcfDepth = texture(uShadowMap, projCoords.xy + offset).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec4 cloudColor = vec4(1.0, 1.0, 1.0, 0.8);

    // Fog-based visibility (0 near, 1 far)
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist * 1.5; // Clouds visible further than terrain
    float fogStart = fogEnd * 0.5;
    float fogFactor = clamp((distance - fogStart) / (fogEnd - fogStart), 0.0, 1.0);

    vec3 finalColor = mix(cloudColor.rgb, uSkyColor, fogFactor);

    float baseAlpha = 0.6;
    float alpha = baseAlpha * clamp(1.0 - fogFactor, 0.05, 1.0);

    // Shadowing
    float shadow = 0.0;
    if(uUseShadows == 1) {
        vec4 fragPosLightSpace = uLightSpaceMatrix * vec4(vWorldPos, 1.0);
        shadow = ShadowPCF(fragPosLightSpace);
    }
    finalColor *= mix(1.0, 0.5, shadow);

    // Keep clouds visible even when camera is inside
    FragColor = vec4(finalColor, alpha);
}
