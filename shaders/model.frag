#version 450 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D uAlbedoMap;
uniform bool uHasTexture;
uniform vec4 uBaseColor;

uniform sampler2D uEmissiveMap;
uniform bool uHasEmissive;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;

void main() {
    vec4 albedo = uBaseColor;
    if (uHasTexture) {
        vec4 texColor = texture(uAlbedoMap, TexCoord);
        if (texColor.a < 0.1) discard;
        albedo *= texColor;
    }
    
    vec3 emission = vec3(0.0);
    if (uHasEmissive) {
        vec4 emColor = texture(uEmissiveMap, TexCoord);
        emission = emColor.rgb;
        // If the main texture is missing but we have emission, use emission alpha for discard
        if (!uHasTexture && emColor.a < 0.1) discard;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(norm, lightDir), 0.2);
    
    vec3 ambient = 0.5 * albedo.rgb; 
    vec3 diffuse = diff * albedo.rgb;
    
    vec3 finalColor = ambient + diffuse + emission;
    
    FragColor = vec4(finalColor, albedo.a);
}
