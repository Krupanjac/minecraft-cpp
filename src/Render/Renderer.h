#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Camera.h"
#include "Frustum.h"
#include "FrameBuffer.h"
#include "PostProcess.h"
#include "../World/ChunkManager.h"
#include "../Mesh/Mesh.h"
#include <memory>
#include <unordered_map>

class Renderer {
public:
    Renderer();
    ~Renderer() = default;

    bool initialize(int windowWidth, int windowHeight);
    void render(ChunkManager& chunkManager, Camera& camera, int windowWidth, int windowHeight);
    void onResize(int width, int height);
    
    void setLightDirection(const glm::vec3& direction) { lightDirection = direction; }
    glm::vec3 getLightDirection() const { return lightDirection; }

    void setSkyColor(const glm::vec3& color) { skyColor = color; }

    Shader& getBlockShader() { return blockShader; }
    
    // Add mesh for a chunk
    void uploadChunkMesh(const ChunkPos& pos, 
                        const std::vector<Vertex>& vertices, 
                        const std::vector<u32>& indices,
                        const std::vector<Vertex>& waterVertices,
                        const std::vector<u32>& waterIndices);

    void renderCrosshair(int windowWidth, int windowHeight);
    void renderLoadingScreen(int windowWidth, int windowHeight, float progress);

private:
    Shader blockShader;
    Shader waterShader;
    Shader crosshairShader;
    Shader sunShader;
    std::unique_ptr<Mesh> crosshairMesh;
    std::unique_ptr<Mesh> sunMesh;
    std::unique_ptr<Texture> blockAtlas;
    
    // Post Processing
    std::unique_ptr<FrameBuffer> mainFBO;
    std::unique_ptr<PostProcess> postProcess;

    Frustum frustum;
    
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> chunkMeshes;
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> waterMeshes;
    
    glm::vec3 lightDirection = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 skyColor = glm::vec3(0.53f, 0.81f, 0.92f);

    void setupOpenGL();
    bool loadShaders();
    void initCrosshair();
    void initSun();
    void renderSun(const Camera& camera, int windowWidth, int windowHeight);
};
