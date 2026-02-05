#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Camera.h"
#include "Frustum.h"
#include "FrameBuffer.h"
#include "ShadowMap.h"
#include "PostProcess.h"
#include "VoxelRayTracer.h"
#include "../World/ChunkManager.h"
#include "../Mesh/Mesh.h"
#include "../World/Block.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

class Entity;
class ExplosionVolumeSystem;

class Renderer {
public:
    // Render debris particles as small textured cubes
    struct DebrisRenderData {
        glm::vec3 position;
        glm::quat rotation;
        float scale;
        BlockType blockType;
        float alpha;
        float waterFactor;
    };

    struct BloodDecalRenderData {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 size;
        float rotation;
        glm::vec3 color;
        float alpha;
        float seed;      // Random seed for pattern variation
        int pattern;     // 0=splatter, 1=drip, 2=pool, 3=spray
        bool attachedToBlock = false;  // For world-space clipping
        glm::ivec3 blockPos = glm::ivec3(0);  // Block position for clipping
    };
    
    // Model blood decals - projected directly onto model surfaces in the shader
    struct ModelBloodDecal {
        Entity* entity = nullptr;    // Which entity this decal belongs to
        int boneIndex = -1;          // Bone/joint index for animation tracking (-1 = entity local)
        glm::vec3 localPos;          // Position relative to bone (or entity if boneIndex=-1)
        glm::vec3 localNormal;       // Direction decal faces (in bone/entity local space)
        float radius = 0.15f;        // Radius of decal influence
        float seed = 0.0f;           // Random seed for pattern
        float alpha = 1.0f;          // Current alpha (for fading)
        float age = 0.0f;            // Current age
        float lifetime = 10.0f;      // How long until fade
    };
    
    Renderer();
    ~Renderer() = default;

    bool initialize(int windowWidth, int windowHeight);
    void render(ChunkManager& chunkManager, Camera& camera, const std::vector<Entity*>& entities, 
                int windowWidth, int windowHeight, const std::vector<DebrisRenderData>& debris = {},
                const std::vector<BloodDecalRenderData>& bloodDecals = {},
                ExplosionVolumeSystem* explosionVolumes = nullptr,
                const std::function<void(Shader&, const glm::vec3&, const glm::vec3&)>& extraModelPass = nullptr,
                const std::vector<ModelBloodDecal>& modelDecals = {});
    void onResize(int width, int height);
    
    void setLightDirection(const glm::vec3& direction) { lightDirection = direction; }
    glm::vec3 getLightDirection() const { return lightDirection; }

    void setSunHeight(float height) { sunHeight = height; }
    void setTimeOfDay(float time) { timeOfDay = time; }

    void setSkyColor(const glm::vec3& color) { skyColor = color; }
    
    // Set biome colors for vegetation tinting
    void setBiomeGrassColor(const glm::vec3& color) { biomeGrassColor = color; }
    void setBiomeFoliageColor(const glm::vec3& color) { biomeFoliageColor = color; }
    void setUseBiomeColors(bool use) { useBiomeColors = use; }
    void setFireLightPositions(const std::vector<glm::vec3>& positions) { fireLightPositions = positions; }
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
    void renderBlockBreakOverlay(const Camera& camera, const glm::ivec3& blockPos, float progress, int windowWidth, int windowHeight);
    
    void renderDebris(const Camera& camera, const std::vector<DebrisRenderData>& debris, int windowWidth, int windowHeight);
    void renderDebrisShadow(const std::vector<DebrisRenderData>& debris, const glm::mat4& lightSpaceMatrix, const glm::dvec3& renderOrigin);
    void renderBloodDecals(const std::vector<BloodDecalRenderData>& decals, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& renderOrigin);
    
    void setShowCrosshair(bool show) { showCrosshair = show; }

    // Access PostProcess for debug/metrics
    PostProcess* getPostProcess() { return postProcess.get(); }
    
    // Blit depth buffer from mainFBO to default framebuffer for post-render drawing with depth
    void blitDepthToScreen(int windowWidth, int windowHeight);

private:
    Shader blockShader;
    Shader waterShader;
    Shader crosshairShader;
    Shader sunShader;
    Shader shadowShader;
    Shader shadowModelShader; // Shadow shader for skinned models
    Shader starShader;
    Shader cloudShader;
    Shader modelShader; // New shader for entities
    Shader destroyOverlayShader; // Shader for block destruction overlay
    Shader debrisShader; // Shader for debris particles
    Shader bloodDecalShader; // Shader for blood decals
    Shader explosionVolumeShader; // Shader for volumetric explosions
    
    std::unique_ptr<Mesh> crosshairMesh;
    bool showCrosshair = true;
    std::unique_ptr<Mesh> sunMesh;
    std::unique_ptr<Mesh> starMesh;
    std::unique_ptr<Mesh> cloudMesh;
    std::unique_ptr<Mesh> destroyOverlayMesh; // Mesh for block destruction overlay
    GLuint destroyOverlayVAO = 0;
    GLuint destroyOverlayVBO = 0;
    GLuint destroyOverlayEBO = 0;
    int destroyOverlayIndexCount = 0;
    
    // Debris rendering
    GLuint debrisVAO = 0;
    GLuint debrisVBO = 0;
    GLuint debrisEBO = 0;
    int debrisIndexCount = 0;

    // Blood decal rendering
    GLuint bloodDecalVAO = 0;
    GLuint bloodDecalVBO = 0;
    GLuint bloodDecalEBO = 0;
    int bloodDecalIndexCount = 0;
    
    std::unique_ptr<Texture> blockAtlas;
    
    // Post Processing
    std::unique_ptr<FrameBuffer> mainFBO;
    std::unique_ptr<ShadowMap> shadowMap;
    std::unique_ptr<PostProcess> postProcess;
    std::unique_ptr<VoxelRayTracer> rayTracer;
    
    // Reflection copy texture for water SSR
    GLuint reflectionCopyTexture = 0;
    GLuint reflectionCopyDepth = 0;
    int reflectionCopyWidth = 0;
    int reflectionCopyHeight = 0;

    Frustum frustum;
    Frustum shadowFrustum;
    
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> chunkMeshes;
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> waterMeshes;
    
    glm::vec3 lightDirection = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 skyColor = glm::vec3(0.53f, 0.81f, 0.92f);
    float sunHeight = 1.0f;
    float timeOfDay = 0.0f;
    // bool showShadows = true; // Removed
    
    // Biome colors for vegetation tinting
    glm::vec3 biomeGrassColor = glm::vec3(0.5f, 0.85f, 0.4f);   // Default green
    glm::vec3 biomeFoliageColor = glm::vec3(0.45f, 0.75f, 0.35f); // Default foliage
    bool useBiomeColors = true;

    static constexpr int MAX_FIRE_LIGHTS = 16;
    std::vector<glm::vec3> fireLightPositions;
    
    // Camera-Relative Rendering for TAA stability
    glm::dvec3 renderOrigin = glm::dvec3(0.0); // Origin for rendering (rebased periodically)
    glm::dvec3 prevRenderOrigin = glm::dvec3(0.0); // Previous origin for velocity calculation
    glm::mat4 prevView = glm::mat4(1.0f);
    glm::mat4 prevProjection = glm::mat4(1.0f);
    bool isFirstFrame = true;
    
    static constexpr double ORIGIN_REBASE_THRESHOLD = 256.0; // Rebase when camera is this far from origin
    size_t lastChunkCount = 0; // Track chunk loading for TAA history invalidation

    // Track mesh upload frames so newly uploaded chunks are prioritized into shadow map
    int frameCounter = 0;
    std::unordered_map<ChunkPos, int> lastUploadedFrame; // frame index of last upload

    void setupOpenGL();
    bool loadShaders();
    void initCrosshair();
    void initSun();
    void renderSun(const Camera& camera, int windowWidth, int windowHeight);
    void initStars();
    void renderStars(const Camera& camera, int windowWidth, int windowHeight);
    void initClouds();
    void renderClouds(const Camera& camera, int windowWidth, int windowHeight, const glm::mat4& lightSpaceMatrix);
    void initDestroyOverlay();
    void initDebrisMesh();
    void initBloodDecalMesh();
};
