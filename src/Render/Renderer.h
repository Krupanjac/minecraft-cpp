#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Camera.h"
#include "Frustum.h"
#include "FrameBuffer.h"
#include "ShadowMap.h"
#include "PostProcess.h"
#include "../World/ChunkManager.h"
#include "../Mesh/Mesh.h"
#include <memory>
#include <unordered_map>
#include <vector>

class Entity;

class Renderer {
public:
    Renderer();
    ~Renderer() = default;

    bool initialize(int windowWidth, int windowHeight);
    void render(ChunkManager& chunkManager, Camera& camera, const std::vector<Entity*>& entities, int windowWidth, int windowHeight);
    void onResize(int width, int height);
    
    void setLightDirection(const glm::vec3& direction) { lightDirection = direction; }
    glm::vec3 getLightDirection() const { return lightDirection; }

    void setSunHeight(float height) { sunHeight = height; }
    void setTimeOfDay(float time) { timeOfDay = time; }

    void setSkyColor(const glm::vec3& color) { skyColor = color; }
    // void setShowShadows(bool show) { showShadows = show; } // Removed, uses Settings

    Shader& getBlockShader() { return blockShader; }
    Shader& getModelShader() { return modelShader; }
    
    // Add mesh for a chunk
    void uploadChunkMesh(const ChunkPos& pos, 
                        const std::vector<Vertex>& vertices, 
                        const std::vector<u32>& indices,
                        const std::vector<Vertex>& waterVertices,
                        const std::vector<u32>& waterIndices);

    // Clean up meshes for chunks that are no longer in the ChunkManager
    void cleanUnusedMeshes(const ChunkManager& chunkManager);

    void clear() {
        chunkMeshes.clear();
        waterMeshes.clear();
    }

    void renderCrosshair(int windowWidth, int windowHeight);
    void renderLoadingScreen(int windowWidth, int windowHeight, float progress);

private:
    Shader blockShader;
    Shader waterShader;
    Shader crosshairShader;
    Shader sunShader;
    Shader shadowShader;
    Shader starShader;
    Shader cloudShader;
    Shader modelShader; // New shader for entities
    
    std::unique_ptr<Mesh> crosshairMesh;
    std::unique_ptr<Mesh> sunMesh;
    std::unique_ptr<Mesh> starMesh;
    std::unique_ptr<Mesh> cloudMesh;
    std::unique_ptr<Texture> blockAtlas;
    
    // Post Processing
    std::unique_ptr<FrameBuffer> mainFBO;
    std::unique_ptr<ShadowMap> shadowMap;
    std::unique_ptr<PostProcess> postProcess;

    Frustum frustum;
    Frustum shadowFrustum;
    
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> chunkMeshes;
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> waterMeshes;
    
    glm::vec3 lightDirection = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 skyColor = glm::vec3(0.53f, 0.81f, 0.92f);
    float sunHeight = 1.0f;
    float timeOfDay = 0.0f;
    // bool showShadows = true; // Removed
    
    // Camera-Relative Rendering for TAA stability
    glm::dvec3 renderOrigin = glm::dvec3(0.0); // Origin for rendering (rebased periodically)
    glm::dvec3 prevRenderOrigin = glm::dvec3(0.0); // Previous origin for velocity calculation
    glm::mat4 prevView = glm::mat4(1.0f);
    glm::mat4 prevProjection = glm::mat4(1.0f);
    bool isFirstFrame = true;
    
    static constexpr double ORIGIN_REBASE_THRESHOLD = 256.0; // Rebase when camera is this far from origin
    size_t lastChunkCount = 0; // Track chunk loading for TAA history invalidation

    void setupOpenGL();
    bool loadShaders();
    void initCrosshair();
    void initSun();
    void renderSun(const Camera& camera, int windowWidth, int windowHeight);
    void initStars();
    void renderStars(const Camera& camera, int windowWidth, int windowHeight);
    void initClouds();
    void renderClouds(const Camera& camera, int windowWidth, int windowHeight, const glm::mat4& lightSpaceMatrix);
};
