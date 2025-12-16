#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPositionDepth;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection;
uniform vec2 noiseScale; // screenSize / noiseTexSize

// Parameters (tweakable)
uniform float ssaoRadius; // e.g. 1.0
uniform float ssaoBias;   // e.g. 0.025
uniform int kernelSize;

vec3 getPosition(vec2 uv) {
    float depth = texture(gPositionDepth, uv).r;
    vec4 clipSpaceLocation = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpaceLocation = invProjection * clipSpaceLocation;
    return viewSpaceLocation.xyz / viewSpaceLocation.w;
}

void main()
{
    vec3 fragPos = getPosition(TexCoords);
    // Reconstruct a more stable normal by sampling neighbor positions (sobel-like)
    vec2 texel = 1.0 / vec2(textureSize(gPositionDepth, 0));
    vec3 pL = getPosition(TexCoords + vec2(-texel.x, 0.0));
    vec3 pR = getPosition(TexCoords + vec2(texel.x, 0.0));
    vec3 pU = getPosition(TexCoords + vec2(0.0, -texel.y));
    vec3 pD = getPosition(TexCoords + vec2(0.0, texel.y));
    vec3 normal = normalize(cross(pR - pL, pD - pU));
    if (!all(isfinite(normal))) {
        normal = vec3(0.0, 0.0, 1.0);
    }
    
    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz * 2.0 - 1.0);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position in view-space
        vec3 sampleVec = TBN * samples[i]; // local to view-space
        vec3 samplePos = fragPos + sampleVec * ssaoRadius; 
        
        // project sample position to screen
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;
        
        // if outside the screen, skip
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;
        
        // get depth at sample
        vec3 sampleViewPos = getPosition(sampleUV);
        float sampleDepth = sampleViewPos.z;
        
        // range-based weighting
        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / abs(fragPos.z - sampleDepth + 1e-6));
        
        // angle-based weighting: prefer samples facing the normal
        float nDot = max(0.0, dot(normal, normalize(sampleVec)));
        float weight = nDot * rangeCheck;
        
        // depth test: if sample is occluding (is closer than samplePos.z + bias) then count
        if (sampleDepth >= samplePos.z + ssaoBias) {
            occlusion += weight;
        }
    }
    // Normalize by sum of weights (approx kernel size but safer to accumulate weights)
    // For simplicity, divide by float(kernelSize) but we attenuate by typical weight range
    occlusion = 1.0 - clamp(occlusion / float(kernelSize), 0.0, 1.0);
    
    FragColor = occlusion;
}
