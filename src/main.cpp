#include "Core/Window.h"
#include "Core/Time.h"
#include "Core/Logger.h"
#include "Core/ThreadPool.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
#include "World/ChunkManager.h"
#include "World/WorldGenerator.h"
#include "Mesh/MeshBuilder.h"
#include "Util/Config.h"

#include <memory>
#include <iostream>
#include <mutex>
#include <vector>

class Application {
public:
    Application() 
        : camera(glm::vec3(0.0f, 80.0f, 0.0f)),
          threadPool(THREAD_POOL_SIZE),
          lastX(0.0), lastY(0.0), firstMouse(true),
          running(true) {
    }
    
    ~Application() = default;
    
    bool initialize() {
        LOG_INFO("Initializing Minecraft C++ Engine");
        
        try {
            window = std::make_unique<Window>(1280, 720, "Minecraft C++");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create window: " + std::string(e.what()));
            return false;
        }
        
        window->setCursorMode(GLFW_CURSOR_DISABLED);
        
        window->setMouseButtonCallback([this](int button, int action, int mods) {
            onMouseButton(button, action, mods);
        });

        if (!renderer.initialize()) {
            LOG_ERROR("Failed to initialize renderer");
            return false;
        }
        
        LOG_INFO("Application initialized successfully");
        return true;
    }
    
    void run() {
        LOG_INFO("Starting main loop");
        
        Time::instance().reset();
        
        while (!window->shouldClose() && running) {
            Time::instance().update();
            float deltaTime = Time::instance().getDeltaTime();
            
            processInput(deltaTime);
            update(deltaTime);
            render();
            
            window->pollEvents();
            window->swapBuffers();
            
            // Log FPS every second
            static float fpsTimer = 0.0f;
            fpsTimer += deltaTime;
            if (fpsTimer >= 1.0f) {
                LOG_INFO("FPS: " + std::to_string(Time::instance().getFPS()));
                fpsTimer = 0.0f;
            }
        }
        
        LOG_INFO("Application shutting down");
    }
    
private:
    std::unique_ptr<Window> window;
    Renderer renderer;
    Camera camera;
    ChunkManager chunkManager;
    WorldGenerator worldGenerator;
    MeshBuilder meshBuilder;
    ThreadPool threadPool;
    
    std::mutex meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>> pendingMeshes;

    double lastX, lastY;
    bool firstMouse;
    bool running;
    
    void onMouseButton(int button, int action, int mods) {
        if (action == GLFW_PRESS) {
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                // Break block
                auto result = chunkManager.rayCast(camera.getPosition(), camera.getFront(), 5.0f);
                if (result.hit) {
                    glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                    int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
                    int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
                    int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
                    chunkManager.setBlockAt(x, y, z, Block(BlockType::AIR));
                }
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                // Place block
                auto result = chunkManager.rayCast(camera.getPosition(), camera.getFront(), 5.0f);
                if (result.hit) {
                    glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                    int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x + result.normal.x;
                    int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y + result.normal.y;
                    int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z + result.normal.z;
                    
                    // Don't place block inside player
                    glm::vec3 playerPos = camera.getPosition();
                    glm::vec3 blockPos(x + 0.5f, y + 0.5f, z + 0.5f);
                    if (glm::distance(playerPos, blockPos) > 1.0f) { 
                        chunkManager.setBlockAt(x, y, z, Block(BlockType::STONE));
                    }
                }
            }
        }
    }

    void processInput(float deltaTime) {
        if (window->isKeyPressed(GLFW_KEY_ESCAPE)) {
            running = false;
        }
        
        bool forward = window->isKeyPressed(GLFW_KEY_W);
        bool backward = window->isKeyPressed(GLFW_KEY_S);
        bool left = window->isKeyPressed(GLFW_KEY_A);
        bool right = window->isKeyPressed(GLFW_KEY_D);
        bool up = window->isKeyPressed(GLFW_KEY_SPACE);
        bool down = window->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
        
        camera.processInput(forward, backward, left, right, up, down, deltaTime);
        
        // Mouse input
        double xpos, ypos;
        glfwGetCursorPos(window->getNative(), &xpos, &ypos);
        
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        
        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(lastY - ypos); // Reversed
        
        lastX = xpos;
        lastY = ypos;
        
        camera.processMouseMovement(xoffset, yoffset);
    }
    
    void update(float deltaTime) {
        camera.update(deltaTime);
        chunkManager.update(camera.getPosition());
        
        // Generate chunks
        auto chunksToGenerate = chunkManager.getChunksToGenerate(camera.getPosition(), MAX_CHUNKS_PER_FRAME);
        for (const auto& pos : chunksToGenerate) {
            chunkManager.requestChunkGeneration(pos);
            auto chunk = chunkManager.getChunk(pos);
            if (chunk && chunk->getState() == ChunkState::UNLOADED) {
                chunk->setState(ChunkState::GENERATING);
                
                // Generate in thread pool
                threadPool.enqueue([this, chunk]() {
                    worldGenerator.generate(chunk);
                });
            }
        }
        
        // Build meshes
        auto chunksToMesh = chunkManager.getChunksToMesh(MAX_MESHES_PER_FRAME);
        for (auto chunk : chunksToMesh) {
            chunk->setState(ChunkState::READY);
            
            // Get neighbors for greedy meshing
            const ChunkPos& pos = chunk->getPosition();
            auto chunkXPos = chunkManager.getChunk(pos + ChunkPos(1, 0, 0));
            auto chunkXNeg = chunkManager.getChunk(pos + ChunkPos(-1, 0, 0));
            auto chunkYPos = chunkManager.getChunk(pos + ChunkPos(0, 1, 0));
            auto chunkYNeg = chunkManager.getChunk(pos + ChunkPos(0, -1, 0));
            auto chunkZPos = chunkManager.getChunk(pos + ChunkPos(0, 0, 1));
            auto chunkZNeg = chunkManager.getChunk(pos + ChunkPos(0, 0, -1));
            
            threadPool.enqueue([this, chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg]() {
                auto meshData = meshBuilder.buildChunkMesh(chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg);
                
                std::lock_guard<std::mutex> lock(meshMutex);
                pendingMeshes.emplace_back(chunk->getPosition(), std::move(meshData));
            });
        }

        // Upload meshes
        {
            std::lock_guard<std::mutex> lock(meshMutex);
            for (auto& [pos, meshData] : pendingMeshes) {
                if (!meshData.isEmpty()) {
                    renderer.uploadChunkMesh(pos, meshData.vertices, meshData.indices);
                    auto chunk = chunkManager.getChunk(pos);
                    if (chunk) {
                        chunk->setState(ChunkState::GPU_UPLOADED);
                    }
                } else {
                    // Empty mesh (e.g. air chunk), but still mark as processed
                    auto chunk = chunkManager.getChunk(pos);
                    if (chunk) {
                        chunk->setState(ChunkState::GPU_UPLOADED);
                    }
                }
            }
            pendingMeshes.clear();
        }
    }
    
    void render() {
        renderer.render(chunkManager, camera, window->getWidth(), window->getHeight());
    }
};

int main() {
    try {
        Application app;
        
        if (!app.initialize()) {
            LOG_ERROR("Failed to initialize application");
            return 1;
        }
        
        app.run();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Unhandled exception: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
