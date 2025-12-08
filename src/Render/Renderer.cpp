#include "Renderer.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"

Renderer::Renderer() {
}

bool Renderer::initialize() {
    setupOpenGL();
    
    if (!loadShaders()) {
        LOG_ERROR("Failed to load shaders");
        return false;
    }
    
    LOG_INFO("Renderer initialized");
    return true;
}

void Renderer::render(ChunkManager& chunkManager, Camera& camera, int windowWidth, int windowHeight) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Update frustum
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewProj = projection * view;
    frustum.update(viewProj);
    
    // Render chunks
    blockShader.use();
    blockShader.setMat4("uProjection", projection);
    blockShader.setMat4("uView", view);
    blockShader.setVec3("uCameraPos", camera.getPosition());
    
    int chunksRendered = 0;
    const auto& chunks = chunkManager.getChunks();
    
    for (const auto& [pos, chunk] : chunks) {
        if (chunk->getState() != ChunkState::GPU_UPLOADED && 
            chunk->getState() != ChunkState::READY) {
            continue;
        }
        
        // Frustum culling
        glm::vec3 chunkWorldPos = ChunkManager::chunkToWorld(pos);
        glm::vec3 chunkMin = chunkWorldPos;
        glm::vec3 chunkMax = chunkWorldPos + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);
        
        if (!frustum.isBoxVisible(chunkMin, chunkMax)) {
            continue;
        }
        
        // Get or create mesh
        auto it = chunkMeshes.find(pos);
        if (it != chunkMeshes.end() && it->second->isUploaded()) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), chunkWorldPos);
            blockShader.setMat4("uModel", model);
            
            it->second->bind();
            it->second->draw();
            it->second->unbind();
            
            chunksRendered++;
        }
    }
    
    blockShader.unuse();
}

void Renderer::setupOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // Sky blue
    
    LOG_INFO("OpenGL setup complete");
}

bool Renderer::loadShaders() {
    bool success = blockShader.loadFromFiles("shaders/block.vert", "shaders/block.frag");
    if (success) {
        LOG_INFO("Block shader loaded successfully");
    }
    return success;
}

void Renderer::uploadChunkMesh(const ChunkPos& pos, const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    if (vertices.empty() || indices.empty()) {
        return;
    }
    
    auto mesh = std::make_unique<Mesh>();
    mesh->upload(vertices, indices);
    chunkMeshes[pos] = std::move(mesh);
}
