#version 450 core

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec2 uAtlasOffset;  // Offset into the atlas for the destruction stage
uniform float uCellSize;    // Size of one cell in atlas (1/16 = 0.0625)

out vec4 FragColor;

void main() {
    // Sample the destruction texture from the atlas
    vec2 atlasUV = uAtlasOffset + fract(vTexCoord) * uCellSize;
    vec4 texColor = texture(uTexture, atlasUV);
    
    // The destruction textures have black as transparent
    // Discard fully black/transparent pixels
    if (texColor.rgb == vec3(0.0) || texColor.a < 0.01) {
        discard;
    }
    
    // Darken the texture slightly and make it semi-transparent for overlay effect
    FragColor = vec4(texColor.rgb * 0.3, texColor.a * 0.8);
}
