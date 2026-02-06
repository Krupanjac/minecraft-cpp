#version 450 core

in vec3 vNormal;
in vec3 vColor;
in vec3 vFragPos;
in vec2 vUV;
in float vTexLayer;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform float uHighlight; // 0 or 1
uniform int uTextureMode; // 0=color only, 1=atlas, 2=PBR
uniform sampler2D uTexture;          // atlas texture (mode 1)
uniform sampler2DArray uAlbedoArray; // PBR albedo array (mode 2)

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    
    // Get base color from the appropriate source
    vec3 baseColor;
    float alpha = 1.0;
    
    if (uTextureMode == 1) {
        // Atlas texture
        vec4 texSample = textureLod(uTexture, vUV, 0.0);
        baseColor = texSample.rgb;
        alpha = texSample.a;
        if (alpha < 0.1) discard;
    } else if (uTextureMode == 2 && vTexLayer >= 0.0) {
        // PBR albedo array texture
        vec4 texSample = texture(uAlbedoArray, vec3(vUV, vTexLayer));
        baseColor = texSample.rgb;
        alpha = texSample.a;
        if (alpha < 0.1) discard;
    } else {
        // Flat color
        baseColor = vColor;
    }
    
    // Ambient
    float ambient = 0.35;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Specular
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0) * 0.3;
    
    vec3 color = baseColor * (ambient + diff * 0.6 + spec);
    
    // Highlight for selection
    if (uHighlight > 0.5) {
        color = mix(color, vec3(1.0, 1.0, 1.0), 0.3);
    }
    
    FragColor = vec4(color, alpha);
}
