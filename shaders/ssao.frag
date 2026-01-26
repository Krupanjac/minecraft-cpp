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

// Near/far planes for linear depth
const float nearPlane = 0.1;
const float farPlane = 1000.0;

// Convert depth buffer value to linear view-space depth
float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0; // NDC
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

vec3 getPosition(vec2 uv) {
    float depth = texture(gPositionDepth, uv).r;
    // Skip sky pixels
    if (depth >= 0.9999) return vec3(0.0, 0.0, -1000.0);
    
    vec4 clipSpaceLocation = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpaceLocation = invProjection * clipSpaceLocation;
    return viewSpaceLocation.xyz / viewSpaceLocation.w;
}

float getLinearDepth(vec2 uv) {
    float depth = texture(gPositionDepth, uv).r;
    if (depth >= 0.9999) return farPlane;
    return linearizeDepth(depth);
}

vec3 reconstructNormal(vec2 uv, vec3 fragPos) {
    vec2 texelSize = 1.0 / vec2(textureSize(gPositionDepth, 0));
    
    // Get linear depths for comparison (more accurate at depth edges)
    float centerLinear = getLinearDepth(uv);
    float leftLinear = getLinearDepth(uv - vec2(texelSize.x, 0.0));
    float rightLinear = getLinearDepth(uv + vec2(texelSize.x, 0.0));
    float upLinear = getLinearDepth(uv - vec2(0.0, texelSize.y));
    float downLinear = getLinearDepth(uv + vec2(0.0, texelSize.y));
    
    // Threshold for edge detection (in world units)
    float edgeThreshold = centerLinear * 0.1; // 10% of depth
    
    // Get positions
    vec3 posL = getPosition(uv - vec2(texelSize.x, 0.0));
    vec3 posR = getPosition(uv + vec2(texelSize.x, 0.0));
    vec3 posU = getPosition(uv - vec2(0.0, texelSize.y));
    vec3 posD = getPosition(uv + vec2(0.0, texelSize.y));
    
    // Check for depth discontinuities using linear depth
    bool leftValid = abs(leftLinear - centerLinear) < edgeThreshold;
    bool rightValid = abs(rightLinear - centerLinear) < edgeThreshold;
    bool upValid = abs(upLinear - centerLinear) < edgeThreshold;
    bool downValid = abs(downLinear - centerLinear) < edgeThreshold;
    
    // Choose best derivative for X
    vec3 dx;
    if (leftValid && rightValid) {
        dx = (posR - posL) * 0.5; // Central difference
    } else if (rightValid) {
        dx = posR - fragPos; // Forward difference
    } else if (leftValid) {
        dx = fragPos - posL; // Backward difference
    } else {
        dx = vec3(1.0, 0.0, 0.0); // Fallback
    }
    
    // Choose best derivative for Y
    vec3 dy;
    if (upValid && downValid) {
        dy = (posD - posU) * 0.5;
    } else if (downValid) {
        dy = posD - fragPos;
    } else if (upValid) {
        dy = fragPos - posU;
    } else {
        dy = vec3(0.0, 1.0, 0.0);
    }
    
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
    float linearDepth = getLinearDepth(TexCoords);
    
    // Get random rotation vector
    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz * 2.0 - 1.0);
    
    // Create TBN matrix using Gram-Schmidt
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Scale radius with distance for consistent visual size
    // Use linear depth for more predictable scaling
    float depthFactor = linearDepth / 10.0; // Normalize around 10 units
    float scaledRadius = radius * (1.0 + radiusScaleFactor * clamp(depthFactor, 0.0, 5.0));
    scaledRadius = clamp(scaledRadius, 0.05, 3.0);

    float occlusion = 0.0;
    int validSamples = 0;
    
    // Adaptive bias based on depth and surface angle
    float surfaceBias = bias * (1.0 + linearDepth * 0.01);
    
    for(int i = 0; i < kernelSize; ++i)
    {
        // Get sample position in view-space
        vec3 sampleOffset = TBN * samples[i];
        vec3 samplePos = fragPos + sampleOffset * scaledRadius; 

        // Project sample position
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Skip samples outside screen with margin
        if (offset.x < 0.005 || offset.x > 0.995 || offset.y < 0.005 || offset.y > 0.995) continue;

        // Get sample depth
        vec3 sampleViewPos = getPosition(offset.xy);
        
        // Skip invalid samples (sky)
        if (sampleViewPos.z < -500.0) continue;
        
        validSamples++;
        
        float sampleDepth = sampleViewPos.z;

        // Range check using linear depth for more natural falloff
        float sampleLinear = getLinearDepth(offset.xy);
        float depthDiff = abs(linearDepth - sampleLinear);
        
        // Smooth range check - falloff based on actual world distance
        float rangeCheck = 1.0 - smoothstep(scaledRadius * 0.5, scaledRadius * 2.0, depthDiff);
        
        // Occlusion test with adaptive bias
        // sampleDepth is more negative = closer to camera
        float occlusionDepthDiff = sampleDepth - samplePos.z;
        
        // Soft occlusion falloff instead of hard step
        float occluder = smoothstep(-surfaceBias, surfaceBias * 2.0, occlusionDepthDiff);
        occluder *= rangeCheck;
        
        occlusion += occluder;
    }
    
    // Normalize by valid samples
    if (validSamples > 0) {
        occlusion = occlusion / float(validSamples);
    } else {
        FragColor = 1.0;
        return;
    }
    
    // Apply subtle curve for more natural appearance
    occlusion = pow(occlusion, 0.8);
    
    // Clamp to prevent over-darkening
    occlusion = clamp(occlusion, 0.0, 0.85);
    
    // Output: 1 = no occlusion, 0 = full occlusion
    FragColor = 1.0 - occlusion;
}

