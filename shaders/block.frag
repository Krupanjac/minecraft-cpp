#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in uint vMaterial;

uniform vec3 uCameraPos;

out vec4 FragColor;

vec3 getMaterialColor(uint material) {
    if (material == 1u) return vec3(0.4, 0.7, 0.3);      // Grass
    else if (material == 2u) return vec3(0.5, 0.35, 0.2); // Dirt
    else if (material == 3u) return vec3(0.5, 0.5, 0.5);  // Stone
    else if (material == 4u) return vec3(0.9, 0.85, 0.6); // Sand
    else if (material == 5u) return vec3(0.2, 0.3, 0.8);  // Water
    else if (material == 6u) return vec3(0.4, 0.3, 0.2);  // Wood
    else if (material == 7u) return vec3(0.2, 0.6, 0.2);  // Leaves
    return vec3(1.0, 0.0, 1.0);  // Magenta for undefined
}

void main() {
    vec3 baseColor = getMaterialColor(vMaterial);
    
    // Simple lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(vNormal);
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    
    vec3 lighting = vec3(ambient + diffuse * 0.7);
    vec3 color = baseColor * lighting;
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogStart = 100.0;
    float fogEnd = 200.0;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogColor = vec3(0.53, 0.81, 0.92);
    color = mix(fogColor, color, fogFactor);
    
    FragColor = vec4(color, 1.0);
}
