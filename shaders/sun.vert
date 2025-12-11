#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 3) in uint aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vTexCoord;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    
    float u = float((aUV >> 8u) & 0xFFu) / 255.0;
    float v = float(aUV & 0xFFu) / 255.0;
    vTexCoord = vec2(u, v);
}