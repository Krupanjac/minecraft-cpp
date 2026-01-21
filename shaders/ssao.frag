#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPositionDepth;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection;

// Parameters
const int kernelSize = 64;

uniform vec2 noiseScale;
uniform float radius;
uniform float bias;
uniform float radiusScaleFactor;

vec3 getPosition(vec2 uv) {
    float depth = texture(gPositionDepth, uv).r;
    // Skip sky pixels
    if (depth >= 0.9999) return vec3(0.0, 0.0, -1000.0);
    
    vec4 clipSpaceLocation = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpaceLocation = invProjection * clipSpaceLocation;
    return viewSpaceLocation.xyz / viewSpaceLocation.w;
}

vec3 reconstructNormal(vec2 uv, vec3 fragPos) {
    vec2 texelSize = 1.0 / vec2(textureSize(gPositionDepth, 0));
    
    // Use central differences for more accurate normal reconstruction
    vec3 posL = getPosition(uv - vec2(texelSize.x, 0.0));
    vec3 posR = getPosition(uv + vec2(texelSize.x, 0.0));
    vec3 posU = getPosition(uv - vec2(0.0, texelSize.y));
    vec3 posD = getPosition(uv + vec2(0.0, texelSize.y));
    
    // Pick the pair with smaller depth difference for edge preservation
    vec3 dx = (abs(posR.z - fragPos.z) < abs(fragPos.z - posL.z)) ? (posR - fragPos) : (fragPos - posL);
    vec3 dy = (abs(posD.z - fragPos.z) < abs(fragPos.z - posU.z)) ? (posD - fragPos) : (fragPos - posU);
    
    vec3 normal = cross(dx, dy);
    float len = length(normal);
    if (len < 1e-6) return vec3(0.0, 0.0, 1.0);
    return normalize(normal);
}

void main()
{
    vec3 fragPos = getPosition(TexCoords);
    
    // Early out for sky
    if (fragPos.z < -500.0) {
        FragColor = 1.0;
        return;
    }
    
    vec3 normal = reconstructNormal(TexCoords, fragPos);
    
    // Get random rotation vector
    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz * 2.0 - 1.0);
    
    // Create TBN matrix using Gram-Schmidt
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Scale radius with view depth for consistent screen-space size
    float viewDepth = -fragPos.z; // View space z is negative
    float scaledRadius = radius * (1.0 + radiusScaleFactor * viewDepth * 0.01);
    scaledRadius = clamp(scaledRadius, 0.1, 5.0);

    float occlusion = 0.0;
    int validSamples = 0;
    
    for(int i = 0; i < kernelSize; ++i)
    {
        // Get sample position in view-space
        vec3 sampleOffset = TBN * samples[i];
        vec3 samplePos = fragPos + sampleOffset * scaledRadius; 

        // Project sample position
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Skip samples outside screen
        if (offset.x < 0.001 || offset.x > 0.999 || offset.y < 0.001 || offset.y > 0.999) continue;

        // Get sample depth
        float sampleDepth = getPosition(offset.xy).z;
        
        // Skip invalid samples (sky)
        if (sampleDepth < -500.0) continue;
        
        validSamples++;

        // Range check with smooth falloff
        float rangeCheck = smoothstep(0.0, 1.0, scaledRadius / (abs(fragPos.z - sampleDepth) + 0.001));
        
        // Occlusion test - sample is occluding if it's closer to camera than the test position
        // sampleDepth is more negative = closer to camera
        // samplePos.z is the test point
        float depthDiff = sampleDepth - samplePos.z + bias;
        float occluder = step(0.0, depthDiff) * rangeCheck;
        
        occlusion += occluder;
    }
    
    // Normalize by valid samples
    if (validSamples > 0) {
        occlusion = occlusion / float(validSamples);
    }
    
    // Output: 1 = no occlusion, 0 = full occlusion
    FragColor = 1.0 - occlusion;
}

