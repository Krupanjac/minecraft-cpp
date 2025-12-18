#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 uModel;
uniform mat4 uPrevModel;
uniform mat4 uView;
uniform mat4 uProjection;

uniform mat4 uPrevView;
uniform mat4 uPrevProjection;

out vec4 vCurrentClip;
out vec4 vPrevClip;

const int MAX_JOINTS = 100;
uniform mat4 uJoints[MAX_JOINTS];
uniform mat4 uPrevJoints[MAX_JOINTS];
uniform bool uHasSkin;

void main() {
    vec4 totalLocalPos = vec4(0.0);
    vec4 totalLocalPosPrev = vec4(0.0);
    vec3 totalNormal = vec3(0.0);
    
    if (uHasSkin) {
        for(int i = 0; i < 4; i++) {
            int jointIndex = int(aJoints[i]);
            if (jointIndex >= 0 && jointIndex < MAX_JOINTS) {
                mat4 jointMat = uJoints[jointIndex];
                mat4 prevJointMat = uPrevJoints[jointIndex];
                float weight = aWeights[i];
                
                totalLocalPos += jointMat * vec4(aPos, 1.0) * weight;
                totalLocalPosPrev += prevJointMat * vec4(aPos, 1.0) * weight;
                vec3 worldNormal = mat3(jointMat) * aNormal;
                totalNormal += worldNormal * weight;
            }
        }
    } else {
        totalLocalPos = vec4(aPos, 1.0);
        totalLocalPosPrev = totalLocalPos;
        totalNormal = aNormal;
    }

    FragPos = vec3(uModel * totalLocalPos);
    
    // Normal matrix should ideally be passed from CPU, but for now:
    // If skinning, 'uModel' applies after skinning.
    // If not skinning, uModel applies to raw pos. (matches branch else)
    
    // Simplification: uModel is the Entity transform. Skinning happens in Model space relative to uModel.
    // Actually skinning transforms into Model space. So uModel is correct.
    
    Normal = mat3(transpose(inverse(uModel))) * totalNormal; // Should optimize
    TexCoord = aTexCoord;
    
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
    
    vCurrentClip = gl_Position;
    // Previous position (entity motion + animation)
    vec4 prevWorldPos = uPrevModel * totalLocalPosPrev;
    vPrevClip = uPrevProjection * uPrevView * prevWorldPos;
}
