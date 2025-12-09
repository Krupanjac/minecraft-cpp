#include "Renderer.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"
#include "../Core/Settings.h"

Renderer::Renderer() {
}

bool Renderer::initialize() {
    setupOpenGL();
    
    if (!loadShaders()) {
        LOG_ERROR("Failed to load shaders");
        return false;
    }
    
    initCrosshair();
    initSun();

    LOG_INFO("Renderer initialized");
    return true;
}

void Renderer::render(ChunkManager& chunkManager, Camera& camera, int windowWidth, int windowHeight) {
    // Clear with sky color
    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Update frustum
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewProj = projection * view;
    frustum.update(viewProj);
    
    // Render sun first (behind everything)
    renderSun(camera, windowWidth, windowHeight);

    // Render chunks
    blockShader.use();
    blockShader.setMat4("uProjection", projection);
    blockShader.setMat4("uView", view);
    blockShader.setVec3("uCameraPos", camera.getPosition());
    blockShader.setVec3("uLightDir", lightDirection);
    blockShader.setFloat("uAOStrength", Settings::instance().aoStrength);
    blockShader.setFloat("uGamma", Settings::instance().gamma);
    
    // Fog settings
    float fogDist = static_cast<float>(Settings::instance().renderDistance * CHUNK_SIZE);
    blockShader.setFloat("uFogDist", fogDist);
    blockShader.setVec3("uSkyColor", skyColor);

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

    renderCrosshair(windowWidth, windowHeight);
}

void Renderer::renderCrosshair(int windowWidth, int windowHeight) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); // Invert colors

    crosshairShader.use();
    
    // Simple orthographic projection for 2D UI
    // Center is (0,0), range [-1, 1]
    // We want a fixed size crosshair regardless of aspect ratio
    float scaleX = 20.0f / windowWidth;  // 20 pixels wide
    float scaleY = 20.0f / windowHeight; // 20 pixels tall
    
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(scaleX, scaleY, 1.0f));
    crosshairShader.setMat4("uModel", model);
    
    crosshairMesh->bind();
    glDrawElements(GL_LINES, 4, GL_UNSIGNED_INT, 0);
    crosshairMesh->unbind();
    
    crosshairShader.unuse();
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
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
    
    if (!sunShader.loadFromFiles("shaders/sun.vert", "shaders/sun.frag")) {
        LOG_ERROR("Failed to load sun shader");
        success = false;
    }

    // Create simple shader for crosshair inline or load from file
    // For simplicity, we'll use a very basic shader source here
    const char* crosshairVert = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 uModel;
        void main() {
            gl_Position = uModel * vec4(aPos, 1.0);
        }
    )";
    
    const char* crosshairFrag = R"(
        #version 450 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";
    
    if (!crosshairShader.loadFromSource(crosshairVert, crosshairFrag)) {
        LOG_ERROR("Failed to load crosshair shader");
        return false;
    }
    
    return success;
}

void Renderer::initCrosshair() {
    // Simple crosshair geometry (lines)
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    // Horizontal line
    vertices.emplace_back(static_cast<i16>(-1), static_cast<i16>(0), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    vertices.emplace_back(static_cast<i16>(1), static_cast<i16>(0), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    
    // Vertical line
    vertices.emplace_back(static_cast<i16>(0), static_cast<i16>(-1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    vertices.emplace_back(static_cast<i16>(0), static_cast<i16>(1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    
    indices = {0, 1, 2, 3};
    
    crosshairMesh = std::make_unique<Mesh>();
    crosshairMesh->upload(vertices, indices);
}

void Renderer::initSun() {
    // Simple quad for sun
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    // Quad facing -Z
    vertices.emplace_back(static_cast<i16>(-1), static_cast<i16>(-1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    vertices.emplace_back(static_cast<i16>(1), static_cast<i16>(-1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    vertices.emplace_back(static_cast<i16>(1), static_cast<i16>(1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    vertices.emplace_back(static_cast<i16>(-1), static_cast<i16>(1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
    
    indices = {0, 1, 2, 2, 3, 0};
    
    sunMesh = std::make_unique<Mesh>();
    sunMesh->upload(vertices, indices);
}

void Renderer::renderSun(const Camera& camera, int windowWidth, int windowHeight) {
    sunShader.use();
    
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    
    // Remove translation from view matrix so sun stays at infinity (skybox style)
    glm::mat4 view = glm::mat4(glm::mat3(camera.getViewMatrix()));
    
    // Position sun based on light direction
    // Light direction is vector TO the sun
    glm::vec3 sunPos = lightDirection * 50.0f; // Distance 50 units away
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), sunPos);
    // Billboard the sun to face camera
    model = glm::lookAt(sunPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::inverse(model); // Invert lookAt to get model matrix
    model = glm::scale(model, glm::vec3(5.0f)); // Scale sun
    
    sunShader.setMat4("uProjection", projection);
    sunShader.setMat4("uView", view);
    sunShader.setMat4("uModel", model);
    
    glDisable(GL_DEPTH_TEST); // Sun is always behind everything
    glDisable(GL_CULL_FACE);  // Disable culling to ensure sun is visible from any angle
    sunMesh->bind();
    sunMesh->draw();
    sunMesh->unbind();
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    
    sunShader.unuse();
}

void Renderer::uploadChunkMesh(const ChunkPos& pos, const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    if (vertices.empty() || indices.empty()) {
        return;
    }
    
    auto mesh = std::make_unique<Mesh>();
    mesh->upload(vertices, indices);
    chunkMeshes[pos] = std::move(mesh);
}
