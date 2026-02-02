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

out vec3 vNormal;
out vec4 vColor;
out vec3 vWorldPos;

void main() {
    vNormal = normalize(aNormal);
    vColor = aColor;

    // Scale around explosion center and add subtle noise wobble
    vec3 centered = aPos - uCenter;
    vec3 scaled = centered * uScale;
    float n = sin((aPos.x + uNoisePhase) * 2.1) * sin((aPos.y + uNoisePhase) * 1.7) * sin((aPos.z + uNoisePhase) * 2.3);
    vec3 displaced = scaled + vNormal * n * 0.15;

    vec3 worldPos = displaced + uCenter;
    vWorldPos = worldPos;

    vec3 relPos = worldPos - uRenderOrigin;
    gl_Position = uProjection * uView * vec4(relPos, 1.0);
}
