#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;

out vec2 TexCoord;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

const int MAX_JOINTS = 100;
uniform mat4 uJoints[MAX_JOINTS];
uniform bool uHasSkin;

void main() {
    vec4 totalLocalPos = vec4(0.0);
    
    if (uHasSkin) {
        for(int i = 0; i < 4; i++) {
            int jointIndex = int(aJoints[i]);
            if (jointIndex >= 0 && jointIndex < MAX_JOINTS) {
                mat4 jointMat = uJoints[jointIndex];
                float weight = aWeights[i];
                totalLocalPos += jointMat * vec4(aPos, 1.0) * weight;
            }
        }
    } else {
        totalLocalPos = vec4(aPos, 1.0);
    }
    
    TexCoord = aTexCoord;
    gl_Position = uLightSpaceMatrix * uModel * totalLocalPos;
}
