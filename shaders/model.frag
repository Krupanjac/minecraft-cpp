#version 450 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 vCurrentClip;
in vec4 vPrevClip;
in vec4 vFragPosLightSpace;
in vec3 WorldPos;

uniform sampler2D uAlbedoMap;
uniform bool uHasTexture;
uniform vec4 uBaseColor;

uniform sampler2D uEmissiveMap;
uniform bool uHasEmissive;

// Shadow mapping
uniform sampler2D uShadowMap;
uniform int uUseShadows;

// Debug uniforms
uniform int uDebugNoTexture;
uniform int uDebugShowNormals;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uAlphaMultiplier; // For death fade effect

// Blood decal projection system - decals are spheres projected onto the model surface
const int MAX_BLOOD_DECALS = 32;
uniform int uBloodDecalCount;
uniform vec3 uBloodDecalPos[MAX_BLOOD_DECALS];      // World position of decal center
uniform vec3 uBloodDecalNormal[MAX_BLOOD_DECALS];   // Direction decal faces (for backface check)
uniform float uBloodDecalRadius[MAX_BLOOD_DECALS];  // Radius of influence
uniform float uBloodDecalSeed[MAX_BLOOD_DECALS];    // Random seed for pattern
uniform float uBloodDecalAlpha[MAX_BLOOD_DECALS];   // Alpha/intensity

// Hash functions for procedural blood pattern
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p, float seed) {
    float v = 0.0;
    float a = 0.5;
    p += seed * 13.7;
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p = p * 2.0 + vec2(seed * 3.1, seed * 7.3);
        a *= 0.5;
    }
    return v;
}

// Generate realistic blood splatter pattern
float bloodPattern(vec2 uv, float seed) {
    vec2 c = uv - 0.5;
    float d = length(c);
    float angle = atan(c.y, c.x);
    
    // Main splatter shape with very irregular edges
    float edgeNoise = fbm(vec2(angle * 4.0 + seed * 10.0, seed * 5.0), seed);
    float edgeNoise2 = fbm(vec2(angle * 8.0 - seed * 7.0, seed * 3.0), seed + 50.0);
    float radius = 0.32 + edgeNoise * 0.15 + edgeNoise2 * 0.08;
    
    // Soft organic edge
    float blob = smoothstep(radius + 0.08, radius - 0.12, d);
    
    // Add radiating tendrils/spray
    float tendrils = 0.0;
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        float tendrilAngle = hash11(seed + fi * 1.7) * 6.28;
        float tendrilLen = hash11(seed + fi + 0.3) * 0.3 + 0.15;
        float tendrilWidth = hash11(seed + fi + 0.7) * 0.04 + 0.02;
        vec2 tendrilDir = vec2(cos(tendrilAngle), sin(tendrilAngle));
        float along = dot(c, tendrilDir);
        float perp = abs(dot(c, vec2(-tendrilDir.y, tendrilDir.x)));
        
        // Tapered tendril
        float taper = smoothstep(tendrilLen, 0.0, along) * smoothstep(0.0, 0.05, along);
        float widthAtPoint = tendrilWidth * (1.0 - along / tendrilLen * 0.7);
        float tendril = smoothstep(widthAtPoint, widthAtPoint * 0.3, perp) * taper;
        tendrils = max(tendrils, tendril * 0.8);
    }
    
    // Add drip trails going downward
    float drips = 0.0;
    for (int i = 0; i < 3; i++) {
        float di = float(i);
        float dripX = (hash11(seed + di * 3.0) - 0.5) * 0.5;
        float dripStartY = hash11(seed + di * 3.0 + 1.0) * 0.2 - 0.1;
        float dripLen = hash11(seed + di * 3.0 + 2.0) * 0.25 + 0.1;
        float dripWidth = hash11(seed + di * 3.0 + 3.0) * 0.02 + 0.015;
        
        // Drip runs downward (negative Y in UV space)
        vec2 dripStart = vec2(dripX, dripStartY);
        float dripDist = c.y - dripStart.y;
        float horizDist = abs(c.x - dripStart.x);
        
        if (dripDist < 0.0 && dripDist > -dripLen && horizDist < dripWidth) {
            float dripTaper = 1.0 - (-dripDist / dripLen);
            float widthTaper = dripWidth * (0.5 + dripTaper * 0.5);
            drips = max(drips, smoothstep(widthTaper, widthTaper * 0.3, horizDist) * dripTaper * 0.7);
        }
    }
    
    // Scattered droplets around the main splatter
    float droplets = 0.0;
    for (int i = 0; i < 8; i++) {
        float di = float(i);
        vec2 dropPos = vec2(hash11(seed + di * 2.0), hash11(seed + di * 2.0 + 1.0)) - 0.5;
        dropPos *= 0.9;  // Keep within bounds
        float dropSize = hash11(seed + di * 3.0) * 0.04 + 0.015;
        float dropDist = length(uv - 0.5 - dropPos);
        droplets = max(droplets, smoothstep(dropSize, dropSize * 0.2, dropDist) * 0.6);
    }
    
    // Internal texture variation (pooling effect)
    float internalNoise = fbm(uv * 8.0 + seed * 2.0, seed) * 0.3;
    float pooling = blob * (0.7 + internalNoise);
    
    return clamp(pooling + tendrils + drips + droplets, 0.0, 1.0);
}

// Calculate blood decal contribution at this fragment
vec4 calculateBloodDecals(vec3 worldPos, vec3 surfaceNormal) {
    vec3 totalBlood = vec3(0.0);
    float totalAlpha = 0.0;
    
    for (int i = 0; i < MAX_BLOOD_DECALS; i++) {
        if (i >= uBloodDecalCount) break;
        
        vec3 decalPos = uBloodDecalPos[i];
        vec3 decalNormal = uBloodDecalNormal[i];
        float radius = uBloodDecalRadius[i];
        float seed = uBloodDecalSeed[i];
        float alpha = uBloodDecalAlpha[i];
        
        // Vector from decal center to fragment
        vec3 toFrag = worldPos - decalPos;
        float dist = length(toFrag);
        
        // Skip if too far (use larger sphere for detection)
        if (dist > radius * 1.5) continue;
        
        // Use SURFACE NORMAL for projection - this wraps the decal around geometry
        vec3 projNormal = surfaceNormal;
        
        // NO backface culling - allow decals on ALL surfaces
        // The sphere-of-influence will naturally show on whatever geometry is nearby
        
        // Build tangent frame from surface normal for proper wrapping
        vec3 up = abs(projNormal.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, projNormal));
        vec3 bitangent = normalize(cross(projNormal, tangent));
        
        // Project onto surface plane for UV
        float u = dot(toFrag, tangent) / radius * 0.5 + 0.5;
        float v = dot(toFrag, bitangent) / radius * 0.5 + 0.5;
        
        // Clamp UVs to create circular pattern
        vec2 centered = vec2(u, v) - 0.5;
        float uvDist = length(centered);
        if (uvDist > 0.5) continue;
        
        // Distance falloff (spherical, smooth edge)
        float distFalloff = 1.0 - smoothstep(0.0, radius, dist);
        
        // Radial falloff for circular shape
        float radialFalloff = 1.0 - smoothstep(0.3, 0.5, uvDist);
        
        // Generate blood pattern
        float pattern = bloodPattern(vec2(u, v), seed);
        
        // Combine falloffs
        float decalAlpha = pattern * distFalloff * radialFalloff * alpha;
        
        // Realistic blood color - darker in thick areas, brighter at edges
        // Fresh blood is darker red, thinner areas are more translucent
        float thickness = pattern * 0.8 + 0.2;
        vec3 darkBlood = vec3(0.25, 0.01, 0.01);   // Deep dark red (thick pooled blood)
        vec3 brightBlood = vec3(0.6, 0.05, 0.03);  // Brighter red (thin/fresh)
        vec3 bloodColor = mix(brightBlood, darkBlood, thickness);
        
        // Add subtle wet specular highlight
        float specular = pow(max(0.0, 1.0 - uvDist * 2.5), 4.0) * 0.15 * pattern;
        
        // Accumulate (blend)
        totalBlood = mix(totalBlood, bloodColor + specular, decalAlpha);
        totalAlpha = max(totalAlpha, decalAlpha);
    }
    
    return vec4(totalBlood, totalAlpha);
}

// Shadow calculation with PCF
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir2) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 0.0;
    }
    
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir2)), 0.001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    vec4 albedo = uBaseColor;
    if (uDebugNoTexture == 0) {
        if (uHasTexture) {
            vec4 texColor = texture(uAlbedoMap, TexCoord);
            if (texColor.a < 0.1) discard;
            albedo *= texColor;
        }
    } else {
        albedo = uBaseColor;
    }
    
    vec3 emission = vec3(0.0);
    if (uHasEmissive) {
        vec4 emColor = texture(uEmissiveMap, TexCoord);
        emission = emColor.rgb;
        if (!uHasTexture && emColor.a < 0.1) discard;
    }

    if (uDebugShowNormals == 1) {
        vec3 normalColor = normalize(Normal) * 0.5 + 0.5;
        FragColor = vec4(normalColor, 1.0);
        vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
        vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
        Velocity = a - b;
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    
    float shadow = 0.0;
    if (uUseShadows != 0) {
        shadow = ShadowCalculation(vFragPosLightSpace, norm, lightDir);
    }
    
    vec3 ambient = 0.4 * albedo.rgb;
    vec3 diffuse = diff * albedo.rgb * (1.0 - shadow * 0.7);
    
    vec3 finalColor = ambient + diffuse + emission;
    
    // Apply blood decals - projected onto the model surface
    vec4 blood = calculateBloodDecals(WorldPos, norm);
    if (blood.a > 0.01) {
        // Blend blood on top of the albedo
        finalColor = mix(finalColor, blood.rgb, blood.a);
    }
    
    float finalAlpha = albedo.a * uAlphaMultiplier;
    if (finalAlpha < 0.01) discard;
    FragColor = vec4(finalColor, finalAlpha);
    
    vec2 a = (vCurrentClip.xy / vCurrentClip.w) * 0.5 + 0.5;
    vec2 b = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = a - b;
}
