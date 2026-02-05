#pragma once

#include "Shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>
#include <string>

// Animated sprite particle system for blood splatters
class BloodSplatterSystem {
public:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec2 size;
        glm::vec4 color;
        float rotation;
        float rotationSpeed;
        float frame;
        float frameRate;
        float lifetime;
        float age;
        bool active;
    };

    BloodSplatterSystem();
    ~BloodSplatterSystem();

    bool initialize();
    void update(float deltaTime);
    void render(const glm::mat4& view, const glm::mat4& projection, 
                const glm::vec3& cameraRight, const glm::vec3& cameraUp);

    // Spawn blood splatter at position with direction and backsplash
    void spawnSplatter(const glm::vec3& position, const glm::vec3& direction, 
                       const glm::vec3& backSplashDir, float intensity = 1.0f);
    
    // Clear all particles
    void clear();
    
    // Get active particle count
    size_t getActiveCount() const;

private:
    void initQuadMesh();
    bool loadAtlas();
    
    // Particle pool
    std::vector<Particle> m_particles;
    size_t m_maxParticles = 500;
    
    // OpenGL resources
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_instanceVBO = 0;
    GLuint m_atlasTexture = 0;
    Shader m_shader;
    
    // Atlas info (6 columns x 5 rows = 30 frames)
    int m_atlasColumns = 6;
    int m_atlasRows = 5;
    int m_totalFrames = 30;
    
    // Random number generator
    std::mt19937 m_rng;
    
    // Instance data for GPU upload
    struct InstanceData {
        glm::vec3 worldPos;
        glm::vec2 size;
        glm::vec4 atlasInfo;  // frame, totalFrames, columns, rows
        glm::vec4 color;
        float rotation;
        float padding[3];  // Align to 16 bytes
    };
    std::vector<InstanceData> m_instanceData;
    
    bool m_initialized = false;
};
