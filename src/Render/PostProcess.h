#pragma once

#include "Shader.h"
#include "FrameBuffer.h"
#include <memory>
#include <vector>
#include <random>

class PostProcess {
public:
    PostProcess(int width, int height);
    ~PostProcess();

    void resize(int width, int height);
    
    // Main render function
    void render(GLuint colorTexture, GLuint depthTexture, const glm::mat4& projection, const glm::mat4& view, const glm::vec3& cameraPos, const glm::vec3& lightDir, const glm::mat4& unjitteredProjection, float volumetricIntensity = 1.0f, const glm::vec3& lightColor = glm::vec3(1.0f, 0.9f, 0.7f));

    // TAA specific
    const glm::mat4& getJitterMatrix() const { return jitterMatrix; }
    void updateJitter(int w, int h);

private:
    int width, height;
    GLuint quadVAO, quadVBO;

    // Shaders
    Shader ssaoShader;
    Shader ssaoBlurShader;
    Shader volumetricShader;
    Shader compositeShader; // Final combine + Tone mapping + Gamma
    Shader taaShader;

    // Framebuffers
    std::unique_ptr<FrameBuffer> ssaoFBO;
    std::unique_ptr<FrameBuffer> ssaoBlurFBO;
    std::unique_ptr<FrameBuffer> volumetricFBO;
    std::unique_ptr<FrameBuffer> intermediateFBO;
    
    // TAA History
    std::unique_ptr<FrameBuffer> historyFBO[2]; // Ping-pong buffers
    int currentHistoryIndex = 0;
    glm::mat4 jitterMatrix;
    glm::mat4 prevViewProj;
    int frameCount = 0;

    // SSAO Kernel
    std::vector<glm::vec3> ssaoKernel;
    std::vector<glm::vec3> ssaoNoise;
    GLuint noiseTexture;

    void initQuad();
    void initShaders();
    void initSSAO();
    void generateSSAOKernel();
    void generateSSAONoise();
};
