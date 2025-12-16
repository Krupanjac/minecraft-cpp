#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform sampler2D historyFrame;
uniform sampler2D depthMap;
uniform sampler2D velocityMap;
uniform sampler2D historyDepthMap;

uniform mat4 invViewProj;
uniform mat4 prevViewProj;

// Camera movement for velocity-based rejection
uniform vec3 cameraDelta;
uniform float nearPlane;
uniform float farPlane;

// Convert depth buffer value to linear depth
float linearizeDepth(float d) {
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - d * (farPlane - nearPlane));
}

void main() {
    // 1. Get current depth
    float depth = texture(depthMap, TexCoords).r;
    
    // Handle sky (depth == 1.0)
    if (depth >= 0.9999) {
        vec3 current = texture(currentFrame, TexCoords).rgb;
        // For sky, use simpler blending with less history weight
        vec2 prevUV = TexCoords; // Sky is at infinity, so no reprojection needed
        if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
            vec3 history = texture(historyFrame, prevUV).rgb;
            FragColor = vec4(mix(current, history, 0.5), 1.0); // Less history for sky
        } else {
            FragColor = vec4(current, 1.0);
        }
        return;
    }
    
    // 2. Get Velocity from buffer (Screen Space Velocity)
    vec2 velocity = texture(velocityMap, TexCoords).rg;
    vec2 prevUV = TexCoords - velocity;
    
    // Calculate motion vector for velocity weighting
    vec2 motionVector = velocity;
    float motionMagnitude = length(motionVector);
    
    // 4. Sample current
    vec3 current = texture(currentFrame, TexCoords).rgb;
    
    // 5. Neighborhood Clamping with VARIANCE clipping (better than min/max)
    vec3 m1 = vec3(0.0); // Mean
    vec3 m2 = vec3(0.0); // Variance
    
    vec2 texSize = vec2(textureSize(currentFrame, 0));
    vec2 texelSize = 1.0 / texSize;
    
    // 3x3 neighborhood sampling
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec3 sampleColor = texture(currentFrame, TexCoords + vec2(x, y) * texelSize).rgb;
            m1 += sampleColor;
            m2 += sampleColor * sampleColor;
        }
    }
    
    m1 /= 9.0;
    m2 /= 9.0;
    
    // Calculate standard deviation
    vec3 sigma = sqrt(max(m2 - m1 * m1, vec3(0.0)));
    
    // Variance clipping box (gamma correction friendly)
    float gamma = 1.25; // Slightly larger box to reduce artifacts
    vec3 minColor = m1 - gamma * sigma;
    vec3 maxColor = m1 + gamma * sigma;

    // Check if reprojected UV is valid
    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
        vec3 history = texture(historyFrame, prevUV).rgb;
        
        // Depth rejection: sample previous depth and compare
        float historyDepth = texture(historyDepthMap, prevUV).r;
        float currentLinearDepth = linearizeDepth(depth);
        float historyLinearDepth = linearizeDepth(historyDepth);
        
        // Calculate depth difference threshold (relative)
        float depthThreshold = currentLinearDepth * 0.02; // 2% of depth
        bool depthValid = abs(currentLinearDepth - historyLinearDepth) < depthThreshold;
        
        // Clamp history to current neighborhood (variance clipping)
        history = clamp(history, minColor, maxColor);
        
        // Adaptive blend factor based on motion and depth validity
        float baseBlend = 0.9; // 90% history, 10% current
        float blend = baseBlend;
        
        // Reduce history weight for fast motion
        blend = mix(baseBlend, 0.3, smoothstep(0.001, 0.02, motionMagnitude));
        
        // Reduce history weight if depth changed significantly (disocclusion)
        if (!depthValid) {
            blend = min(blend, 0.3);
        }
        
        FragColor = vec4(mix(current, history, blend), 1.0);
    } else {
        // Outside screen, use current frame
        FragColor = vec4(current, 1.0);
    }
}
