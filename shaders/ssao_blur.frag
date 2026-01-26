#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionDepth;
uniform float blurDepthFalloff;

// Near/far planes for linear depth (should match ssao.frag)
const float nearPlane = 0.1;
const float farPlane = 1000.0;

float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float centerDepthRaw = texture(gPositionDepth, TexCoords).r;
    float centerAO = texture(ssaoInput, TexCoords).r;
    
    // Early out for sky
    if (centerDepthRaw >= 0.9999) {
        FragColor = 1.0;
        return;
    }
    
    // Use linear depth for more accurate edge detection
    float centerLinear = linearizeDepth(centerDepthRaw);
    
    float result = 0.0;
    float weightSum = 0.0;

    // 9x9 gaussian-weighted bilateral blur for smoother results
    // Sigma = 2.0 for spatial, adjusted by depth difference
    const int blurRadius = 4;
    
    // Precomputed Gaussian weights for sigma = 2.0
    const float gaussian[9] = float[](
        0.0019, 0.0110, 0.0439, 0.1213, 0.2340, 
        0.1213, 0.0439, 0.0110, 0.0019
    );
    
    // Depth-aware falloff scaled by distance
    float depthSigma = centerLinear * 0.05; // 5% of linear depth as threshold
    depthSigma = max(depthSigma, 0.1); // Minimum threshold
    
    for (int x = -blurRadius; x <= blurRadius; ++x) {
        for (int y = -blurRadius; y <= blurRadius; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = TexCoords + offset;
            
            // Clamp to valid range
            sampleUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
            
            float sampleAO = texture(ssaoInput, sampleUV).r;
            float sampleDepthRaw = texture(gPositionDepth, sampleUV).r;
            
            // Skip sky samples
            if (sampleDepthRaw >= 0.9999) continue;
            
            float sampleLinear = linearizeDepth(sampleDepthRaw);
            
            // Linear depth-based weight (bilateral)
            float depthDiff = abs(centerLinear - sampleLinear);
            float depthWeight = exp(-(depthDiff * depthDiff) / (2.0 * depthSigma * depthSigma));
            
            // Spatial gaussian weight (use 1D lookup for separable approximation)
            float spatialWeight = gaussian[x + blurRadius] * gaussian[y + blurRadius];
            
            // Combined weight
            float w = spatialWeight * depthWeight;
            result += sampleAO * w;
            weightSum += w;
        }
    }
    
    // Fallback to center value if no valid samples
    if (weightSum < 0.0001) {
        FragColor = centerAO;
        return;
    }
    
    FragColor = result / weightSum;
}
