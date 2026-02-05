#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec3 vWorldPos;

void main() {
    vUV = aUV;
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = uProjection * uView * worldPos;
}
