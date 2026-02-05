#version 450 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec2 vUV;
in vec3 vWorldPos;

uniform vec3 uColor;
uniform float uAlpha;
uniform float uSeed;     // Unique per-decal seed for pattern variation
uniform int uPattern;    // 0=splatter, 1=drip, 2=pool, 3=spray
uniform int uAttachedToBlock;  // 1 if attached to block, 0 otherwise
uniform vec3 uBlockPos;        // Block position for clipping
uniform vec3 uRenderOrigin;    // Render origin for world pos calculation
uniform vec3 uDecalNormal;     // Normal of the decal face for axis-aware clipping

// Hash functions for procedural randomness
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

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
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

float fbm(vec2 p, int octaves) {
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int i = 0; i < octaves; ++i) {
        v += a * noise(p);
        p = rot * p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

// Voronoi for cell-like splatter
float voronoi(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float minDist = 1.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = hash22(i + neighbor);
            vec2 diff = neighbor + point - f;
            float dist = length(diff);
            minDist = min(minDist, dist);
        }
    }
    return minDist;
}

// Main splatter pattern - organic blood splat
float patternSplatter(vec2 uv, float seed) {
    vec2 c = uv - 0.5;
    float d = length(c);
    float angle = atan(c.y, c.x);
    
    // Irregular edge using multiple noise octaves
    float edgeNoise = fbm(vec2(angle * 3.0 + seed * 10.0, seed), 4);
    float radius = 0.35 + edgeNoise * 0.25 - hash11(seed) * 0.1;
    
    // Main blob
    float blob = smoothstep(radius + 0.08, radius - 0.15, d);
    
    // Add irregular protrusions (fingers of blood)
    float fingers = 0.0;
    for (int i = 0; i < 5; i++) {
        float fi = float(i);
        float fingerAngle = hash11(seed + fi) * 6.28318;
        float fingerLen = hash11(seed + fi + 0.5) * 0.3 + 0.15;
        float fingerWidth = hash11(seed + fi + 0.7) * 0.08 + 0.04;
        
        vec2 fingerDir = vec2(cos(fingerAngle), sin(fingerAngle));
        float alongFinger = dot(c, fingerDir);
        float perpFinger = length(c - fingerDir * alongFinger);
        
        float finger = smoothstep(fingerWidth, 0.0, perpFinger) * 
                       smoothstep(0.0, 0.1, alongFinger) * 
                       smoothstep(fingerLen + 0.1, fingerLen - 0.05, alongFinger);
        fingers = max(fingers, finger);
    }
    
    // Scattered droplets
    float droplets = 0.0;
    for (int i = 0; i < 8; i++) {
        float di = float(i);
        vec2 dropPos = vec2(hash11(seed + di * 2.0), hash11(seed + di * 2.0 + 1.0)) - 0.5;
        dropPos *= 0.9;
        float dropSize = hash11(seed + di * 3.0) * 0.06 + 0.02;
        float drop = smoothstep(dropSize, dropSize * 0.3, length(uv - 0.5 - dropPos));
        droplets = max(droplets, drop * 0.7);
    }
    
    return clamp(blob + fingers * 0.8 + droplets, 0.0, 1.0);
}

// Drip pattern - vertical streaks
float patternDrip(vec2 uv, float seed) {
    vec2 c = uv - 0.5;
    
    // Multiple drip streams
    float drips = 0.0;
    for (int i = 0; i < 4; i++) {
        float di = float(i);
        float xOffset = (hash11(seed + di) - 0.5) * 0.6;
        float width = hash11(seed + di + 0.3) * 0.08 + 0.03;
        float dripLen = hash11(seed + di + 0.6) * 0.4 + 0.3;
        
        float xDist = abs(c.x - xOffset);
        float yStart = 0.4 - hash11(seed + di + 0.9) * 0.2;
        
        // Drip shape - wider at top, narrow at bottom
        float taper = smoothstep(yStart, yStart - dripLen, c.y);
        float currentWidth = width * (1.0 + taper * 0.5);
        
        float drip = smoothstep(currentWidth, currentWidth * 0.3, xDist);
        drip *= smoothstep(yStart + 0.1, yStart - 0.05, c.y);
        drip *= smoothstep(yStart - dripLen - 0.1, yStart - dripLen + 0.05, c.y);
        
        // Waviness
        float wave = sin(c.y * 15.0 + seed * 10.0 + di) * 0.01;
        drip *= smoothstep(currentWidth + wave, currentWidth * 0.3 + wave, xDist);
        
        drips = max(drips, drip);
    }
    
    // Impact point at top
    float impact = smoothstep(0.2, 0.0, length(c - vec2(0.0, 0.35)));
    
    return clamp(drips + impact * 0.6, 0.0, 1.0);
}

// Pool pattern - settling blood pool
float patternPool(vec2 uv, float seed) {
    vec2 c = uv - 0.5;
    float d = length(c);
    float angle = atan(c.y, c.x);
    
    // Organic pool edge
    float edgeVar = fbm(vec2(angle * 2.0 + seed * 5.0, d * 3.0 + seed), 5) * 0.3;
    float radius = 0.4 + edgeVar;
    
    // Smooth pool with soft edge
    float pool = smoothstep(radius + 0.1, radius - 0.2, d);
    
    // Internal variation (thicker/thinner areas)
    float internal = fbm(uv * 4.0 + seed, 3) * 0.3 + 0.7;
    pool *= internal;
    
    // Small bubbles/variations
    float bubbles = voronoi(uv * 8.0 + seed);
    pool *= 0.8 + bubbles * 0.2;
    
    return pool;
}

// Spray pattern - fine mist of droplets
float patternSpray(vec2 uv, float seed) {
    float spray = 0.0;
    
    // Many small droplets
    for (int i = 0; i < 20; i++) {
        float di = float(i);
        vec2 dropPos = vec2(hash11(seed + di * 1.1), hash11(seed + di * 1.1 + 0.5));
        
        // Bias toward center
        dropPos = mix(vec2(0.5), dropPos, 0.8);
        
        float dropSize = hash11(seed + di * 2.2) * 0.04 + 0.01;
        float dist = length(uv - dropPos);
        
        float drop = smoothstep(dropSize, dropSize * 0.2, dist);
        spray = max(spray, drop * (0.5 + hash11(seed + di * 3.3) * 0.5));
    }
    
    // Central impact area
    vec2 c = uv - 0.5;
    float centralImpact = smoothstep(0.15, 0.0, length(c)) * 0.6;
    
    return clamp(spray + centralImpact, 0.0, 1.0);
}

void main() {
    vec2 uv = vUV;
    
    // Hard clip - discard any fragment outside [0,1] UV bounds
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        discard;
    }
    
    // World-space block clipping - clip only in axes perpendicular to the face normal
    if (uAttachedToBlock == 1) {
        vec3 actualWorldPos = vWorldPos + uRenderOrigin;
        vec3 blockMin = uBlockPos - vec3(0.01);  // Small expansion for surface tolerance
        vec3 blockMax = uBlockPos + vec3(1.01);
        
        // Get absolute normal to determine which axis NOT to clip
        vec3 absNormal = abs(uDecalNormal);
        
        // Only clip in axes where normal is small (perpendicular to face)
        bool clipX = absNormal.x < 0.5;
        bool clipY = absNormal.y < 0.5;
        bool clipZ = absNormal.z < 0.5;
        
        if (clipX && (actualWorldPos.x < blockMin.x || actualWorldPos.x > blockMax.x)) discard;
        if (clipY && (actualWorldPos.y < blockMin.y || actualWorldPos.y > blockMax.y)) discard;
        if (clipZ && (actualWorldPos.z < blockMin.z || actualWorldPos.z > blockMax.z)) discard;
    }
    
    // Clamp UV to valid range for pattern sampling
    vec2 clampedUV = clamp(uv, 0.001, 0.999);
    
    // Edge fade for smooth pattern edges (not for clipping - that's done above)
    float edgeMargin = 0.03;
    float edgeFadeX = smoothstep(0.0, edgeMargin, uv.x) * smoothstep(1.0, 1.0 - edgeMargin, uv.x);
    float edgeFadeY = smoothstep(0.0, edgeMargin, uv.y) * smoothstep(1.0, 1.0 - edgeMargin, uv.y);
    float edgeFade = edgeFadeX * edgeFadeY;
    
    // Select pattern based on uniform
    float mask = 0.0;
    int pattern = uPattern % 4;
    
    if (pattern == 0) {
        mask = patternSplatter(clampedUV, uSeed);
    } else if (pattern == 1) {
        mask = patternDrip(clampedUV, uSeed);
    } else if (pattern == 2) {
        mask = patternPool(clampedUV, uSeed);
    } else {
        mask = patternSpray(clampedUV, uSeed);
    }
    
    // Apply edge fade
    mask *= edgeFade;
    
    float alpha = uAlpha * mask;
    if (alpha < 0.01) discard;
    
    // Color variation - darker in thick areas, lighter at edges
    float thickness = mask;
    vec3 darkBlood = uColor * 0.4;
    vec3 lightBlood = uColor * 1.1;
    vec3 color = mix(lightBlood, darkBlood, thickness);
    
    // Subtle specular highlight for wet look
    float spec = pow(max(0.0, 1.0 - thickness), 3.0) * 0.15;
    color += vec3(spec);
    
    FragColor = vec4(color, alpha);
    Velocity = vec2(0.0);
}
