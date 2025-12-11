#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;

uniform int uIsMoon;

float noise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    if (uIsMoon == 1) {
        // Pixelated Moon
        vec2 uv = vTexCoord;
        // 8x8 grid
        vec2 grid = floor(uv * 8.0);
        float n = noise(grid);
        
        vec3 moonColor = vec3(0.9, 0.9, 1.0);
        if (n > 0.7) moonColor *= 0.8; // Darker spots
        if (n > 0.9) moonColor *= 0.7; // Even darker spots
        
        FragColor = vec4(moonColor, 1.0);
    } else {
        FragColor = vec4(1.0, 0.95, 0.8, 1.0); // Bright sun color
    }
}