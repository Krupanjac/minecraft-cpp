#include "Renderer.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"
#include "../Core/Settings.h"
#include "../Entity/Entity.h"
#include <GLFW/glfw3.h>
#include <random>
#include <cmath>
#include <fstream> // For debug shadow dump

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
    initStars();
    initClouds();

    blockAtlas = std::make_unique<Texture>("assets/block_atlas.png");

    // Initialize Post Processing
    mainFBO = std::make_unique<FrameBuffer>(windowWidth, windowHeight);
    postProcess = std::make_unique<PostProcess>(windowWidth, windowHeight);
    
    // Initialize Shadow Map (High resolution for crisp shadows)
    shadowMap = std::make_unique<ShadowMap>();
    if (!shadowMap->init(4096, 4096)) {
        LOG_ERROR("Failed to initialize Shadow Map");
        return false;
    }

    LOG_INFO("Renderer initialized");
    return true;
}

void Renderer::onResize(int width, int height) {
    if (mainFBO) mainFBO->resize(width, height);
    if (postProcess) postProcess->resize(width, height);
}

void Renderer::render(ChunkManager& chunkManager, Camera& camera, const std::vector<Entity*>& entities, int windowWidth, int windowHeight) {
    // === CAMERA-RELATIVE RENDERING SETUP ===
    // This prevents floating-point precision issues when far from world origin
    
    glm::dvec3 cameraPos = glm::dvec3(camera.getPosition());
    
    // Check if we need to rebase the render origin
    glm::dvec3 offsetFromOrigin = cameraPos - renderOrigin;
    if (glm::length(offsetFromOrigin) > ORIGIN_REBASE_THRESHOLD) {
        // Snap origin to camera position (snapped to reduce jitter)
        renderOrigin = glm::dvec3(
            std::floor(cameraPos.x / 16.0) * 16.0,
            0.0, // Keep Y at 0 to avoid vertical rebasing issues
            std::floor(cameraPos.z / 16.0) * 16.0
        );
        // Origin changed — invalidate TAA history to avoid ghosting from large snaps
        if (postProcess) postProcess->invalidateTAAHistory();
    }
    
    // Camera position relative to render origin (for stable float precision)
    glm::vec3 cameraRelative = glm::vec3(cameraPos - renderOrigin);
    
    // Render origin offset for chunk position calculation
    glm::vec3 originOffset = glm::vec3(renderOrigin);
    
    // Calculate Origin Delta for Velocity Buffer
    glm::vec3 originDelta = glm::vec3(renderOrigin - prevRenderOrigin);
    
    // === TAA HISTORY INVALIDATION ON CHUNK LOAD ===
    const auto& chunks = chunkManager.getChunks();
    size_t currentChunkCount = chunks.size();
    // NOTE:
    // Invalidating TAA every time chunk count changes can effectively disable TAA while streaming,
    // making the ON/OFF toggle look identical. Depth rejection should handle most disocclusions.
    lastChunkCount = currentChunkCount;

    // Update frame counter for upload tracking
    frameCounter++;

    // Calculate Light Space Matrix
    // Center on player
    // We position the "sun" far away along the light direction
    // IMPORTANT:
    // All rendering (including shadows) is done in camera-relative coordinates (world - renderOrigin).
    // That means the shadow frustum must also be built in that SAME coordinate space.
    // Using world-space renderOrigin here will break shadows after teleports / origin rebases.
    float shadowRange = Settings::instance().shadowDistance; // Covers visible area
    glm::vec3 lightTarget = glm::vec3(0.0f); // renderOrigin in camera-relative space
    glm::vec3 lightPos = lightTarget + lightDirection * 1000.0f; // far away
    // Use an orthographic projection centered on the light target
    glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    
    float near_plane = 1.0f, far_plane = 2000.0f;
    glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, near_plane, far_plane);
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // 0. Shadow Pass
    if (Settings::instance().enableShadows) {
        shadowMap->bind();
        
        // Update shadow frustum for culling
        shadowFrustum.update(lightSpaceMatrix);

        // Fix shadow acne by rendering front faces and using polygon offset
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        shadowShader.use();
        shadowShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
        
        int shadowDrawCalls = 0;
        // Render chunks for shadow map (use camera-relative positions to match lightSpaceMatrix)
        for (const auto& [pos, chunk] : chunks) {
            bool shouldRender = (chunk->getState() == ChunkState::GPU_UPLOADED);
            if (!shouldRender) {
                auto it = chunkMeshes.find(pos);
                if (it != chunkMeshes.end() && it->second->isUploaded()) {
                    if (chunk->getState() == ChunkState::MESH_BUILD || chunk->getState() == ChunkState::READY) {
                        shouldRender = true;
                    }
                }
            }

            if (!shouldRender) continue;

            // Compute chunk position relative to render origin (same space as cameraRelative / lightSpaceMatrix)
            glm::vec3 chunkWorldPos = ChunkManager::chunkToWorld(pos);
            glm::vec3 chunkRelativePos = chunkWorldPos - glm::vec3(renderOrigin);
            glm::vec3 chunkMin = chunkRelativePos;
            glm::vec3 chunkMax = chunkRelativePos + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);

            // If this chunk was uploaded very recently, force it into shadow pass for a few frames to ensure it contributes
            auto itUploaded = lastUploadedFrame.find(pos);
            bool recentUpload = (itUploaded != lastUploadedFrame.end() && (frameCounter - itUploaded->second) <= 3);
            if (!recentUpload) {
                if (!shadowFrustum.isBoxVisible(chunkMin, chunkMax)) {
                    continue;
                }
            }

            auto it = chunkMeshes.find(pos);
            if (it != chunkMeshes.end() && it->second->isUploaded()) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), chunkRelativePos);
                shadowShader.setMat4("uModel", model);

                it->second->bind();
                it->second->draw();
                it->second->unbind();
                ++shadowDrawCalls;
            }
        }

        // Restore state
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        shadowMap->unbind();
    }
    
    glViewport(0, 0, windowWidth, windowHeight);

    // 1. Render Scene to FBO
    mainFBO->bind();
    
    // Clear Color Buffer (0) with Sky Color
    float sky[4] = {skyColor.r, skyColor.g, skyColor.b, 1.0f};
    glClearBufferfv(GL_COLOR, 0, sky);
    
    // Clear Velocity Buffer (1) with 0
    float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 1, zero);
    
    // Clear Depth Buffer
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Update frustum
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 unjitteredProjection = camera.getProjectionMatrix(aspect);
    glm::mat4 projection = unjitteredProjection;
    if (Settings::instance().enableTAA && postProcess) {
        // Subpixel jitter is required for TAA to actually reduce aliasing.
        // Apply jitter directly into the projection matrix so motion vectors include it.
        postProcess->updateJitter(windowWidth, windowHeight);
        glm::vec2 j = postProcess->getJitterOffset(); // UV units (pixel/res)
        projection[2][0] += j.x * 2.0f;
        projection[2][1] += j.y * 2.0f;
    }

    // Create view matrix. For third-person we compute a collision-safe eye position
    glm::mat4 view;
    if (camera.isThirdPerson()) {
        // Compute target (player eye position in world space)
        float bobY = 0.0f;
        if (!camera.getFlightMode()) {
            bobY = sin(camera.bobbingTimer) * 0.15f;
        }
        glm::vec3 targetWorld = camera.getPosition() + glm::vec3(0.0f, camera.defaultY + bobY, 0.0f);
        glm::vec3 forward = camera.getFront();
        glm::vec3 up = camera.getUp();

        // Desired camera world position behind the player
        glm::vec3 desiredEyeWorld = targetWorld - forward * camera.thirdPersonDistance + glm::vec3(0.0f, 0.2f, 0.0f);

        // Collision test: sample along the line from target to desired eye and clamp at first opaque block
        const int samples = 32;
        glm::vec3 lastSafe = desiredEyeWorld; // fallback
        bool foundCollision = false;
        for (int i = 0; i <= samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(samples);
            glm::vec3 samplePos = glm::mix(targetWorld, desiredEyeWorld, t);

            auto chunk = chunkManager.getChunkAt(samplePos);
            if (chunk) {
                glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(chunk->getPosition());
                int lx = static_cast<int>(floor(samplePos.x)) - static_cast<int>(chunkOrigin.x);
                int ly = static_cast<int>(floor(samplePos.y)) - static_cast<int>(chunkOrigin.y);
                int lz = static_cast<int>(floor(samplePos.z)) - static_cast<int>(chunkOrigin.z);
                if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_HEIGHT && lz >= 0 && lz < CHUNK_SIZE) {
                    if (chunk->getBlock(lx, ly, lz).isOpaque()) {
                        foundCollision = true;
                        if (i == 0) {
                            // Direct collision at target: push camera slightly forward instead
                            lastSafe = targetWorld - forward * 0.5f + glm::vec3(0.0f, 0.2f, 0.0f);
                        }
                        break;
                    }
                }
            }
            // No opaque block here; mark as safe
            lastSafe = samplePos;
        }

        glm::vec3 eyeWorld = lastSafe;
        glm::vec3 targetRel = glm::vec3(targetWorld) - glm::vec3(renderOrigin);
        glm::vec3 eyeRel = glm::vec3(eyeWorld) - glm::vec3(renderOrigin);
        view = glm::lookAt(eyeRel, targetRel, up);
    } else {
        view = glm::lookAt(
            cameraRelative,                           // Eye position (relative to render origin)
            cameraRelative + camera.getFront(),       // Look target
            camera.getUp()                            // Up vector
        );
    }
    
    if (isFirstFrame) {
        prevView = view;
        prevProjection = projection;
        prevRenderOrigin = renderOrigin;
        isFirstFrame = false;
    }

    glm::mat4 viewProj = projection * view;
    frustum.update(viewProj);
    
    // Enable depth testing with LEQUAL for better precision
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Render stars and sun/moon (behind everything)
    renderStars(camera, windowWidth, windowHeight);
    renderSun(camera, windowWidth, windowHeight);
    renderClouds(camera, windowWidth, windowHeight, lightSpaceMatrix);

    // Render chunks
    glActiveTexture(GL_TEXTURE0);
    blockAtlas->bind(0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap->getDepthMap());
    
    blockShader.use();
    blockShader.setInt("uTexture", 0);
    blockShader.setInt("uShadowMap", 1);
    blockShader.setInt("uUseShadows", Settings::instance().enableShadows ? 1 : 0);
    blockShader.setMat4("uProjection", projection);
    blockShader.setMat4("uView", view);
    blockShader.setMat4("uPrevView", prevView);
    blockShader.setMat4("uPrevProjection", prevProjection);
    blockShader.setVec3("uOriginDelta", originDelta);
    blockShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
    blockShader.setVec3("uCameraPos", cameraRelative); // Use camera-relative position
    blockShader.setVec3("uLightDir", lightDirection);
    blockShader.setFloat("uAOStrength", Settings::instance().aoStrength);
    blockShader.setFloat("uGamma", Settings::instance().gamma);

    // Debug uniforms
    blockShader.setInt("uDebugNoTexture", Settings::instance().debugNoTexture ? 1 : 0);
    blockShader.setInt("uDebugShowNormals", Settings::instance().debugShowNormals ? 1 : 0);

    // Wireframe toggle applied around draw loop to only affect chunk rendering
    if (Settings::instance().debugWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }    
    // Disable face culling to prevent missing faces due to LOD/winding issues
    glDisable(GL_CULL_FACE);
    
    // Fog settings
    float fogDist = static_cast<float>(Settings::instance().renderDistance * CHUNK_SIZE);
    blockShader.setFloat("uFogDist", fogDist);
    blockShader.setVec3("uSkyColor", skyColor);

    int chunksRendered = 0;
    // const auto& chunks = chunkManager.getChunks(); // Already got this above
    
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
        
        // Calculate chunk position relative to render origin
        glm::vec3 chunkWorldPos = ChunkManager::chunkToWorld(pos);
        glm::vec3 chunkRelativePos = chunkWorldPos - originOffset; // Camera-relative chunk position
        
        // Frustum culling (use relative positions for consistency with view matrix)
        glm::vec3 chunkMin = chunkRelativePos;
        glm::vec3 chunkMax = chunkRelativePos + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);
        
        if (!frustum.isBoxVisible(chunkMin, chunkMax)) {
            continue;
        }
        
        // Get or create mesh
        auto it = chunkMeshes.find(pos);
        if (it != chunkMeshes.end() && it->second->isUploaded()) {
            // Use camera-relative position for model matrix
            glm::mat4 model = glm::translate(glm::mat4(1.0f), chunkRelativePos);
            blockShader.setMat4("uModel", model);
            
            it->second->bind();
            it->second->draw();
            it->second->unbind();
            
            chunksRendered++;
        }
    }
    
    // Reset polygon mode back to fill if we switched to wireframe
    if (Settings::instance().debugWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    blockShader.unuse();

    // Render Entities
    if (!entities.empty()) {
        modelShader.use();
        modelShader.setMat4("uProjection", projection);
        modelShader.setMat4("uView", view);
        modelShader.setMat4("uPrevView", prevView);
        modelShader.setMat4("uPrevProjection", prevProjection);
        modelShader.setVec3("uOriginDelta", originDelta);
        modelShader.setVec3("uLightDir", lightDirection);
        modelShader.setVec3("uCameraPos", cameraRelative);
        modelShader.setVec4("uBaseColor", glm::vec4(1.0f)); // Default white
        // Debug flags
        modelShader.setInt("uDebugNoTexture", Settings::instance().debugNoTexture ? 1 : 0);
        modelShader.setInt("uDebugShowNormals", Settings::instance().debugShowNormals ? 1 : 0);
        for (auto* entity : entities) {
            if (!entity) continue;
            // Render entity using camera-relative coordinates so rebasing doesn't move the model unexpectedly
            glm::vec3 worldPos = entity->getPosition();
            glm::vec3 relPos = glm::vec3(glm::dvec3(worldPos) - renderOrigin);
            // Temporarily set position for rendering
            entity->setPosition(relPos);
            entity->render(modelShader);
            // Restore world position
            entity->setPosition(worldPos);
        }
        modelShader.unuse();
    }

    // Render water chunks
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE); // Allow seeing water surface from below
    glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent objects
    
    waterShader.use();
    waterShader.setInt("uTexture", 0);
    waterShader.setMat4("uProjection", projection);
    waterShader.setMat4("uView", view);
    waterShader.setMat4("uPrevView", prevView);
    waterShader.setMat4("uPrevProjection", prevProjection);
    waterShader.setVec3("uOriginDelta", glm::vec3(renderOrigin - prevRenderOrigin));
    waterShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
    waterShader.setVec3("uCameraPos", cameraRelative);
    waterShader.setVec3("uLightDir", lightDirection);
    waterShader.setFloat("uFogDist", fogDist);
    waterShader.setVec3("uSkyColor", skyColor);
    waterShader.setFloat("uAOStrength", Settings::instance().aoStrength);
    // Debug flags
    waterShader.setInt("uDebugNoTexture", Settings::instance().debugNoTexture ? 1 : 0);
    waterShader.setInt("uDebugShowNormals", Settings::instance().debugShowNormals ? 1 : 0);    
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
        
        // Frustum culling (use camera-relative positions like other geometry)
        glm::vec3 chunkWorldPos = ChunkManager::chunkToWorld(pos);
        glm::vec3 chunkRelativePos = chunkWorldPos - originOffset; // camera-relative
        glm::vec3 chunkMin = chunkRelativePos;
        glm::vec3 chunkMax = chunkRelativePos + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);
        
        if (!frustum.isBoxVisible(chunkMin, chunkMax)) {
            continue;
        }
        
        // Get or create mesh
        auto it = waterMeshes.find(pos);
        if (it != waterMeshes.end() && it->second->isUploaded()) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), chunkRelativePos);
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
    
    // Calculate volumetric settings based on sun height
    float volIntensity = 1.0f;
    glm::vec3 lightCol = glm::vec3(1.0f, 0.9f, 0.7f); // Sun color

    if (sunHeight < -0.1f) {
        // Night / Moon
        volIntensity = 0.05f; // Much lower intensity for moon
        lightCol = glm::vec3(0.6f, 0.7f, 1.0f); // Moon color
    } else if (sunHeight < 0.1f) {
        // Transition
        float t = (sunHeight + 0.1f) / 0.2f;
        volIntensity = glm::mix(0.05f, 1.0f, t);
        lightCol = glm::mix(glm::vec3(0.6f, 0.7f, 1.0f), glm::vec3(1.0f, 0.9f, 0.7f), t);
    }

    postProcess->render(mainFBO->getTexture(), mainFBO->getDepthTexture(), mainFBO->getVelocityTexture(), projection, view, cameraRelative, lightDirection, unjitteredProjection, volIntensity, lightCol);

    // Update History
    prevView = view;
    prevProjection = projection;
    prevRenderOrigin = renderOrigin;

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
    if (!shadowShader.loadFromFiles("shaders/shadow.vert", "shaders/shadow.frag")) {
        LOG_ERROR("Failed to load shadow shader");
        success = false;
    }
    if (!starShader.loadFromFiles("shaders/stars.vert", "shaders/stars.frag")) {
        LOG_ERROR("Failed to load star shader");
        success = false;
    }
    if (!cloudShader.loadFromFiles("shaders/clouds.vert", "shaders/clouds.frag")) {
        LOG_ERROR("Failed to load cloud shader");
        success = false;
    }
    if (!modelShader.loadFromFiles("shaders/model.vert", "shaders/model.frag")) {
        LOG_ERROR("Failed to load model shader");
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
    
    // Quad facing -Z with UVs
    // UVs are packed into u16: (u << 8) | v
    // We use 0 and 1 (mapped to 0 and 255 in shader logic usually, but here we want 0..1 range)
    // Actually block shader expects 0..255 for tiling.
    // But sun shader is custom. Let's pass 0 and 255 and handle in shader.
    
    u16 uv00 = 0;
    u16 uv10 = (255 << 8);
    u16 uv11 = (255 << 8) | 255;
    u16 uv01 = 255;

    vertices.emplace_back(static_cast<i16>(-1), static_cast<i16>(-1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), uv00);
    vertices.emplace_back(static_cast<i16>(1), static_cast<i16>(-1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), uv10);
    vertices.emplace_back(static_cast<i16>(1), static_cast<i16>(1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), uv11);
    vertices.emplace_back(static_cast<i16>(-1), static_cast<i16>(1), static_cast<i16>(0), static_cast<u8>(0), static_cast<u8>(0), uv01);
    
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
    
    glDisable(GL_DEPTH_TEST); // Sun/Moon is always behind everything
    glDisable(GL_CULL_FACE);  // Disable culling
    sunMesh->bind();

    // Determine Sun and Moon directions
    glm::vec3 sunDir = lightDirection;
    if (sunHeight < -0.1f) {
        sunDir = -lightDirection; // If night, lightDirection is Moon, so Sun is opposite
    }
    glm::vec3 moonDir = -sunDir;

    // Render Sun
    if (sunDir.y > -0.2f) {
        glm::vec3 sunPos = sunDir * 50.0f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), sunPos);
        model = glm::inverse(glm::lookAt(sunPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        float sunScale = std::max(0.1f, Settings::instance().sunSize);
        model = glm::scale(model, glm::vec3(sunScale)); // Scale sun
        
        sunShader.setMat4("uProjection", projection);
        sunShader.setMat4("uView", view);
        sunShader.setMat4("uModel", model);
        sunShader.setInt("uIsMoon", 0);
        sunMesh->draw();
    }

    // Render Moon
    if (moonDir.y > -0.2f) {
        glm::vec3 moonPos = moonDir * 50.0f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), moonPos);
        model = glm::inverse(glm::lookAt(moonPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        float moonScale = std::max(0.1f, Settings::instance().moonSize);
        model = glm::scale(model, glm::vec3(moonScale)); // Scale moon
        
        sunShader.setMat4("uProjection", projection);
        sunShader.setMat4("uView", view);
        sunShader.setMat4("uModel", model);
        sunShader.setInt("uIsMoon", 1);
        sunMesh->draw();
    }

    sunMesh->unbind();
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    
    sunShader.unuse();
}

void Renderer::initStars() {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (int i = 0; i < 1500; ++i) {
        float x = dist(rng);
        float y = dist(rng);
        float z = dist(rng);
        
        // Normalize to sphere
        float len = std::sqrt(x*x + y*y + z*z);
        if (len < 0.001f) continue;
        
        x /= len;
        y /= len;
        z /= len;
        
        // Scale up
        x *= 80.0f;
        y *= 80.0f;
        z *= 80.0f;
        
        vertices.emplace_back(static_cast<i16>(x), static_cast<i16>(y), static_cast<i16>(z), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
        indices.push_back(i);
    }
    
    starMesh = std::make_unique<Mesh>();
    starMesh->upload(vertices, indices);
}

void Renderer::renderStars(const Camera& camera, int windowWidth, int windowHeight) {
    starShader.use();
    
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    
    // Remove translation from view matrix
    glm::mat4 view = glm::mat4(glm::mat3(camera.getViewMatrix()));
    
    starShader.setMat4("uProjection", projection);
    starShader.setMat4("uView", view);
    starShader.setFloat("uTime", timeOfDay);
    starShader.setFloat("uSunHeight", sunHeight);
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    starMesh->bind();
    // Draw points manually since Mesh::draw uses GL_TRIANGLES
    glDrawElements(GL_POINTS, 1500, GL_UNSIGNED_INT, 0);
    starMesh->unbind();
    
    glDisable(GL_BLEND);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    
    starShader.unuse();
}

void Renderer::initClouds() {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    // Generate a large grid of clouds
    // 128x128 grid, each cell is 12 units wide
    int gridSize = 128;
    float scale = 12.0f;
    
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    // Multi-octave sine noise for soft variation; returns roughly [-1, 1]
    auto noiseVal = [&](int x, int z) {
        float nx = static_cast<float>(x) * 0.07f;
        float nz = static_cast<float>(z) * 0.07f;
        float n = 0.0f;
        n += std::sin(nx) * std::cos(nz);
        n += std::sin(nx * 1.9f + 0.7f) * std::cos(nz * 2.1f + 1.3f) * 0.6f;
        n += std::sin(nx * 3.7f + 2.0f) * std::cos(nz * 3.3f + 0.5f) * 0.3f;
        return n * 0.5f; // Keep magnitude modest
    };

    // Precompute occupancy and per-cell height so we can skip internal faces between connected cloud cells
    std::vector<std::vector<bool>> occ(gridSize, std::vector<bool>(gridSize));
    std::vector<std::vector<float>> heightMap(gridSize, std::vector<float>(gridSize, 0.0f));
    for (int ix = 0; ix < gridSize; ++ix) {
        for (int iz = 0; iz < gridSize; ++iz) {
            float n = noiseVal(ix, iz);
            bool hasCloud = n > 0.1f;
            occ[ix][iz] = hasCloud;
            if (hasCloud) {
                float jitter = dist(rng) * 0.5f;
                float thickness = 2.5f + std::max(0.0f, n) * 4.5f + jitter;
                heightMap[ix][iz] = thickness;
            }
        }
    }

    // Greedy meshing for top faces
    const float eps = 0.01f;
    std::vector<std::vector<bool>> topVisited(gridSize, std::vector<bool>(gridSize, false));
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            if (!occ[x][z] || topVisited[x][z]) continue;
            float h = heightMap[x][z];
            // find width
            int w = 1;
            while (x + w < gridSize && occ[x + w][z] && !topVisited[x + w][z] && std::abs(heightMap[x + w][z] - h) < eps) {
                ++w;
            }
            // find height in z
            int d = 1;
            bool extend = true;
            while (z + d < gridSize && extend) {
                for (int xx = x; xx < x + w; ++xx) {
                    if (!occ[xx][z + d] || topVisited[xx][z + d] || std::abs(heightMap[xx][z + d] - h) >= eps) {
                        extend = false;
                        break;
                    }
                }
                if (extend) ++d;
            }
            // mark visited
            for (int zz = z; zz < z + d; ++zz) {
                for (int xx = x; xx < x + w; ++xx) {
                    topVisited[xx][zz] = true;
                }
            }
            float x0 = (x - gridSize/2) * scale;
            float x1 = (x + w - gridSize/2) * scale;
            float z0 = (z - gridSize/2) * scale;
            float z1 = (z + d - gridSize/2) * scale;
            float y = h;
            // Top faces face +Y, CCW from above
            vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            u32 b = static_cast<u32>(vertices.size()) - 4;
            indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
            indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
        }
    }

    // Greedy meshing for bottom faces
    std::vector<std::vector<bool>> bottomVisited(gridSize, std::vector<bool>(gridSize, false));
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            if (!occ[x][z] || bottomVisited[x][z]) continue;
            float h = heightMap[x][z];
            int w = 1;
            while (x + w < gridSize && occ[x + w][z] && !bottomVisited[x + w][z] && std::abs(heightMap[x + w][z] - h) < eps) ++w;
            int d = 1; bool extend = true;
            while (z + d < gridSize && extend) {
                for (int xx = x; xx < x + w; ++xx) {
                    if (!occ[xx][z + d] || bottomVisited[xx][z + d] || std::abs(heightMap[xx][z + d] - h) >= eps) { extend = false; break; }
                }
                if (extend) ++d;
            }
            for (int zz = z; zz < z + d; ++zz) for (int xx = x; xx < x + w; ++xx) bottomVisited[xx][zz] = true;
            float x0 = (x - gridSize/2) * scale;
            float x1 = (x + w - gridSize/2) * scale;
            float z0 = (z - gridSize/2) * scale;
            float z1 = (z + d - gridSize/2) * scale;
            float y = 0.0f;
            // Bottom faces face -Y, so wind CW when looking from below
            vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
            u32 b = static_cast<u32>(vertices.size()) - 4;
            indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 1);
            indices.push_back(b + 0); indices.push_back(b + 3); indices.push_back(b + 2);
        }
    }

    auto mergeSide = [&](int dir) {
        // dir: 0 north(-z), 1 south(+z), 2 west(-x), 3 east(+x)
        if (dir == 0) {
            for (int z = 0; z < gridSize; ++z) {
                int zz = z;
                // mask across x
                int x = 0;
                while (x < gridSize) {
                    if (!occ[x][zz]) { ++x; continue; }
                    float h = heightMap[x][zz];
                    float neighborH = (zz - 1 >= 0 && occ[x][zz - 1]) ? heightMap[x][zz - 1] : 0.0f;
                    float exposed = h - neighborH;
                    if (exposed <= eps) { ++x; continue; }
                    // grow width with same exposed height
                    int w = 1;
                    while (x + w < gridSize && occ[x + w][zz]) {
                        float nh = heightMap[x + w][zz];
                        float nNeighbor = (zz - 1 >= 0 && occ[x + w][zz - 1]) ? heightMap[x + w][zz - 1] : 0.0f;
                        if (std::abs((nh - nNeighbor) - exposed) < eps && (nh - nNeighbor) > eps) {
                            ++w;
                        } else {
                            break;
                        }
                    }
                    float x0 = (x - gridSize/2) * scale;
                    float x1 = (x + w - gridSize/2) * scale;
                    float z0 = (zz - gridSize/2) * scale;
                    float y0 = neighborH;
                    float y1 = neighborH + exposed;
                    // north face outward -Z, CCW when looking at -Z
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y0), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y0), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y1), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y1), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    u32 b = static_cast<u32>(vertices.size()) - 4;
                    indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 1);
                    indices.push_back(b + 0); indices.push_back(b + 3); indices.push_back(b + 2);
                    x += w;
                }
            }
        } else if (dir == 1) {
            for (int z = 0; z < gridSize; ++z) {
                int zz = z;
                int x = 0;
                while (x < gridSize) {
                    if (!occ[x][zz]) { ++x; continue; }
                    float h = heightMap[x][zz];
                    float neighborH = (zz + 1 < gridSize && occ[x][zz + 1]) ? heightMap[x][zz + 1] : 0.0f;
                    float exposed = h - neighborH;
                    if (exposed <= eps) { ++x; continue; }
                    int w = 1;
                    while (x + w < gridSize && occ[x + w][zz]) {
                        float nh = heightMap[x + w][zz];
                        float nNeighbor = (zz + 1 < gridSize && occ[x + w][zz + 1]) ? heightMap[x + w][zz + 1] : 0.0f;
                        if (std::abs((nh - nNeighbor) - exposed) < eps && (nh - nNeighbor) > eps) {
                            ++w;
                        } else {
                            break;
                        }
                    }
                    float x0 = (x - gridSize/2) * scale;
                    float x1 = (x + w - gridSize/2) * scale;
                    float z1 = (zz + 1 - gridSize/2) * scale;
                    float y0 = neighborH;
                    float y1 = neighborH + exposed;
                    // south face outward +Z, CCW when looking at +Z
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y0), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y1), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y1), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y0), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    u32 b = static_cast<u32>(vertices.size()) - 4;
                    indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                    indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
                    x += w;
                }
            }
        } else if (dir == 2) {
            for (int x = 0; x < gridSize; ++x) {
                int xx = x;
                int z = 0;
                while (z < gridSize) {
                    if (!occ[xx][z]) { ++z; continue; }
                    float h = heightMap[xx][z];
                    float neighborH = (xx - 1 >= 0 && occ[xx - 1][z]) ? heightMap[xx - 1][z] : 0.0f;
                    float exposed = h - neighborH;
                    if (exposed <= eps) { ++z; continue; }
                    int d = 1;
                    while (z + d < gridSize && occ[xx][z + d]) {
                        float nh = heightMap[xx][z + d];
                        float nNeighbor = (xx - 1 >= 0 && occ[xx - 1][z + d]) ? heightMap[xx - 1][z + d] : 0.0f;
                        if (std::abs((nh - nNeighbor) - exposed) < eps && (nh - nNeighbor) > eps) {
                            ++d;
                        } else break;
                    }
                    float x0 = (xx - gridSize/2) * scale;
                    float z0 = (z - gridSize/2) * scale;
                    float z1 = (z + d - gridSize/2) * scale;
                    float y0 = neighborH;
                    float y1 = neighborH + exposed;
                    // west face outward -X, CCW when looking at -X
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y0), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y1), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y1), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x0), static_cast<i16>(y0), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    u32 b = static_cast<u32>(vertices.size()) - 4;
                    indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                    indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
                    z += d;
                }
            }
        } else if (dir == 3) {
            for (int x = 0; x < gridSize; ++x) {
                int xx = x;
                int z = 0;
                while (z < gridSize) {
                    if (!occ[xx][z]) { ++z; continue; }
                    float h = heightMap[xx][z];
                    float neighborH = (xx + 1 < gridSize && occ[xx + 1][z]) ? heightMap[xx + 1][z] : 0.0f;
                    float exposed = h - neighborH;
                    if (exposed <= eps) { ++z; continue; }
                    int d = 1;
                    while (z + d < gridSize && occ[xx][z + d]) {
                        float nh = heightMap[xx][z + d];
                        float nNeighbor = (xx + 1 < gridSize && occ[xx + 1][z + d]) ? heightMap[xx + 1][z + d] : 0.0f;
                        if (std::abs((nh - nNeighbor) - exposed) < eps && (nh - nNeighbor) > eps) {
                            ++d;
                        } else break;
                    }
                    float x1 = (xx + 1 - gridSize/2) * scale;
                    float z0 = (z - gridSize/2) * scale;
                    float z1 = (z + d - gridSize/2) * scale;
                    float y0 = neighborH;
                    float y1 = neighborH + exposed;
                    // east face outward +X, CCW when looking at +X
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y0), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y0), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y1), static_cast<i16>(z1), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    vertices.emplace_back(static_cast<i16>(x1), static_cast<i16>(y1), static_cast<i16>(z0), static_cast<u8>(0), static_cast<u8>(0), static_cast<u16>(0));
                    u32 b = static_cast<u32>(vertices.size()) - 4;
                    indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                    indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
                    z += d;
                }
            }
        }
    };

    mergeSide(0); // north
    mergeSide(1); // south
    mergeSide(2); // west
    mergeSide(3); // east
    
    cloudMesh = std::make_unique<Mesh>();
    cloudMesh->upload(vertices, indices);
}

void Renderer::renderClouds(const Camera& camera, int windowWidth, int windowHeight, const glm::mat4& lightSpaceMatrix) {
    cloudShader.use();
    
    float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    glm::mat4 projection = camera.getProjectionMatrix(aspect);
    glm::mat4 view = camera.getViewMatrix();
    
    // Position clouds
    // Move with time
    float speed = 2.0f;
    float offset = timeOfDay * speed;
    
    // Wrap position to keep clouds near camera
    // Grid size is 128 * 12 = 1536
    float worldSize = 128.0f * 12.0f;
    
    glm::vec3 camPos = camera.getPosition();
    float x = camPos.x - std::fmod(camPos.x, 12.0f); // Snap to grid
    
    // Add time offset
    x += offset;
    
    // Wrap
    // We want the mesh to be centered around the camera
    // But the mesh is static relative to its origin.
    // We translate the mesh to (camX, 128, camZ)
    // And offset texture? No, we built geometry.
    // We translate the mesh to (camX, 128, camZ) but we need to snap it so it doesn't jitter.
    // And we need to wrap the "noise" which we can't do easily with static mesh.
    // So we just move the mesh and let it go out of bounds? No.
    // We move the mesh by (offset % worldSize).
    // And we center it on the camera by adding (camPos - mod(camPos, worldSize)).
    
    float moveX = std::fmod(offset, worldSize);
    float baseX = camPos.x - std::fmod(camPos.x, worldSize);
    float baseZ = camPos.z - std::fmod(camPos.z, worldSize);
    
    // We might need to render 4 copies to cover the edges seamlessly if the mesh isn't huge enough
    // Or just make the mesh huge (done: 1536 units is > render distance 32*16=512)
    
    glm::vec3 pos(baseX - moveX, 128.0f, baseZ);
    
    // If the cloud layer moves too far, it wraps.
    // Since we generated a static mesh, we just translate it.
    // But we need it to cover the camera.
    // If camera moves to X=2000, baseX moves to 1536.
    // So the mesh jumps.
    // This works if the mesh is seamless.
    // Our noise function was NOT seamless.
    // Let's assume it's fine for now, or fix noise to be periodic?
    // Fixing noise is better.
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    
    cloudShader.setMat4("uProjection", projection);
    cloudShader.setMat4("uView", view);
    cloudShader.setMat4("uModel", model);
    cloudShader.setVec3("uCameraPos", camPos);
    cloudShader.setVec3("uSkyColor", skyColor);
    cloudShader.setFloat("uFogDist", static_cast<float>(Settings::instance().renderDistance * CHUNK_SIZE));

    // Bind shadow map and pass light matrix for cloud shadows
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap->getDepthMap());
    cloudShader.setInt("uShadowMap", 1);
    cloudShader.setInt("uUseShadows", Settings::instance().enableShadows ? 1 : 0);
    cloudShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
    cloudShader.setVec3("uLightDir", lightDirection);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Disable face culling so we see inside/outside
    glDisable(GL_CULL_FACE);
    
    cloudMesh->bind();
    cloudMesh->draw();
    cloudMesh->unbind();
    
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    
    cloudShader.unuse();
}

void Renderer::cleanUnusedMeshes(const ChunkManager& chunkManager) {
    const auto& chunks = chunkManager.getChunks();
    
    // Clean chunk meshes
    for (auto it = chunkMeshes.begin(); it != chunkMeshes.end(); ) {
        if (chunks.find(it->first) == chunks.end()) {
            it = chunkMeshes.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clean water meshes
    for (auto it = waterMeshes.begin(); it != waterMeshes.end(); ) {
        if (chunks.find(it->first) == chunks.end()) {
            it = waterMeshes.erase(it);
        } else {
            ++it;
        }
    }
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
        // Record upload frame so shadow pass can include this chunk immediately for a few frames
        lastUploadedFrame[pos] = frameCounter;
    } else {
        chunkMeshes.erase(pos);
        lastUploadedFrame.erase(pos);
    }
    
    // Water mesh
    if (!waterVertices.empty() && !waterIndices.empty()) {
        auto mesh = std::make_unique<Mesh>();
        mesh->upload(waterVertices, waterIndices);
        waterMeshes[pos] = std::move(mesh);
        lastUploadedFrame[pos] = frameCounter; // also mark
    } else {
        waterMeshes.erase(pos);
        lastUploadedFrame.erase(pos);
    }
}
