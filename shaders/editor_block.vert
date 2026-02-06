#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;
layout(location = 4) in float aTexLayer;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vColor;
out vec3 vFragPos;
out vec2 vUV;
out float vTexLayer;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    vUV = aUV;
    vTexLayer = aTexLayer;
    gl_Position = uProjection * uView * worldPos;
}
