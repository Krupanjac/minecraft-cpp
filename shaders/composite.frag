#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D ssao;
uniform sampler2D volumetric;

uniform float exposure;
uniform float gamma;
uniform float uAOStrength; // 0..1 (mix between no AO and full SSAO)

void main()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    float ao = texture(ssao, TexCoords).r;
    vec3 vol = texture(volumetric, TexCoords).rgb;
    
    // Soften AO curve slightly to avoid a visible overlay when strength is high
    float aoGamma = mix(1.0, 0.85, clamp(uAOStrength, 0.0, 1.0));
    float aoAdj = pow(ao, aoGamma);
    float aoMix = mix(1.0, aoAdj, clamp(uAOStrength, 0.0, 1.0));
    hdrColor *= aoMix; 
    
    // Add Volumetric Lighting
    hdrColor += vol; 
    
    // Tone mapping (Reinhard)
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    FragColor = vec4(mapped, 1.0);
    // FragColor = vec4(hdrColor, 1.0); // DEBUG: Direct output
}
