#version 450 core

layout(location = 0) in vec3 aPos;       // Quad vertex position (-0.5 to 0.5)
layout(location = 1) in vec2 aTexCoord;  // Quad UV (0 to 1)

// Per-instance data
layout(location = 2) in vec3 aWorldPos;     // Particle world position
layout(location = 3) in vec2 aSize;         // Particle size (width, height)
layout(location = 4) in vec4 aAtlasInfo;    // x=frame, y=totalFrames, z=columns, w=rows
layout(location = 5) in vec4 aColor;        // RGBA tint
layout(location = 6) in float aRotation;    // Rotation angle in radians

out vec2 vTexCoord;
out vec4 vColor;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraRight;  // Camera right vector for billboarding
uniform vec3 uCameraUp;     // Camera up vector for billboarding

void main() {
    // Calculate atlas UV
    float frame = aAtlasInfo.x;
    float columns = aAtlasInfo.z;
    float rows = aAtlasInfo.w;
    
    int frameInt = int(frame);
    int col = frameInt % int(columns);
    int row = frameInt / int(columns);
    
    float cellWidth = 1.0 / columns;
    float cellHeight = 1.0 / rows;
    
    // Map quad UV (0-1) to atlas cell
    vec2 atlasUV;
    atlasUV.x = (float(col) + aTexCoord.x) * cellWidth;
    atlasUV.y = (float(row) + aTexCoord.y) * cellHeight;
    
    vTexCoord = atlasUV;
    vColor = aColor;
    
    // Apply rotation to quad position
    float cosR = cos(aRotation);
    float sinR = sin(aRotation);
    vec2 rotatedPos;
    rotatedPos.x = aPos.x * cosR - aPos.y * sinR;
    rotatedPos.y = aPos.x * sinR + aPos.y * cosR;
    
    // Billboard: construct world position facing camera
    vec3 vertexPos = aWorldPos 
                   + uCameraRight * rotatedPos.x * aSize.x
                   + uCameraUp * rotatedPos.y * aSize.y;
    
    gl_Position = uProjection * uView * vec4(vertexPos, 1.0);
}
