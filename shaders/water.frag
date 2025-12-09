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
uniform float uTime;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    vec3 baseColor = texColor.rgb * vec3(0.5, 0.7, 1.0); // Tint blue
    
    // Simple lighting
    vec3 lightDir = normalize(uLightDir);
    vec3 normal = normalize(vNormal);
    
    // Specular highlight
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(0.5) * spec;
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.4;
    
    // Apply AO
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float aoFactor = mix(0.5, 1.0, aoCurve); // Less AO on water
    
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
}
