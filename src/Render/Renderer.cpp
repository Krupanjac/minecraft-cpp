#include "Renderer.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"
#include "../Core/Settings.h"
#include <GLFW/glfw3.h>

Renderer::Renderer() {
}

bool Renderer::initialize(int windowWidth, int windowHeight) {
    setupOpenGL();
    
    if (!loadShaders()) {
        LOG_ERROR("Failed to load shaders");
        return false;
    }
    
    initCrosshair();
    initSun();

    blockAtlas = std::make_unique<Texture>("assets/block_atlas.png");

    // Initialize Post Processing
    mainFBO = std::make_unique<FrameBuffer>(windowWidth, windowHeight);
    postProcess = std::make_unique<PostProcess>(windowWidth, windowHeight);

    LOG_INFO("Renderer initialized");
    return true;
}

void Renderer::onResize(int width, int height) {
    if (mainFBO) mainFBO->resize(width, height);
    if (postProcess) postProcess->resize(width, height);
}

void Renderer::render(ChunkManager& chunkManager, Camera& camera, int windowWidth, int windowHeight) {
    // 1. Render Scene to FBO
    mainFBO->bind();
    
    // Clear with sky color
    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Update frustum
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    
    // Apply TAA Jitter
    postProcess->updateJitter(windowWidth, windowHeight);
    projection = postProcess->getJitterMatrix() * projection;

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewProj = projection * view;
    frustum.update(viewProj);
    
    // Enable depth testing with LEQUAL for better precision
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Render sun first (behind everything)
    renderSun(camera, windowWidth, windowHeight);

    // Render chunks
    blockAtlas->bind(0);
    blockShader.use();
    blockShader.setInt("uTexture", 0);
    blockShader.setMat4("uProjection", projection);
    blockShader.setMat4("uView", view);
    blockShader.setVec3("uCameraPos", camera.getPosition());
    blockShader.setVec3("uLightDir", lightDirection);
    blockShader.setFloat("uAOStrength", Settings::instance().aoStrength);
    blockShader.setFloat("uGamma", Settings::instance().gamma);
    
    // Disable face culling to prevent missing faces due to LOD/winding issues
    glDisable(GL_CULL_FACE);
    
    // Fog settings
    float fogDist = static_cast<float>(Settings::instance().renderDistance * CHUNK_SIZE);
    blockShader.setFloat("uFogDist", fogDist);
    blockShader.setVec3("uSkyColor", skyColor);

    int chunksRendered = 0;
    const auto& chunks = chunkManager.getChunks();
    
    for (const auto& [pos, chunk] : chunks) {
        // Render if uploaded, or if rebuilding (MESH_BUILD/READY) but we have a mesh
        // This prevents flickering when LOD changes
        bool shouldRender = (chunk->getState() == ChunkState::GPU_UPLOADED);
        
        if (!shouldRender) {
            // Check if we have a valid mesh from previous state
            auto it = chunkMeshes.find(pos);
            if (it != chunkMeshes.end() && it->second->isUploaded()) {
                // Allow rendering during rebuild
                if (chunk->getState() == ChunkState::MESH_BUILD || chunk->getState() == ChunkState::READY) {
                    shouldRender = true;
                }
            }
        }
        
        if (!shouldRender) continue;
        
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

    // Render water chunks
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE); // Allow seeing water surface from below
    glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent objects
    
    waterShader.use();
    waterShader.setInt("uTexture", 0);
    waterShader.setMat4("uProjection", projection);
    waterShader.setMat4("uView", view);
    waterShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
    waterShader.setVec3("uCameraPos", camera.getPosition());
    waterShader.setVec3("uLightDir", lightDirection);
    waterShader.setFloat("uFogDist", fogDist);
    waterShader.setVec3("uSkyColor", skyColor);
    
    for (const auto& [pos, chunk] : chunks) {
        // Render if uploaded, or if rebuilding (MESH_BUILD/READY) but we have a mesh
        bool shouldRender = (chunk->getState() == ChunkState::GPU_UPLOADED);
        
        if (!shouldRender) {
            // Check if we have a valid mesh from previous state
            auto it = waterMeshes.find(pos);
            if (it != waterMeshes.end() && it->second->isUploaded()) {
                // Allow rendering during rebuild
                if (chunk->getState() == ChunkState::MESH_BUILD || chunk->getState() == ChunkState::READY) {
                    shouldRender = true;
                }
            }
        }
        
        if (!shouldRender) continue;
        
        // Frustum culling
        glm::vec3 chunkWorldPos = ChunkManager::chunkToWorld(pos);
        glm::vec3 chunkMin = chunkWorldPos;
        glm::vec3 chunkMax = chunkWorldPos + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);
        
        if (!frustum.isBoxVisible(chunkMin, chunkMax)) {
            continue;
        }
        
        // Get or create mesh
        auto it = waterMeshes.find(pos);
        if (it != waterMeshes.end() && it->second->isUploaded()) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), chunkWorldPos);
            waterShader.setMat4("uModel", model);
            
            it->second->bind();
            it->second->draw();
            it->second->unbind();
        }
    }
    
    waterShader.unuse();
    glDepthMask(GL_TRUE); // Re-enable depth writing
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    mainFBO->unbind();

    // 2. Post Processing Pass
    // Clear default framebuffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    postProcess->render(mainFBO->getTexture(), mainFBO->getDepthTexture(), projection, view, camera.getPosition(), lightDirection);

    // 3. UI / Overlays (Rendered directly to screen)
    
    // Underwater overlay
    glm::vec3 camPos = camera.getPosition();
    // Simple check: get block at camera position
    // We need to access chunkManager here
    auto chunk = chunkManager.getChunkAt(camPos);
    if (chunk) {
        glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(chunk->getPosition());
        int lx = static_cast<int>(floor(camPos.x)) - static_cast<int>(chunkOrigin.x);
        int ly = static_cast<int>(floor(camPos.y)) - static_cast<int>(chunkOrigin.y);
        int lz = static_cast<int>(floor(camPos.z)) - static_cast<int>(chunkOrigin.z);
        
        if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_HEIGHT && lz >= 0 && lz < CHUNK_SIZE) {
            if (chunk->getBlock(lx, ly, lz).isWater()) {
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                crosshairShader.use();
                glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 1.0f)); // Full screen quad
                crosshairShader.setMat4("uModel", model);
                crosshairShader.setVec4("uColor", glm::vec4(0.0f, 0.2f, 0.8f, 0.4f)); // Blue tint
                
                // Use sun mesh (quad) for overlay
                sunMesh->bind();
                sunMesh->draw();
                sunMesh->unbind();
                
                crosshairShader.unuse();
                glEnable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
            }
        }
    }

    renderCrosshair(windowWidth, windowHeight);
}

void Renderer::renderCrosshair(int windowWidth, int windowHeight) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); // Invert colors

    crosshairShader.use();
    crosshairShader.setVec4("uColor", glm::vec4(1.0f)); // White crosshair
    
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

void Renderer::renderLoadingScreen(int /*windowWidth*/, int /*windowHeight*/, float progress) {
    // Clear screen with dirt background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    crosshairShader.use();
    
    // Draw background (dirt texture tiled would be better, but solid color for now)
    // We can use the sun mesh (quad) scaled to cover screen
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 1.0f));
    crosshairShader.setMat4("uModel", model);
    crosshairShader.setVec4("uColor", glm::vec4(0.15f, 0.1f, 0.1f, 1.0f)); // Dark brown
    
    sunMesh->bind();
    sunMesh->draw();
    
    // Draw progress bar background
    float barWidth = 0.6f;
    float barHeight = 0.05f;
    
    model = glm::scale(glm::mat4(1.0f), glm::vec3(barWidth, barHeight, 1.0f));
    crosshairShader.setMat4("uModel", model);
    crosshairShader.setVec4("uColor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black border
    sunMesh->draw();
    
    // Draw progress bar fill
    // Scale X by progress
    // Translate to keep left aligned? No, center aligned is fine for now, or we can offset.
    // Let's do center aligned growing from center for simplicity, or left aligned.
    // To left align: translate by -width/2 + (width*progress)/2
    
    float currentWidth = barWidth * progress;
    // model = glm::translate(glm::mat4(1.0f), glm::vec3(-barWidth/2.0f + currentWidth/2.0f, 0.0f, 0.0f));
    // model = glm::scale(model, glm::vec3(currentWidth, barHeight * 0.8f, 1.0f));
    
    // Simple centered green bar
    model = glm::scale(glm::mat4(1.0f), glm::vec3(currentWidth * 0.98f, barHeight * 0.8f, 1.0f));
    crosshairShader.setMat4("uModel", model);
    crosshairShader.setVec4("uColor", glm::vec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
    
    sunMesh->draw();
    
    sunMesh->unbind();
    crosshairShader.unuse();
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
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
    
    if (!waterShader.loadFromFiles("shaders/water.vert", "shaders/water.frag")) {
        LOG_ERROR("Failed to load water shader");
        success = false;
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
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
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

void Renderer::uploadChunkMesh(const ChunkPos& pos, 
                              const std::vector<Vertex>& vertices, 
                              const std::vector<u32>& indices,
                              const std::vector<Vertex>& waterVertices,
                              const std::vector<u32>& waterIndices) {
    // Opaque mesh
    if (!vertices.empty() && !indices.empty()) {
        auto mesh = std::make_unique<Mesh>();
        mesh->upload(vertices, indices);
        chunkMeshes[pos] = std::move(mesh);
    } else {
        chunkMeshes.erase(pos);
    }
    
    // Water mesh
    if (!waterVertices.empty() && !waterIndices.empty()) {
        auto mesh = std::make_unique<Mesh>();
        mesh->upload(waterVertices, waterIndices);
        waterMeshes[pos] = std::move(mesh);
    } else {
        waterMeshes.erase(pos);
    }
}
