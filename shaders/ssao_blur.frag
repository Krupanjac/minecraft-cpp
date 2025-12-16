#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionDepth; // used for depth-aware/bilateral blur
uniform float blurDepthFalloff;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float centerDepth = texture(gPositionDepth, TexCoords).r;
    float result = 0.0;
    float weightSum = 0.0;

    // 5x5 bilateral-style blur (depth-weighted)
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float sample = texture(ssaoInput, TexCoords + offset).r;
            float sampleDepth = texture(gPositionDepth, TexCoords + offset).r;
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthWeight = exp(-depthDiff * blurDepthFalloff); // Tunable: higher -> more edge preservation
            float kernelWeight = 1.0; // box kernel; could be gaussian for nicer falloff
            float w = kernelWeight * depthWeight;
            result += sample * w;
            weightSum += w;
        }
    }
    FragColor = result / max(weightSum, 1e-6);
}
