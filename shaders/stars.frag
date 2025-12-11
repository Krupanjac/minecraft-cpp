#version 450 core
out vec4 FragColor;

uniform float uSunHeight;

void main() {
    // Fade out stars during day
    // Visible when sunHeight < 0.2
    float alpha = clamp(1.0 - (uSunHeight + 0.2) * 2.5, 0.0, 1.0);
    
    // Ensure minimum visibility for debugging if needed, but for now keep logic
    if (alpha <= 0.01) discard;
    
    FragColor = vec4(1.0, 1.0, 1.0, alpha);
}
