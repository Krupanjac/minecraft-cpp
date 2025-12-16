#include "PostProcess.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include <random>
#include <glm/gtc/matrix_transform.hpp>

PostProcess::PostProcess(int width, int height) : width(width), height(height), prevViewProj(glm::mat4(1.0f)) {
    initQuad();
    initShaders();
    
    ssaoFBO = std::make_unique<FrameBuffer>(width, height); // Single channel ideally
    ssaoBlurFBO = std::make_unique<FrameBuffer>(width, height);
    volumetricFBO = std::make_unique<FrameBuffer>(width / 2, height / 2); // Half res for performance
    intermediateFBO = std::make_unique<FrameBuffer>(width, height);
    
    historyFBO[0] = std::make_unique<FrameBuffer>(width, height);
    historyFBO[1] = std::make_unique<FrameBuffer>(width, height);

    initSSAO();
}

PostProcess::~PostProcess() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &noiseTexture);
}

void PostProcess::resize(int w, int h) {
    width = w;
    height = h;
    ssaoFBO->resize(w, h);
    ssaoBlurFBO->resize(w, h);
    volumetricFBO->resize(w / 2, h / 2);
    intermediateFBO->resize(w, h);
    historyFBO[0]->resize(w, h);
    historyFBO[1]->resize(w, h);
}

void PostProcess::initQuad() {
    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

void PostProcess::initShaders() {
    // We will create these shader files next
    if (!ssaoShader.loadFromFiles("shaders/post_process.vert", "shaders/ssao.frag")) LOG_ERROR("Failed to load SSAO shader");
    if (!ssaoBlurShader.loadFromFiles("shaders/post_process.vert", "shaders/ssao_blur.frag")) LOG_ERROR("Failed to load SSAO Blur shader");
    if (!volumetricShader.loadFromFiles("shaders/post_process.vert", "shaders/volumetric.frag")) LOG_ERROR("Failed to load Volumetric shader");
    if (!compositeShader.loadFromFiles("shaders/post_process.vert", "shaders/composite.frag")) LOG_ERROR("Failed to load Composite shader");
    if (!taaShader.loadFromFiles("shaders/post_process.vert", "shaders/taa.frag")) LOG_ERROR("Failed to load TAA shader");
}

void PostProcess::initSSAO() {
    generateSSAOKernel();
    generateSSAONoise();
}

float PostProcess::getLastTaaMotionMag() const { return lastTaaMotionMag; }
float PostProcess::getLastTaaBlendEstimate() const { return lastTaaBlendEstimate; }

void PostProcess::generateSSAOKernel() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0f;
        
        // Scale samples so they're more aligned to center of kernel
        scale = 0.1f + scale * scale * (1.0f - 0.1f); // lerp
        sample *= scale;
        ssaoKernel.push_back(sample);
    }
}

void PostProcess::generateSSAONoise() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            0.0f); 
        ssaoNoise.push_back(noise);
    }
    
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void PostProcess::updateJitter(int w, int h) {
    // Halton sequence (2, 3)
    static const float halton23[8][2] = {
        {0.5f, 0.333333f}, {0.25f, 0.666667f}, {0.75f, 0.111111f}, {0.125f, 0.444444f},
        {0.625f, 0.777778f}, {0.375f, 0.222222f}, {0.875f, 0.555556f}, {0.0625f, 0.888889f}
    };
    
    frameCount++;
    int index = frameCount % 8;
    
    // Scale to [-0.5, 0.5] pixels (Standard TAA jitter range)
    // Previous [-1, 1] was too aggressive
    float jitterX = (halton23[index][0] - 0.5f) / (float)w;
    float jitterY = (halton23[index][1] - 0.5f) / (float)h;
    
    jitterMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(jitterX, jitterY, 0.0f));
}

void PostProcess::render(GLuint colorTexture, GLuint depthTexture, GLuint velocityTexture, const glm::mat4& projection, const glm::mat4& view, const glm::vec3& cameraPos, const glm::vec3& lightDir, const glm::mat4& unjitteredProjection, float volumetricIntensity, const glm::vec3& lightColor) {
    
    glDisable(GL_DEPTH_TEST); // Disable depth test for full screen quads
    auto& settings = Settings::instance();

    // 1. SSAO Pass
    ssaoFBO->bind();
    if (settings.enableSSAO) {
        glClear(GL_COLOR_BUFFER_BIT);
        ssaoShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);
        ssaoShader.setInt("gPositionDepth", 0); // We reconstruct position from depth
        ssaoShader.setInt("texNoise", 1);
        ssaoShader.setMat4("projection", projection);
        ssaoShader.setMat4("invProjection", glm::inverse(projection));
        
        // Noise tiling and SSAO params
        ssaoShader.setVec2("noiseScale", glm::vec2((float)width / 4.0f, (float)height / 4.0f));

        // Base radius scaled by AO strength so stronger AO increases radius visually
        float baseRadius = 0.5f * (1.0f + settings.aoStrength * 0.8f);
        ssaoShader.setFloat("radius", baseRadius);
        ssaoShader.setFloat("bias", 0.025f);
        ssaoShader.setFloat("radiusScaleFactor", glm::clamp(settings.aoStrength * 1.2f, 0.0f, 3.0f));
        
        for (unsigned int i = 0; i < 64; ++i)
            ssaoShader.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
            
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
        // Clear to white (no occlusion)
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    ssaoFBO->unbind();

    // 2. SSAO Blur
    ssaoBlurFBO->bind();
    if (settings.enableSSAO) {
        glClear(GL_COLOR_BUFFER_BIT);
        ssaoBlurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoFBO->getTexture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTexture); // Provide depth for bilateral blur
        ssaoBlurShader.setInt("ssaoInput", 0);
        ssaoBlurShader.setInt("gPositionDepth", 1);
        float blurFalloff = glm::clamp(30.0f + settings.aoStrength * 40.0f, 5.0f, 200.0f);
        ssaoBlurShader.setFloat("blurDepthFalloff", blurFalloff);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
        // Clear to white
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    ssaoBlurFBO->unbind();

    // 3. Volumetric Lighting
    volumetricFBO->bind();
    if (settings.enableVolumetrics) {
        glClear(GL_COLOR_BUFFER_BIT);
        volumetricShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        volumetricShader.setInt("depthMap", 0);
        volumetricShader.setMat4("invViewProj", glm::inverse(projection * view));
        volumetricShader.setVec3("lightDir", lightDir);
        volumetricShader.setVec3("cameraPos", cameraPos);
        volumetricShader.setFloat("uIntensity", volumetricIntensity);
        volumetricShader.setVec3("uLightColor", lightColor);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
        // Clear to black (no light)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    volumetricFBO->unbind();

    // 4. Composite (Tone Mapping + Gamma + Combine)
    intermediateFBO->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Reset clear color
    glClear(GL_COLOR_BUFFER_BIT); // Ensure we start clean
    
    compositeShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurFBO->getTexture());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, volumetricFBO->getTexture());
    
    compositeShader.setInt("scene", 0);
    compositeShader.setInt("ssao", 1);
    compositeShader.setInt("volumetric", 2);
    compositeShader.setFloat("exposure", settings.exposure);
    compositeShader.setFloat("gamma", settings.gamma);
    compositeShader.setFloat("uAOStrength", settings.aoStrength);
    
    glBindVertexArray(quadVAO); // Ensure VAO is bound
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    intermediateFBO->unbind();

    // 5. TAA Resolve
    if (settings.enableTAA) {
        int prevHistoryIndex = 1 - currentHistoryIndex;
        historyFBO[currentHistoryIndex]->bind();
        glClear(GL_COLOR_BUFFER_BIT);
        
        taaShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, intermediateFBO->getTexture());
        glActiveTexture(GL_TEXTURE1);
        
        // If history is invalidated, use current frame as "history" to prevent ghosting
        if (invalidateHistory) {
            glBindTexture(GL_TEXTURE_2D, intermediateFBO->getTexture()); // Use current as history
        } else {
            glBindTexture(GL_TEXTURE_2D, historyFBO[prevHistoryIndex]->getTexture());
        }
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, depthTexture);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, velocityTexture);

        // Bind previous-frame depth texture for proper depth rejection
        glActiveTexture(GL_TEXTURE4);
        if (invalidateHistory) {
            // Use current depth as history depth when history is invalidated
            glBindTexture(GL_TEXTURE_2D, depthTexture);
        } else {
            glBindTexture(GL_TEXTURE_2D, historyFBO[prevHistoryIndex]->getDepthTexture());
        }
        
        taaShader.setInt("currentFrame", 0);
        taaShader.setInt("historyFrame", 1);
        taaShader.setInt("depthMap", 2);
        taaShader.setInt("velocityMap", 3);
        taaShader.setInt("historyDepthMap", 4);
        
        taaShader.setMat4("invViewProj", glm::inverse(projection * view));
        taaShader.setMat4("prevViewProj", prevViewProj);
        
        // Camera delta for motion-based rejection
        glm::vec3 cameraDelta = cameraPos - prevCameraPos;
        taaShader.setVec3("cameraDelta", cameraDelta);
        
        // Record debug metrics (approximate motion magnitude and estimated history blend used)
        float motionMag = glm::length(cameraDelta);
        lastTaaMotionMag = motionMag;
        float baseBlend = 0.9f;
        float estBlend = glm::mix(baseBlend, 0.3f, glm::smoothstep(0.001f, 0.02f, motionMag));
        if (invalidateHistory) estBlend = 0.0f; // No history
        lastTaaBlendEstimate = estBlend;

        // Depth linearization parameters
        taaShader.setFloat("nearPlane", 0.1f);  // Should match your camera near plane
        taaShader.setFloat("farPlane", 1000.0f); // Should match your camera far plane
        
        glBindVertexArray(quadVAO); // Ensure VAO is bound
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        historyFBO[currentHistoryIndex]->unbind();
        
        // 6. Blit to Screen
        glBindFramebuffer(GL_READ_FRAMEBUFFER, historyFBO[currentHistoryIndex]->getID());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
        // Update history
        prevViewProj = unjitteredProjection * view;
        prevCameraPos = cameraPos;
        currentHistoryIndex = 1 - currentHistoryIndex;
        invalidateHistory = false; // Reset after this frame
    } else {
        // No TAA, just blit intermediate to screen
        glBindFramebuffer(GL_READ_FRAMEBUFFER, intermediateFBO->getID());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
        // Keep history updated just in case we toggle it back on (prevents jump)
        prevViewProj = unjitteredProjection * view;
        prevCameraPos = cameraPos;
    }
}
