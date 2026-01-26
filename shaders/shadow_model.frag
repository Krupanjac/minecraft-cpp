#version 450 core

uniform sampler2D uAlbedoMap;
uniform bool uHasTexture;

in vec2 TexCoord;

void main() {
    // Alpha test for transparent parts of the model
    if (uHasTexture) {
        float alpha = texture(uAlbedoMap, TexCoord).a;
        if (alpha < 0.5) discard;
    }
    // Depth is written automatically
}
