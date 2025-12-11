#version 450 core
layout(location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;
uniform float uTime;

void main() {
    // Rotate stars around X axis (East-West) to match sun/moon
    // Sun moves East -> West. Stars should move with it.
    // uTime is time of day.
    
    // Rotation angle
    // Match day cycle: 2*PI / 2400 = 0.002618
    float angle = uTime * 0.002618; 
    float c = cos(angle);
    float s = sin(angle);
    
    // Rotate around Z axis (matches sun path in XY plane)
    mat3 rot = mat3(
        c, -s, 0.0,
        s, c, 0.0,
        0.0, 0.0, 1.0
    );
    
    vec3 pos = rot * aPos;
    
    // Remove translation from view matrix
    mat4 view = mat4(mat3(uView));
    
    gl_Position = uProjection * view * vec4(pos, 1.0);
    gl_PointSize = 4.0;
}
