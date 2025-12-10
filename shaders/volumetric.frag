#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform mat4 invViewProj;
uniform vec3 lightDir;
uniform vec3 cameraPos;

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
        
        // Simple height fog
        float density = exp(-currentPos.y * 0.1) * 0.1;
        
        // Directional light scattering (Mie scattering approximation)
        float scattering = max(dot(rayDir, lightDir), 0.0);
        scattering = pow(scattering, 4.0); // Phase function
        
        accumulation += density * (0.5 + scattering) * stepSize;
    }
    
    FragColor = vec4(vec3(1.0, 0.9, 0.7) * accumulation, 1.0);
}
