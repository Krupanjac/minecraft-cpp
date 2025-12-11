#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform sampler2D historyFrame;
uniform sampler2D depthMap;

uniform mat4 invViewProj;
uniform mat4 prevViewProj;

void main() {
    // 1. Get current depth
    float depth = texture(depthMap, TexCoords).r;
    
    // 2. Reconstruct world position
    vec4 clipSpace = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldSpace = invViewProj * clipSpace;
    worldSpace /= worldSpace.w;
    
    // 3. Reproject to previous frame
    vec4 prevClip = prevViewProj * worldSpace;
    prevClip /= prevClip.w;
    vec2 prevUV = prevClip.xy * 0.5 + 0.5;
    
    // 4. Sample current
    vec3 current = texture(currentFrame, TexCoords).rgb;
    
    // 5. Neighborhood Clamping (Reduces Blur/Ghosting)
    vec3 minColor = current;
    vec3 maxColor = current;
    
    vec2 texSize = vec2(textureSize(currentFrame, 0));
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec3 sampleColor = texture(currentFrame, TexCoords + vec2(x, y) / texSize).rgb;
            minColor = min(minColor, sampleColor);
            maxColor = max(maxColor, sampleColor);
        }
    }

    // Check if reprojected UV is valid
    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
        vec3 history = texture(historyFrame, prevUV).rgb;
        
        // Clamp history to current neighborhood
        history = clamp(history, minColor, maxColor);
        
        // Blend: 0.15 current + 0.50 history
        FragColor = vec4(mix(current, history, 0.50), 1.0);
    } else {
        FragColor = vec4(current, 1.0);
    }
}
