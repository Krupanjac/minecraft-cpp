#version 450 core

in vec2 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uAtlasTexture;
uniform int uAdditiveBlend;  // 1 for additive, 0 for alpha blend

void main() {
    vec4 texColor = texture(uAtlasTexture, vTexCoord);
    
    // Apply color tint
    vec4 finalColor = texColor * vColor;
    
    // Discard fully transparent pixels
    if (finalColor.a < 0.01) {
        discard;
    }
    
    FragColor = finalColor;
}
