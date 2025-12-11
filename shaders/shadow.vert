#version 450 core
layout (location = 0) in vec3 aPos;
// Other attributes are ignored for shadow pass

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

void main() {
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
