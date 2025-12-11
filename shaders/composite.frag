#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D ssao;
uniform sampler2D volumetric;

uniform float exposure;
uniform float gamma;

void main()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    float ao = texture(ssao, TexCoords).r;
    vec3 vol = texture(volumetric, TexCoords).rgb;
    
    // Apply AO
    hdrColor *= ao; 
    
    // Add Volumetric Lighting
    hdrColor += vol; 
    
    // Tone mapping (Reinhard)
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    FragColor = vec4(mapped, 1.0);
    // FragColor = vec4(hdrColor, 1.0); // DEBUG: Direct output
}
