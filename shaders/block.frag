#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in uint vMaterial;
in float vAO;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform float uFogDist;
uniform vec3 uSkyColor;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.1) discard;
    
    vec3 baseColor = texColor.rgb;
    
    // Tinting for biome colors (Grass/Leaves)
    if (vMaterial == 1u && vNormal.y > 0.5) { // Grass Top
        baseColor *= vec3(0.4, 0.8, 0.3);
    } else if (vMaterial == 7u) { // Leaves
        baseColor *= vec3(0.3, 0.7, 0.3);
    }
    
    // Simple lighting
    vec3 lightDir = normalize(uLightDir);
    vec3 normal = normalize(vNormal);
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    
    // Apply AO
    // Use smoothstep for non-linear AO curve
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float aoFactor = mix(0.25, 1.0, aoCurve);
    
    vec3 lighting = vec3(ambient + diffuse * 0.7) * aoFactor;
    vec3 color = baseColor * lighting;
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    FragColor = vec4(color, 1.0);
}
