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
uniform sampler2D uShadowMap;
uniform int uUseShadows;

in vec4 vFragPosLightSpace;

out vec4 FragColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        return 0.0;
        
    // calculate bias (based on depth map resolution and slope)
    float bias = max(0.001 * (1.0 - dot(normal, lightDir)), 0.0001);
    
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            float currentDepth = projCoords.z;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

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
    } else if (vMaterial == 13u) { // Tall Grass
        baseColor *= vec3(0.4, 0.8, 0.3);
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
    
    // Ensure light doesn't leak from below
    // If lightDir.y is negative (sun below horizon), diffuse should be 0
    // But we might want moonlight. For now, let's just clamp.
    float diffuse = max(dot(normal, lightDir), 0.0);
    
    // Ambient light
    // Increase ambient slightly at night so it's not pitch black
    float ambient = 0.3;
    
    // Calculate Shadow
    // Only calculate shadow if surface is facing the light
    float shadow = 0.0;
    if (uUseShadows != 0 && diffuse > 0.0) {
        shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
    }
    
    // Apply AO
    // Use smoothstep for non-linear AO curve
    float aoCurve = smoothstep(0.0, 1.0, vAO);
    float aoFactor = mix(0.25, 1.0, aoCurve);
    
    vec3 lighting = vec3(ambient + (1.0 - shadow) * diffuse * 0.7) * aoFactor;
    vec3 color = baseColor * lighting;
    
    // Fog
    float distance = length(vWorldPos - uCameraPos);
    float fogEnd = uFogDist;
    float fogStart = fogEnd * 0.75;
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    
    color = mix(uSkyColor, color, fogFactor);
    
    FragColor = vec4(color, 1.0);
}
