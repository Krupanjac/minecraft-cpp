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

void main() {
    float t = clamp(uAge / uDuration, 0.0, 1.0);
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float light = max(dot(N, L), 0.0);

    float dist = length(vWorldPos - uCenter);
    float edgeFade = smoothstep(1.05, 0.7, dist / max(uRadius, 0.001));

    // Height-based smoke blend
    float height = clamp((vWorldPos.y - uCenter.y) / max(uRadius, 0.001) + 0.5, 0.0, 1.0);
    vec3 fireColor = vec3(1.0, 0.45, 0.15);
    vec3 smokeColor = vec3(0.18, 0.18, 0.18);
    vec3 baseColor = mix(fireColor, smokeColor, smoothstep(0.35, 1.0, height + t * 0.3));

    // Emissive core fades over time
    float core = pow(clamp(1.0 - dist / max(uRadius, 0.001), 0.0, 1.0), 2.0);
    vec3 emissive = baseColor * (2.4 - t * 1.1) * (0.6 + core * 0.8);

    // Soft fresnel for volume feel
    vec3 V = normalize(uCameraPos - vWorldPos);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.0);

    vec3 color = baseColor * (0.25 + 0.75 * light) + emissive + fresnel * 0.25;
    float alpha = vColor.a * edgeFade * (1.0 - t * 0.1) * (0.6 + core * 0.6);

    FragColor = vec4(color, alpha);
}
