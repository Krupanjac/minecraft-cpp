#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform mat4 invViewProj;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform float uIntensity;
uniform vec3 uLightColor;

const int STEPS = 32;
const float MAX_DIST = 100.0;

vec3 getWorldPos(vec2 uv, float depth) {
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldSpace = invViewProj * clipSpace;
    return worldSpace.xyz / worldSpace.w;
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
    
    // Simple volumetric fog: accumulate density
    // In a real engine, we'd check shadow map here
    
    for(int i = 0; i < STEPS; ++i) {
        currentPos += rayDir * stepSize;
        
        // Uniform low density atmosphere
        float density = 0.005;
        
        // Fade out underground to prevent light leaking
        if (currentPos.y < 0.0) {
            density = 0.0;
        }
        
        // Directional light scattering (Mie scattering approximation)
        float scattering = max(dot(rayDir, lightDir), 0.0);
        float phase = pow(scattering, 6.0); // Sharper forward scattering
        
        // Accumulate light
        // Mostly forward scattering (sun glow), very little ambient
        accumulation += density * (0.05 + phase) * stepSize;
    }
    
    FragColor = vec4(uLightColor * accumulation * uIntensity, 1.0);
}
