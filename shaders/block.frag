#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in vec2 vCellOrigin;
flat in uint vMaterial;
in float vAO;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform float uFogDist;
uniform vec3 uSkyColor;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    // Tiling logic:
    // vTexCoord contains (0..w, 0..h)
    // vCellOrigin contains the atlas UV of the top-left of the texture
    float cellSize = 1.0 / 16.0;
    
    // Use textureLod to avoid artifacts at tile boundaries due to discontinuous derivatives from fract()
    vec2 uv = vCellOrigin + fract(vTexCoord) * cellSize;
    
    vec4 texColor = textureLod(uTexture, uv, 0.0);
    if (texColor.a < 0.1) discard;
    
    vec3 baseColor = texColor.rgb;
    
    // Material-based coloring for blocks without textures
    // GRASS = 1, SAND = 4, SNOW = 8, ICE = 9, GRAVEL = 10, SANDSTONE = 11
    if (vMaterial == 1u && vNormal.y > 0.5) { // Grass Top
        baseColor *= vec3(0.4, 0.8, 0.3);
    } else if (vMaterial == 7u) { // Leaves
        baseColor *= vec3(0.3, 0.7, 0.3);
    } else if (vMaterial == 4u) { // Sand
        baseColor = vec3(0.93, 0.87, 0.69);
    } else if (vMaterial == 8u) { // Snow
        baseColor = vec3(0.95, 0.95, 0.98);
    } else if (vMaterial == 9u) { // Ice
        baseColor = vec3(0.7, 0.85, 0.95);
    } else if (vMaterial == 10u) { // Gravel
        baseColor = vec3(0.55, 0.52, 0.50);
    } else if (vMaterial == 11u) { // Sandstone
        baseColor = vec3(0.85, 0.75, 0.60);
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
