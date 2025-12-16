#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPositionDepth;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection;

// Parameters (could be uniforms)
const int kernelSize = 64;

uniform vec2 noiseScale;
uniform float radius;
uniform float bias;
uniform float radiusScaleFactor; // scales radius with view depth and AO strength

// Tile noise texture over screen based on screen dimensions divided by noise size
// noiseScale is set from the CPU to match current framebuffer size (noise texture is 4x4)

vec3 getPosition(vec2 uv) {
    float depth = texture(gPositionDepth, uv).r;
    vec4 clipSpaceLocation = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpaceLocation = invProjection * clipSpaceLocation;
    return viewSpaceLocation.xyz / viewSpaceLocation.w;
} 

void main()
{
    vec3 fragPos = getPosition(TexCoords);
    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    vec3 n = cross(dx, dy);
    vec3 normal = (length(n) < 1e-4) ? vec3(0.0, 0.0, 1.0) : normalize(n); // Robust fallback for flat/degenerate areas

    vec3 randomVec = texture(texNoise, TexCoords * noiseScale).xyz;
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = normalize(cross(normal, tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Scale radius with view depth (fragPos.z is negative moving away from camera) - use absolute depth
    float viewDepth = abs(fragPos.z);
    float scaledRadius = radius * (1.0 + radiusScaleFactor * viewDepth);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position in view-space
        vec3 samplePos = TBN * samples[i]; // tangent->view
        samplePos = fragPos + samplePos * scaledRadius; 

        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

        // skip samples outside the screen (prevents sampling invalid depths)
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) continue;

        // get sample depth (view-space z)
        float sampleDepth = getPosition(offset.xy).z;

        // range check & continuous occlusion (soft contribution instead of hard step)
        float rangeCheck = smoothstep(0.0, 1.0, scaledRadius / (abs(fragPos.z - sampleDepth) + 1e-6));
        float occluder = smoothstep(0.0, scaledRadius, (sampleDepth - samplePos.z) + bias);
        occlusion += occluder * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / float(kernelSize));

    FragColor = occlusion;
}
