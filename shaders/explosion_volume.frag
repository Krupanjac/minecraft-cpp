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

void main() {
    float t = clamp(uAge / uDuration, 0.0, 1.0);
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float light = max(dot(N, L), 0.0);

    float dist = length(vWorldPos - uCenter);
    float edgeFade = smoothstep(1.05, 0.7, dist / max(uRadius, 0.001));

    float height = clamp((vWorldPos.y - uCenter.y) / max(uRadius, 0.001) + 0.5, 0.0, 1.0);
    vec3 fireColor = vec3(1.0, 0.32, 0.08);
    vec3 fireHot = vec3(1.0, 0.9, 0.6);
    vec3 smokeColor = vec3(0.12, 0.12, 0.12);

    // Emissive core fades over time
    float core = pow(clamp(1.0 - dist / max(uRadius, 0.001), 0.0, 1.0), 2.0);
    vec3 emissive = vec3(0.0);
    vec3 baseColor = (uVolumeType == 0) ? fireColor : smokeColor;
    if (uVolumeType == 0) {
        float firePhase = smoothstep(0.0, 0.28, 1.0 - t);
        float hotCore = pow(core, 1.6);
        emissive = (mix(fireColor, fireHot, hotCore)) * (3.6 * firePhase) * (0.6 + hotCore * 1.2);
    }

    // Soft fresnel for volume feel
    vec3 V = normalize(uCameraPos - vWorldPos);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.0);

    // Shock glow at early phase
    float shock = smoothstep(0.0, 0.15, 1.0 - t) * smoothstep(0.2, 0.85, dist / max(uRadius, 0.001));
    vec3 shockGlow = vec3(1.0, 0.55, 0.15) * shock * 0.8;

    vec3 color = baseColor * (0.22 + 0.78 * light) + emissive + fresnel * 0.3 + shockGlow;
    float fade = smoothstep(1.0, 0.6, t);
    // Noisy breakup for smoke
    float noise = fract(sin(dot(vWorldPos * 1.7, vec3(12.9898, 78.233, 37.719)) + uNoisePhase) * 43758.5453);
    float smokeNoise = mix(0.75, 1.15, noise);

    // Fire is bright and short, smoke is softer and longer
    float alpha = 0.0;
    if (uVolumeType == 0) {
        float firePhase = smoothstep(0.0, 0.25, 1.0 - t);
        alpha = vColor.a * edgeFade * fade * (0.5 + core * 0.9) * firePhase;
    } else {
        // Smoke uses smokeColor only
        color = smokeColor * (0.18 + 0.55 * light) + fresnel * 0.2;
        float smokePhase = smoothstep(0.02, 1.0, t);
        float heightSmoke = smoothstep(0.05, 1.0, height);
        float smokeEdge = smoothstep(1.2, 0.55, dist / max(uRadius, 0.001));
        float smokeFade = smoothstep(1.0, 0.65, t); // fade out near end
        float density = (1.6 + core * 0.9) * smokePhase * heightSmoke;
        alpha = vColor.a * smokeEdge * density * smokeNoise * smokeFade;
        alpha = clamp(alpha * 1.9, 0.0, 1.0);
    }

    FragColor = vec4(color, alpha);
}
