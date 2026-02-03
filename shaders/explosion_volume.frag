#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec4 vColor;
in vec3 vWorldPos;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform vec3 uCenter;
uniform float uAge;
uniform float uDuration;
uniform float uRadius;
uniform float uNoisePhase;
uniform int uVolumeType;

// Simplex-like noise for smooth flame animation (used for block fire only)
float hash(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n = mix(
        mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x),
            mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
        mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
            mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
    return n;
}

float fbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3D(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    float t = clamp(uAge / uDuration, 0.0, 1.0);
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float light = max(dot(N, L), 0.0);

    float dist = length(vWorldPos - uCenter);
    float radiusSafe = max(uRadius, 0.001);

    float heightBasic = clamp((vWorldPos.y - uCenter.y) / radiusSafe + 0.5, 0.0, 1.0);
    float heightFlame = clamp((vWorldPos.y - uCenter.y + uRadius * 0.5) / max(uRadius * 1.5, 0.001), 0.0, 1.0);

    vec3 fireColor = vec3(1.0, 0.32, 0.08);
    vec3 fireHot = vec3(1.0, 0.9, 0.6);
    vec3 smokeColor = vec3(0.12, 0.12, 0.12);

    float core = pow(clamp(1.0 - dist / radiusSafe, 0.0, 1.0), 2.0);

    // === BLOCK FIRE (realistic flame) ===
    if (uVolumeType == 2) {
        float edgeFade = smoothstep(1.1, 0.5, dist / radiusSafe);

        vec3 fireWhite = vec3(1.0, 0.95, 0.85);
        vec3 fireYellow = vec3(1.0, 0.85, 0.25);
        vec3 fireOrange = vec3(1.0, 0.45, 0.08);
        vec3 fireRed = vec3(0.85, 0.12, 0.02);
        vec3 fireDark = vec3(0.25, 0.03, 0.0);

        vec3 noisePos = vWorldPos * 3.5;
        noisePos.y -= uNoisePhase * 2.8;
        noisePos.x += sin(uNoisePhase * 1.2 + vWorldPos.y * 2.0) * 0.3;
        noisePos.z += cos(uNoisePhase * 0.9 + vWorldPos.y * 2.0) * 0.3;

        float turbulence = fbm(noisePos, 4);
        float detailNoise = fbm(noisePos * 2.0 + vec3(0, uNoisePhase * 1.5, 0), 3);

        float flicker = 0.7 + 0.3 * sin(uNoisePhase * 15.0 + hash(vWorldPos) * 6.28);
        flicker *= 0.85 + 0.15 * sin(uNoisePhase * 23.0 + vWorldPos.x * 10.0);

        float fireLife = smoothstep(0.0, 0.15, 1.0 - t);
        float colorHeight = clamp(heightFlame + (turbulence - 0.5) * 0.4, 0.0, 1.0);

        vec3 flameColor;
        if (colorHeight < 0.25) {
            flameColor = mix(fireWhite, fireYellow, colorHeight * 4.0);
        } else if (colorHeight < 0.5) {
            flameColor = mix(fireYellow, fireOrange, (colorHeight - 0.25) * 4.0);
        } else if (colorHeight < 0.75) {
            flameColor = mix(fireOrange, fireRed, (colorHeight - 0.5) * 4.0);
        } else {
            flameColor = mix(fireRed, fireDark, (colorHeight - 0.75) * 4.0);
        }

        vec3 coreGlow = mix(fireOrange, fireWhite, core * core) * core * 2.0;
        float emissive = (1.0 - heightFlame * 0.6) * (0.5 + core * 1.5) * fireLife;

        vec3 color = flameColor * (0.4 + emissive * 1.2) + coreGlow;
        color *= flicker;

        vec3 V = normalize(uCameraPos - vWorldPos);
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);
        color += fireOrange * fresnel * 0.4 * fireLife;

        float baseAlpha = edgeFade * (0.3 + core * 0.7);
        float turbulenceBreakup = smoothstep(0.3, 0.7, turbulence + detailNoise * 0.5);
        float heightFade = 1.0 - pow(heightFlame, 1.5);

        float alpha = baseAlpha * turbulenceBreakup * heightFade * fireLife * flicker;
        alpha = clamp(alpha * 1.4, 0.0, 0.95);

        float edgeBreak = smoothstep(0.4, 0.6, detailNoise) * smoothstep(0.3, 0.7, 1.0 - dist / radiusSafe);
        alpha *= mix(0.6, 1.0, edgeBreak);

        FragColor = vec4(color, alpha);
        return;
    }

    // === EXPLOSION FIRE (legacy) + SMOKE (legacy) ===
    vec3 emissive = vec3(0.0);
    vec3 baseColor = (uVolumeType == 0) ? fireColor : smokeColor;
    if (uVolumeType == 0) {
        float firePhase = smoothstep(0.0, 0.28, 1.0 - t);
        float hotCore = pow(core, 1.6);
        emissive = (mix(fireColor, fireHot, hotCore)) * (3.6 * firePhase) * (0.6 + hotCore * 1.2);
    }

    vec3 V = normalize(uCameraPos - vWorldPos);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.0);

    float shock = smoothstep(0.0, 0.15, 1.0 - t) * smoothstep(0.2, 0.85, dist / radiusSafe);
    vec3 shockGlow = vec3(1.0, 0.55, 0.15) * shock * 0.8;

    vec3 color = baseColor * (0.22 + 0.78 * light) + emissive + fresnel * 0.3 + shockGlow;
    float fade = smoothstep(1.0, 0.6, t);
    float noise = fract(sin(dot(vWorldPos * 1.7, vec3(12.9898, 78.233, 37.719)) + uNoisePhase) * 43758.5453);
    float smokeNoise = mix(0.75, 1.15, noise);

    float alpha = 0.0;
    if (uVolumeType == 0) {
        float firePhase = smoothstep(0.0, 0.25, 1.0 - t);
        float flameNoise = fract(sin(dot(vWorldPos * 3.1, vec3(12.9898, 78.233, 37.719)) + uNoisePhase) * 43758.5453);
        float flameFlicker = mix(0.75, 1.25, flameNoise);
        vec3 flameBase = mix(fireHot, fireColor, heightBasic);
        color = flameBase * (0.35 + 0.65 * light) + emissive + fresnel * 0.25 + shockGlow;
        float edgeFadeLegacy = smoothstep(1.05, 0.7, dist / radiusSafe);
        alpha = vColor.a * edgeFadeLegacy * fade * (0.55 + core * 0.9) * firePhase * flameFlicker;
    } else {
        color = smokeColor * (0.18 + 0.55 * light) + fresnel * 0.2;
        float smokePhase = smoothstep(0.02, 1.0, t);
        float heightSmoke = smoothstep(0.05, 1.0, heightBasic);
        float smokeEdge = smoothstep(1.2, 0.55, dist / radiusSafe);
        float smokeFade = smoothstep(1.0, 0.65, t);
        float density = (1.6 + core * 0.9) * smokePhase * heightSmoke;
        alpha = vColor.a * smokeEdge * density * smokeNoise * smokeFade;
        alpha = clamp(alpha * 1.9, 0.0, 1.0);
    }

    FragColor = vec4(color, alpha);
}
