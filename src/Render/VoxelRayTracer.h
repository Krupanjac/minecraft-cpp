#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include "Shader.h"

class ChunkManager;

// Voxel Ray Tracing system using OpenGL 4.5 compute shaders
// Provides accurate lighting through ray tracing in the voxel world
class VoxelRayTracer {
public:
    VoxelRayTracer();
    ~VoxelRayTracer();
    
    // Initialize the ray tracer
    bool initialize(int screenWidth, int screenHeight);
    
    // Resize output textures
    void resize(int width, int height);
    
    // Update the 3D voxel texture from chunk data
    // Call this when chunks change near the player
    void updateVoxelGrid(ChunkManager& chunkManager, const glm::vec3& playerPos);
    
    // Trace rays and compute lighting
    // Returns the lighting texture ID
    // renderOrigin is the camera-relative rendering offset (needed to convert back to world space)
    GLuint trace(GLuint depthTexture, const glm::mat4& invViewProj, 
                 const glm::vec3& cameraPos, const glm::vec3& lightDir,
                 const glm::vec3& lightColor, const glm::vec3& renderOrigin);
    
    // Get the output lighting texture
    GLuint getLightingTexture() const { return lightingTexture; }
    
    // Settings
    void setMaxRaySteps(int steps) { maxRaySteps = steps; }
    void setRayMaxDistance(float dist) { rayMaxDistance = dist; }
    void setVoxelGridRadius(int radius) { 
        if (radius != voxelGridRadius) {
            voxelGridRadius = radius; 
            voxelGridSize = voxelGridRadius * 2;
            if (initialized) {
                createVoxelTexture(); // Recreate texture with new size
                lastUpdateChunk = glm::ivec3(INT_MAX); // Force voxel grid update
            }
        }
    }
    
    // Check if ray tracing is available (compute shader support)
    static bool isSupported();

private:
    // Compute shader
    Shader computeShader;
    
    // Output lighting texture
    GLuint lightingTexture = 0;
    int width = 0;
    int height = 0;
    
    // 3D voxel texture
    GLuint voxelTexture = 0;
    int voxelGridSize = 0;     // Size of the 3D grid (cubic)
    int voxelGridRadius = 48;  // Radius in blocks from player (smaller = faster)
    glm::vec3 voxelGridOrigin; // World position of grid corner
    
    // Settings
    int maxRaySteps = 128;      // Default for medium quality
    float rayMaxDistance = 64.0f; // Default for medium quality
    
    // Last update position (to avoid unnecessary updates)
    glm::ivec3 lastUpdateChunk = glm::ivec3(INT_MAX);
    
    // Create/recreate the voxel 3D texture
    void createVoxelTexture();
    
    // Create the output lighting texture
    void createLightingTexture();
    
    bool initialized = false;
};
