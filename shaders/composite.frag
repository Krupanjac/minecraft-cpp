#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D ssao;
uniform sampler2D volumetric;

uniform float exposure;
uniform float gamma;
uniform float uAOStrength; // 0..1 (mix between no AO and full SSAO)
uniform vec2 uScreenShake; // Screen shake offset in UV space
uniform float uShakeStrength; // 0..1, controls blur strength

void main()
{
    // Apply screen shake offset to UV coordinates
    vec2 shakenUV = TexCoords + uScreenShake;
    
    vec3 hdrColor;
    if (uShakeStrength > 0.001) {
        // Simple 5-tap blur when shaking
        vec2 blurOffset = vec2(0.0034) * uShakeStrength;
        vec3 c0 = texture(scene, shakenUV).rgb;
        vec3 c1 = texture(scene, shakenUV + vec2( blurOffset.x, 0.0)).rgb;
        vec3 c2 = texture(scene, shakenUV + vec2(-blurOffset.x, 0.0)).rgb;
        vec3 c3 = texture(scene, shakenUV + vec2(0.0,  blurOffset.y)).rgb;
        vec3 c4 = texture(scene, shakenUV + vec2(0.0, -blurOffset.y)).rgb;
        hdrColor = (c0 * 0.4) + (c1 + c2 + c3 + c4) * 0.15;
    } else {
        hdrColor = texture(scene, shakenUV).rgb;
    }
    float ao = texture(ssao, shakenUV).r;
    vec3 vol = texture(volumetric, shakenUV).rgb;
    
    // Improved SSAO blending
    // Use a more natural falloff curve
    float aoStrength = clamp(uAOStrength, 0.0, 1.0);
    
    // Remap AO to avoid too dark shadows
    // ao is 0-1 where 1 = no occlusion, 0 = full occlusion
    // We want to limit how dark it can get based on strength
    float minAO = mix(1.0, 0.3, aoStrength); // At full strength, minimum brightness is 0.3
    float aoRemapped = mix(minAO, 1.0, ao);
    
    // Apply a subtle curve for more natural appearance
    float aoCurved = pow(aoRemapped, mix(1.0, 1.2, aoStrength));
    
    // Blend: at 0 strength = no AO, at 1 strength = full AO effect
    float aoFinal = mix(1.0, aoCurved, aoStrength);
    
    // Apply AO to color
    // Use soft-light style blending for more natural look on bright surfaces
    vec3 aoColor = hdrColor * aoFinal;
    
    // Preserve some color in shadowed areas to prevent overly dark corners
    float luminance = dot(hdrColor, vec3(0.2126, 0.7152, 0.0722));
    float preserveFactor = smoothstep(0.0, 0.3, luminance) * 0.15 * (1.0 - ao) * aoStrength;
    aoColor += hdrColor * preserveFactor;
    
    hdrColor = aoColor;
    
    // Add Volumetric Lighting
    hdrColor += vol; 
    
    // Tone mapping (Reinhard)
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    FragColor = vec4(mapped, 1.0);
}
