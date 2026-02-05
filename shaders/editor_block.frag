#version 450 core

in vec3 vNormal;
in vec3 vColor;
in vec3 vFragPos;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform float uHighlight; // 0 or 1

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    
    // Ambient
    float ambient = 0.35;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Specular
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0) * 0.3;
    
    vec3 color = vColor * (ambient + diff * 0.6 + spec);
    
    // Highlight for selection
    if (uHighlight > 0.5) {
        color = mix(color, vec3(1.0, 1.0, 1.0), 0.3);
    }
    
    FragColor = vec4(color, 1.0);
}
