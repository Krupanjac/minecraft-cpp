#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uRenderOrigin;
uniform vec3 uCenter;
uniform float uScale;
uniform float uNoisePhase;
uniform float uAge;
uniform float uDuration;
uniform float uRadius;
uniform int uVolumeType;

out vec3 vNormal;
out vec4 vColor;
out vec3 vWorldPos;

void main() {
    vNormal = normalize(aNormal);
    vColor = aColor;

    float t = clamp(uAge / max(uDuration, 0.001), 0.0, 1.0);

    // Scale around explosion center and add subtle noise wobble
    vec3 centered = aPos - uCenter;
    // Stretch upward to avoid perfect sphere
    vec3 stretched = centered * vec3(1.0, 1.25, 1.0);
    vec3 scaled = stretched * uScale;

    // Turbulence to break spherical shape
    float n1 = sin((aPos.x + uNoisePhase) * 2.3) * sin((aPos.y + uNoisePhase) * 1.9) * sin((aPos.z + uNoisePhase) * 2.7);
    float n2 = sin((aPos.x - uNoisePhase) * 3.1) * sin((aPos.y + uNoisePhase) * 2.4) * sin((aPos.z - uNoisePhase) * 3.3);
    float n = (n1 * 0.6 + n2 * 0.4);
    vec3 displaced = scaled + vNormal * n * 0.22 + vec3(0.0, n * 0.18, 0.0);

    // Buoyant rise + lateral drift for smoke
    float smokeFactor = (uVolumeType == 1) ? 1.0 : 0.0;
    float rise = t * t * uRadius * mix(0.15, 0.45, smokeFactor);
    vec3 drift = vec3(sin(uNoisePhase * 0.7 + centered.y * 0.15), 0.0,
                      cos(uNoisePhase * 0.8 + centered.y * 0.12)) * (t * t) * mix(0.08, 0.35, smokeFactor);
    displaced += vec3(0.0, rise, 0.0) + drift;

    vec3 worldPos = displaced + uCenter;
    vWorldPos = worldPos;

    vec3 relPos = worldPos - uRenderOrigin;
    gl_Position = uProjection * uView * vec4(relPos, 1.0);
}
