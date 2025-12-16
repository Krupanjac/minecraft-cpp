#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;
uniform sampler2D depthMap;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float centerDepth = texture(depthMap, TexCoords).r;

    float sum = 0.0;
    float weightSum = 0.0;

    // Bilateral-like blur: weight by spatial distance and depth similarity
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float sample = texture(ssaoInput, TexCoords + offset).r;
            float sampleDepth = texture(depthMap, TexCoords + offset).r;
            float dDepth = abs(centerDepth - sampleDepth);
            float wDepth = exp(-dDepth * 80.0); // depth weight (tune)
            float wSpatial = exp(- (float(x*x + y*y)) / 8.0); // gaussian spatial
            float w = wDepth * wSpatial;
            sum += sample * w;
            weightSum += w;
        }
    }

    FragColor = sum / max(1e-5, weightSum);
}
