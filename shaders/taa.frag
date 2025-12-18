#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform sampler2D historyFrame;
uniform sampler2D depthMap;
uniform sampler2D velocityMap;
uniform sampler2D historyDepthMap;

uniform mat4 invViewProj;
uniform mat4 prevViewProj;

// Camera movement for velocity-based rejection
uniform vec3 cameraDelta;
uniform float nearPlane;
uniform float farPlane;
uniform float gamma;

vec3 toLinear(vec3 c) {
    // currentFrame/historyFrame are already gamma-corrected by composite.frag
    // Convert back to linear so TAA blend/clamp doesn't over-blur.
    return pow(max(c, vec3(0.0)), vec3(gamma));
}

vec3 toGamma(vec3 c) {
    return pow(clamp(c, 0.0, 1.0), vec3(1.0 / max(gamma, 1e-3)));
}

vec3 rgbToYCoCg(vec3 c) {
    // Cheap, stable color space for clipping (reduces hue shifts vs RGB clamp)
    float Y  = 0.25 * c.r + 0.5 * c.g + 0.25 * c.b;
    float Co = 0.5  * c.r - 0.5 * c.b;
    float Cg = -0.25 * c.r + 0.5 * c.g - 0.25 * c.b;
    return vec3(Y, Co, Cg);
}

vec3 yCoCgToRgb(vec3 c) {
    float Y = c.x, Co = c.y, Cg = c.z;
    float r = Y + Co - Cg;
    float g = Y + Cg;
    float b = Y - Co - Cg;
    return vec3(r, g, b);
}

// Convert depth buffer value to linear depth
float linearizeDepth(float d) {
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - d * (farPlane - nearPlane));
}

void main() {
    // 1. Get current depth
    float depth = texture(depthMap, TexCoords).r;
    
    // Handle sky (depth very close to 1.0)
    // NOTE: too-low threshold will classify distant geometry as "sky" due to depth precision.
    if (depth >= 0.999999) {
        vec3 current = texture(currentFrame, TexCoords).rgb;
        // For sky, use simpler blending with less history weight
        vec2 prevUV = TexCoords; // Sky is at infinity, so no reprojection needed
        if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
            vec3 history = texture(historyFrame, prevUV).rgb;
            FragColor = vec4(mix(current, history, 0.5), 1.0); // Less history for sky
        } else {
            FragColor = vec4(current, 1.0);
        }
        return;
    }
    
    // 2. Get Velocity from buffer (Screen Space Velocity)
    vec2 velocity = texture(velocityMap, TexCoords).rg;
    vec2 prevUV = TexCoords - velocity;
    
    // Calculate motion vector for velocity weighting
    vec2 motionVector = velocity;
    float motionMagnitude = length(motionVector);
    
    // 4. Sample current
    vec3 currentG = texture(currentFrame, TexCoords).rgb;
    vec3 current = toLinear(currentG);
    
    // 5. Neighborhood stats in YCoCg (in linear space) for stable clipping
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    
    vec2 texSize = vec2(textureSize(currentFrame, 0));
    vec2 texelSize = 1.0 / texSize;
    
    // 3x3 neighborhood sampling
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec3 sG = texture(currentFrame, TexCoords + vec2(x, y) * texelSize).rgb;
            vec3 sL = toLinear(sG);
            vec3 sYC = rgbToYCoCg(sL);
            m1 += sYC;
            m2 += sYC * sYC;
        }
    }
    
    m1 /= 9.0;
    m2 /= 9.0;
    
    // Calculate standard deviation
    vec3 sigma = sqrt(max(m2 - m1 * m1, vec3(0.0)));
    
    // Build a clipping box around neighborhood mean/variance.
    // Tighten it during motion (prevents ghosting) and loosen it when static (reduces shimmer).
    float motionPx = length(motionVector * texSize); // resolution independent
    // Be less eager to drop history on tiny motion (reduces visible jitter).
    float motionT = smoothstep(1.0, 16.0, motionPx);
    // Edge detection based on luminance variance; slightly less sensitive to avoid killing history on fine detail
    float edgeT = smoothstep(0.03, 0.12, sigma.x); // sigma.x is Y variance in YCoCg
    
    float clipGamma = mix(1.50, 1.00, max(motionT, edgeT));
    vec3 minColor = m1 - clipGamma * sigma;
    vec3 maxColor = m1 + clipGamma * sigma;

    // Check if reprojected UV is valid
    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
        vec3 historyG = texture(historyFrame, prevUV).rgb;
        vec3 history = toLinear(historyG);
        
        // Depth rejection: sample previous depth and compare
        float historyDepth = texture(historyDepthMap, prevUV).r;
        bool historyIsSky = (historyDepth >= 0.999999);
        
        // Use depth-buffer-space comparison (more stable at far distances than linear depth)
        float depthDiff = abs(depth - historyDepth);
        float depthThreshold = max(0.00075, depth * 0.0015); // looser for far values near 1.0
        bool depthValid = (!historyIsSky) && (depthDiff < depthThreshold);
        
        // Clamp history to current neighborhood (variance clipping in YCoCg)
        vec3 histYC = rgbToYCoCg(history);
        histYC = clamp(histYC, minColor, maxColor);
        history = yCoCgToRgb(histYC);
        
        // Adaptive blend factor: less history on motion and on edges (reduces blur/ghosting)
        float baseBlend = 0.90; // slightly less history to reduce blur
        float minBlend = 0.15;  // respond faster when motion/edges are present
        float response = max(motionT, edgeT);
        float blend = mix(baseBlend, minBlend, response);
        
        // Reduce history weight if depth changed significantly (disocclusion)
        if (!depthValid) {
            blend = 0.0;
        }
        
        vec3 outLinear = mix(current, history, blend);

        // Mild, motion-aware sharpening to counter TAA blur (clamped to avoid halos)
        vec3 n0 = toLinear(texture(currentFrame, TexCoords + vec2( 1.0,  0.0) * texelSize).rgb);
        vec3 n1 = toLinear(texture(currentFrame, TexCoords + vec2(-1.0,  0.0) * texelSize).rgb);
        vec3 n2 = toLinear(texture(currentFrame, TexCoords + vec2( 0.0,  1.0) * texelSize).rgb);
        vec3 n3 = toLinear(texture(currentFrame, TexCoords + vec2( 0.0, -1.0) * texelSize).rgb);
        vec3 avgN = 0.25 * (n0 + n1 + n2 + n3);

        float sharpenAmt = 0.08 * (1.0 - response); // only when stable
        vec3 sharpened = outLinear + sharpenAmt * (outLinear - avgN);

        vec3 minN = min(min(n0, n1), min(n2, n3));
        vec3 maxN = max(max(n0, n1), max(n2, n3));
        outLinear = clamp(sharpened, minN, maxN);

        FragColor = vec4(toGamma(outLinear), 1.0);
    } else {
        // Outside screen, use current frame
        FragColor = vec4(currentG, 1.0);
    }
}
