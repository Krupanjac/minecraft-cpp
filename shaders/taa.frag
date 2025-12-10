#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform sampler2D historyFrame;
uniform sampler2D depthMap;

uniform mat4 invViewProj;
uniform mat4 prevViewProj;

void main() {
    // DEBUG: Passthrough current frame to verify FBO chain
    FragColor = texture(currentFrame, TexCoords);
    return;

    /*
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
    
    // 4. Sample history
    vec3 historyColor = texture(historyFrame, prevUV).rgb;
    vec3 currentColor = texture(currentFrame, TexCoords).rgb;
    
    // 5. Neighborhood clamping (to reduce ghosting)
    vec3 minColor = currentColor;
    vec3 maxColor = currentColor;
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec3 sampleColor = texture(currentFrame, TexCoords + vec2(x, y) / vec2(textureSize(currentFrame, 0))).rgb;
            minColor = min(minColor, sampleColor);
            maxColor = max(maxColor, sampleColor);
        }
    }
    
    historyColor = clamp(historyColor, minColor, maxColor);
    
    // 6. Blend
    float blendFactor = 0.1; // 10% current, 90% history
    // Check if history is valid
    if(prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        blendFactor = 1.0;
    }
    
    FragColor = vec4(mix(historyColor, currentColor, blendFactor), 1.0);
    */
}
