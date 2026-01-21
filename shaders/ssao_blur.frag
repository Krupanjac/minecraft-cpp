#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionDepth;
uniform float blurDepthFalloff;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float centerDepth = texture(gPositionDepth, TexCoords).r;
    float centerAO = texture(ssaoInput, TexCoords).r;
    
    // Early out for sky
    if (centerDepth >= 0.9999) {
        FragColor = 1.0;
        return;
    }
    
    float result = 0.0;
    float weightSum = 0.0;

    // 7x7 gaussian-weighted bilateral blur for smoother results
    const float gaussianKernel[7] = float[](0.0044, 0.054, 0.242, 0.398, 0.242, 0.054, 0.0044);
    
    for (int x = -3; x <= 3; ++x) {
        for (int y = -3; y <= 3; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = TexCoords + offset;
            
            float sampleAO = texture(ssaoInput, sampleUV).r;
            float sampleDepth = texture(gPositionDepth, sampleUV).r;
            
            // Skip sky samples
            if (sampleDepth >= 0.9999) continue;
            
            // Depth-based weight (bilateral)
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthWeight = exp(-depthDiff * blurDepthFalloff);
            
            // Spatial gaussian weight
            float spatialWeight = gaussianKernel[x + 3] * gaussianKernel[y + 3];
            
            float w = spatialWeight * depthWeight;
            result += sampleAO * w;
            weightSum += w;
        }
    }
    
    FragColor = (weightSum > 0.0001) ? (result / weightSum) : centerAO;
}
