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
uniform float uExplosionVignette; // 0..1
uniform float uSaturation;
uniform float uVibrance;
uniform float uContrast;
uniform float uBrightness;
uniform float uLift;
uniform float uGammaLift;
uniform float uGain;
uniform float uWhiteBalanceTemp;
uniform float uWhiteBalanceTint;
uniform float uBloomStrength;
uniform float uBloomThreshold;
uniform float uBloomKnee;
uniform float uVignetteStrength;
uniform float uVignetteRoundness;
uniform float uVignetteSmoothness;
uniform float uChromaticAberration;
uniform float uSharpness;

// ACES filmic tonemapping
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 applyWhiteBalance(vec3 color, float temperature, float tint) {
    // Simple white balance approximation
    vec3 balance = vec3(
        1.0 + temperature * 0.1 + tint * 0.05,
        1.0 - tint * 0.08,
        1.0 - temperature * 0.1 + tint * 0.05
    );
    return color * balance;
}

vec3 applyLiftGammaGain(vec3 color, float lift, float gamma, float gain) {
    vec3 lifted = color + lift;
    vec3 g = pow(max(lifted, vec3(0.0)), vec3(1.0 / max(gamma, 0.0001)));
    return g * gain;
}

vec3 applySaturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, saturation);
}

vec3 applyVibrance(vec3 color, float vibrance) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float maxc = max(color.r, max(color.g, color.b));
    float minc = min(color.r, min(color.g, color.b));
    float sat = (maxc - minc);
    float v = clamp(vibrance * (1.0 - sat), 0.0, 1.0);
    return mix(color, mix(vec3(luma), color, 1.2), v);
}

vec3 applyContrastBrightness(vec3 color, float contrast, float brightness) {
    color = (color - 0.5) * contrast + 0.5;
    color += brightness;
    return color;
}

vec3 sampleScene(vec2 uv) {
    vec2 clampedUV = clamp(uv, vec2(0.001), vec2(0.999));
    return texture(scene, clampedUV).rgb;
}

vec3 applySharpen(vec2 uv, float amount) {
    if (amount <= 0.0001) return sampleScene(uv);
    vec2 texel = vec2(1.0) / vec2(textureSize(scene, 0));
    vec3 center = sampleScene(uv);
    vec3 north = sampleScene(uv + vec2(0.0, texel.y));
    vec3 south = sampleScene(uv - vec2(0.0, texel.y));
    vec3 east = sampleScene(uv + vec2(texel.x, 0.0));
    vec3 west = sampleScene(uv - vec2(texel.x, 0.0));
    // Unsharp mask: amplify difference from neighbors, clamped to prevent negatives
    vec3 avg = (north + south + east + west) * 0.25;
    vec3 diff = center - avg;
    return max(center + diff * amount, vec3(0.0));
}

vec3 computeBloom(vec2 uv) {
    // Soft thresholding
    vec3 c = sampleScene(uv);
    float brightness = max(c.r, max(c.g, c.b));
    float knee = uBloomKnee * 0.5 + 0.0001;
    float soft = clamp((brightness - uBloomThreshold + knee) / (2.0 * knee), 0.0, 1.0);
    float contribution = max(brightness - uBloomThreshold, 0.0) + soft * soft * knee * 2.0;
    vec3 bloom = c * (contribution / max(brightness, 0.0001));

    // Small blur (9-tap)
    vec2 texel = vec2(1.0) / vec2(textureSize(scene, 0));
    vec3 blur = bloom * 0.2;
    blur += sampleScene(uv + vec2( texel.x, 0.0)) * 0.1;
    blur += sampleScene(uv + vec2(-texel.x, 0.0)) * 0.1;
    blur += sampleScene(uv + vec2(0.0,  texel.y)) * 0.1;
    blur += sampleScene(uv + vec2(0.0, -texel.y)) * 0.1;
    blur += sampleScene(uv + vec2( texel.x,  texel.y)) * 0.1;
    blur += sampleScene(uv + vec2(-texel.x,  texel.y)) * 0.1;
    blur += sampleScene(uv + vec2( texel.x, -texel.y)) * 0.1;
    blur += sampleScene(uv + vec2(-texel.x, -texel.y)) * 0.1;
    return blur * uBloomStrength;
}

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
        if (abs(uChromaticAberration) > 0.00001) {
            float dist = length(TexCoords - 0.5);
            float edgeFade = smoothstep(0.0, 0.35, dist) * (1.0 - smoothstep(0.85, 0.98, dist));
            vec2 ca = uChromaticAberration * edgeFade * (TexCoords - 0.5);
            float r = applySharpen(shakenUV + ca, uSharpness).r;
            float g = applySharpen(shakenUV, uSharpness).g;
            float b = applySharpen(shakenUV - ca, uSharpness).b;
            hdrColor = vec3(r, g, b);
        } else {
            hdrColor = applySharpen(shakenUV, uSharpness);
        }
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

    // Bloom
    if (uBloomStrength > 0.001) {
        hdrColor += computeBloom(shakenUV);
    }

    // Color grading in HDR
    hdrColor = max(hdrColor, vec3(0.0));
    hdrColor = applyWhiteBalance(hdrColor, uWhiteBalanceTemp, uWhiteBalanceTint);
    hdrColor = applyLiftGammaGain(hdrColor, uLift, uGammaLift, uGain);
    hdrColor = max(hdrColor, vec3(0.0));

    // Filmic tonemapping (ACES) with exposure
    vec3 mapped = ACESFilm(hdrColor * exposure);

    // LDR adjustments
    mapped = applyContrastBrightness(mapped, uContrast, uBrightness);
    mapped = applySaturation(mapped, uSaturation);
    mapped = applyVibrance(mapped, uVibrance);
    mapped = clamp(mapped, 0.0, 1.0);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    // Explosion vignette
    float vig = 1.0;
    if (uExplosionVignette > 0.001) {
        float d = distance(TexCoords, vec2(0.5));
        float v = smoothstep(0.25, 0.85, d);
        vig = 1.0 - v * uExplosionVignette;
    }

    // Artistic vignette
    if (uVignetteStrength > 0.001) {
        vec2 centered = TexCoords * 2.0 - 1.0;
        float len = length(centered * vec2(uVignetteRoundness, 1.0));
        float v = smoothstep(uVignetteRoundness, uVignetteRoundness + uVignetteSmoothness, len);
        vig *= 1.0 - v * uVignetteStrength;
    }

    FragColor = vec4(mapped * vig, 1.0);
}
