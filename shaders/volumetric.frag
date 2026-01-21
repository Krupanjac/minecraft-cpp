#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform sampler2D shadowMap;
uniform mat4 invViewProj;
uniform mat4 lightSpaceMatrix;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform float uIntensity;
uniform vec3 uLightColor;
uniform int uUseShadows;

const int STEPS = 48;
const float MAX_DIST = 150.0;

vec3 getWorldPos(vec2 uv, float depth) {
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldSpace = invViewProj * clipSpace;
    return worldSpace.xyz / worldSpace.w;
}

// Check if a point is in shadow using the shadow map
float getShadow(vec3 worldPos) {
    if (uUseShadows == 0) return 0.0; // No shadows = fully lit
    
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 0.0; // Assume lit outside shadow frustum
    }
    
    float currentDepth = projCoords.z;
    float shadowDepth = texture(shadowMap, projCoords.xy).r;
    
    // Small bias to prevent shadow acne
    float bias = 0.002;
    
    return currentDepth - bias > shadowDepth ? 1.0 : 0.0;
}

void main() {
    float depth = texture(depthMap, TexCoords).r;
    vec3 worldPos = getWorldPos(TexCoords, depth);
    vec3 rayDir = normalize(worldPos - cameraPos);
    float rayLen = length(worldPos - cameraPos);
    
    rayLen = min(rayLen, MAX_DIST);
    
    float stepSize = rayLen / float(STEPS);
    vec3 currentPos = cameraPos;
    
    float accumulation = 0.0;
    float transmittance = 1.0;
    
    for(int i = 0; i < STEPS; ++i) {
        currentPos += rayDir * stepSize;
        
        // Base density - uniform low density atmosphere
        float density = 0.004;
        
        // Fade out underground to prevent light leaking
        if (currentPos.y < 0.0) {
            density *= smoothstep(-20.0, 0.0, currentPos.y);
        }
        
        // Height-based density falloff (more fog/haze near ground)
        float heightFactor = exp(-max(0.0, currentPos.y - 64.0) * 0.01);
        density *= mix(0.5, 1.0, heightFactor);
        
        // Check shadow - if in shadow, no direct light contribution
        float shadow = getShadow(currentPos);
        float lightVisibility = 1.0 - shadow;
        
        // Directional light scattering (Mie scattering approximation)
        float scattering = max(dot(rayDir, lightDir), 0.0);
        float phase = pow(scattering, 8.0); // Sharper forward scattering
        
        // Combine phase function with visibility
        float inScatter = density * (0.02 + phase * 0.8) * lightVisibility;
        
        // Beer-Lambert absorption
        float absorption = density * 0.1;
        transmittance *= exp(-absorption * stepSize);
        
        // Accumulate in-scattered light
        accumulation += inScatter * transmittance * stepSize;
    }
    
    FragColor = vec4(uLightColor * accumulation * uIntensity, 1.0);
}
