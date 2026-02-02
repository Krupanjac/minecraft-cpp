#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
flat in int vFaceId;
in vec4 vFragPosLightSpace;

uniform sampler2D uTexture;
uniform sampler2D uShadowMap;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uSkyColor;
uniform float uAlpha;
uniform int uUseShadows;

// Texture atlas uniforms - one index per face
uniform int uTexIndexTop;
uniform int uTexIndexBottom;
uniform int uTexIndexSide;

layout(location = 0) out vec4 FragColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    float NdotL = dot(normal, lightDir);
    if (NdotL <= 0.0) return 1.0;
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) {
        return 0.0;
    }
    
    float slopeFactor = sqrt(1.0 - NdotL * NdotL);
    float bias = 0.001 + 0.003 * slopeFactor;
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float currentDepth = projCoords.z;
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    // Select texture index based on face
    int texIndex;
    if (vFaceId == 4) {         // Top face (Y+)
        texIndex = uTexIndexTop;
    } else if (vFaceId == 5) {  // Bottom face (Y-)
        texIndex = uTexIndexBottom;
    } else {                     // Side faces
        texIndex = uTexIndexSide;
    }
    
    // Calculate atlas UV
    const float atlasSize = 16.0;
    const float cellSize = 1.0 / atlasSize;
    
    float col = float(texIndex % 16);
    float row = float(texIndex / 16);
    row = 15.0 - row;  // Flip because OpenGL Y is inverted
    
    vec2 cellOrigin = vec2(col, row) * cellSize;
    
    // Inset to prevent bleeding
    vec2 texelSize = 1.0 / vec2(textureSize(uTexture, 0));
    vec2 localUV = fract(vTexCoord);
    vec2 uv = cellOrigin + localUV * (vec2(cellSize) - 2.0 * texelSize) + texelSize;
    
    vec4 texColor = texture(uTexture, uv);
    if (texColor.a < 0.1) discard;
    
    vec3 baseColor = texColor.rgb;
    
    // Lighting
    vec3 lightDir = normalize(uLightDir);
    vec3 normal = normalize(vNormal);
    
    float diffuse = 0.0;
    if (lightDir.y >= 0.0) {
        diffuse = max(dot(normal, lightDir), 0.0);
    }
    
    float skyBrightness = dot(uSkyColor, vec3(0.299, 0.587, 0.114));
    float ambient = clamp(skyBrightness * 0.6, 0.1, 0.5);
    
    // Shadow
    float shadow = 0.0;
    if (uUseShadows != 0) {
        shadow = ShadowCalculation(vFragPosLightSpace, normal, lightDir);
    }
    
    // Combine lighting
    float light = ambient + diffuse * (1.0 - shadow) * 0.7;
    vec3 finalColor = baseColor * light;
    
    // Slight rim lighting for better visibility
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = pow(rim, 3.0) * 0.2;
    finalColor += rim * skyBrightness;
    
    FragColor = vec4(finalColor, uAlpha);
}
