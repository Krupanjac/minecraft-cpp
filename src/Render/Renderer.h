#pragma once

#include "Shader.h"
#include "Camera.h"
#include "Frustum.h"
#include "../World/ChunkManager.h"
#include "../Mesh/Mesh.h"
#include <memory>
#include <unordered_map>

class Renderer {
public:
    Renderer();
    ~Renderer() = default;

    bool initialize();
    void render(ChunkManager& chunkManager, Camera& camera, int windowWidth, int windowHeight);
    
    Shader& getBlockShader() { return blockShader; }
    
    // Add mesh for a chunk
    void uploadChunkMesh(const ChunkPos& pos, const std::vector<Vertex>& vertices, const std::vector<u32>& indices);

    void renderCrosshair(int windowWidth, int windowHeight);

private:
    Shader blockShader;
    Shader crosshairShader;
    std::unique_ptr<Mesh> crosshairMesh;
    Frustum frustum;
    
    std::unordered_map<ChunkPos, std::unique_ptr<Mesh>> chunkMeshes;
    
    void setupOpenGL();
    bool loadShaders();
    void initCrosshair();
};
