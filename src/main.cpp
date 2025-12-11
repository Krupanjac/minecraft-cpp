#include "Core/Window.h"
#include "Core/Time.h"
#include "Core/Logger.h"
#include "Core/ThreadPool.h"
#include "Core/Settings.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
#include "World/ChunkManager.h"
#include "World/WorldGenerator.h"
#include "Mesh/MeshBuilder.h"
#include "Util/Config.h"
#include "UI/UIManager.h"
#include "World/WorldSerializer.h"

#include <memory>
#include <iostream>
#include <mutex>
#include <vector>
#include <ctime>
#include <cstdlib>

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
        
        // Start with cursor visible for menu
        window->setCursorMode(GLFW_CURSOR_NORMAL);
        
        window->setMouseButtonCallback([this](int button, int action, int mods) {
            onMouseButton(button, action, mods);
        });
        
        window->setKeyCallback([this](int key, int /*scancode*/, int action, int /*mods*/) {
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                uiManager.handleKeyInput(key);
            }

            if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
                double currentTime = glfwGetTime();
                if (currentTime - lastSpaceTime < 0.3) {
                    camera.toggleFlightMode();
                }
                lastSpaceTime = currentTime;
            }
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                bool isMenuOpen = uiManager.isMenuOpen();
                uiManager.setMenuState(isMenuOpen ? MenuState::NONE : MenuState::IN_GAME_MENU);
                window->setCursorMode(isMenuOpen ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            }
        });
        
        window->setCharCallback([this](unsigned int codepoint) {
            uiManager.handleCharInput(codepoint);
        });
        
        window->setFramebufferSizeCallback([this](int width, int height) {
            glViewport(0, 0, width, height);
            uiManager.handleResize(width, height);
            renderer.onResize(width, height);
        });

        if (!renderer.initialize(window->getWidth(), window->getHeight())) {
            LOG_ERROR("Failed to initialize renderer");
            return false;
        }
        
        // Apply initial settings
        window->setVSync(Settings::instance().vsync);
        window->setFullscreen(Settings::instance().fullscreen);
        
        // Get actual framebuffer size for initialization
        int fbW, fbH;
        glfwGetFramebufferSize(window->getNative(), &fbW, &fbH);
        uiManager.initialize(fbW, fbH);
        
        // Setup UI Callbacks
        uiManager.setOnNewGame([this](std::string name, long seed) {
            createWorld(name, seed);
            uiManager.setMenuState(MenuState::NONE);
            window->setCursorMode(GLFW_CURSOR_DISABLED);
        });
        
        uiManager.setOnLoadGame([this](std::string name) {
            if (loadWorld(name)) {
                uiManager.setMenuState(MenuState::NONE);
                window->setCursorMode(GLFW_CURSOR_DISABLED);
            }
        });
        
        uiManager.setOnSave([this]() {
            WorldSerializer::saveWorld(currentWorldName, chunkManager, camera.getPosition(), currentSeed);
            LOG_INFO("Game Saved");
        });
        
        uiManager.setOnExit([this]() {
            window->close();
        });
        
        uiManager.setOnSettingsChanged([this]() {
            applySettings();
        });
        
        // Apply initial settings
        applySettings();
        
        // Start in Main Menu
        uiManager.setMenuState(MenuState::MAIN_MENU);
        
        LOG_INFO("Application initialized successfully");
        return true;
    }

    void createWorld(const std::string& name, long seed = 12345) {
        LOG_INFO("Creating new world: " + name + " with seed: " + std::to_string(seed));
        
        currentSeed = seed;
        currentWorldName = name.empty() ? "World_" + std::to_string(seed) : name;
        
        // Set seed
        worldGenerator.setSeed(static_cast<unsigned int>(seed));
        
        // Clear existing world
        chunkManager.unloadAll();
        chunkManager.clear(); // Clear preloaded data too
        renderer.clear(); // Clear GPU buffers from previous world
        
        // Find a safe spawn location (Land)
        // If (0,0) is ocean, search outwards until we find land.
        int spawnX = 0;
        int spawnZ = 0;
        int searchRadius = 0;
        bool foundLand = false;
        
        // Check origin first
        if (worldGenerator.getSurfaceHeight(0, 0) >= SEA_LEVEL) {
            foundLand = true;
        }
        
        while (!foundLand && searchRadius < 10000) {
            searchRadius += 64; // Step by 4 chunks
            
            // Check 4 cardinal directions
            if (worldGenerator.getSurfaceHeight(searchRadius, 0) >= SEA_LEVEL) { spawnX = searchRadius; spawnZ = 0; foundLand = true; break; }
            if (worldGenerator.getSurfaceHeight(-searchRadius, 0) >= SEA_LEVEL) { spawnX = -searchRadius; spawnZ = 0; foundLand = true; break; }
            if (worldGenerator.getSurfaceHeight(0, searchRadius) >= SEA_LEVEL) { spawnX = 0; spawnZ = searchRadius; foundLand = true; break; }
            if (worldGenerator.getSurfaceHeight(0, -searchRadius) >= SEA_LEVEL) { spawnX = 0; spawnZ = -searchRadius; foundLand = true; break; }
        }
        
        if (foundLand && (spawnX != 0 || spawnZ != 0)) {
            LOG_INFO("Spawn moved to (" + std::to_string(spawnX) + ", " + std::to_string(spawnZ) + ") to avoid ocean.");
        }

        // Determine safe spawn height at the new coordinates
        int terrainHeight = worldGenerator.getSurfaceHeight(spawnX, spawnZ);
        // Start the camera high above the terrain to ensure we load the chunks *above* the ground
        // and to avoid being inside a mountain before chunks load.
        float initialSpawnY = static_cast<float>(terrainHeight) + 30.0f;

        // Reset camera
        camera.setPosition(glm::vec3(static_cast<float>(spawnX), initialSpawnY, static_cast<float>(spawnZ)));
        camera.setYaw(-90.0f);
        camera.setPitch(0.0f);
        
        // Initial world generation loading screen
        LOG_INFO("Generating initial world...");
        
        // Load a small radius around player first (e.g. 4 chunks)
        int initialRadius = 4;
        
        // Wait for generation of initial chunks
        bool initialGenDone = false;
        while (!initialGenDone && !window->shouldClose()) {
            auto chunksToGen = chunkManager.getChunksToGenerate(camera.getPosition(), initialRadius, 10000);
            
            if (chunksToGen.empty()) {
                initialGenDone = true;
            } else {
                int generated = 0;
                int totalChunks = static_cast<int>(chunksToGen.size());
                
                for (const auto& pos : chunksToGen) {
                    chunkManager.requestChunkGeneration(pos);
                    auto chunk = chunkManager.getChunk(pos);
                    if (chunk) {
                        // Check if we have preloaded data
                        if (chunkManager.hasPreloadedData(pos)) {
                            auto blocks = chunkManager.getPreloadedData(pos);
                            std::copy(blocks.begin(), blocks.end(), chunk->getBlocks().begin());
                            chunk->setModified(true); // Mark as modified so it saves again
                        } else {
                            worldGenerator.generate(chunk);
                        }
                        
                        chunk->setState(ChunkState::MESH_BUILD);
                        
                        // Mark neighbors for update to ensure no gaps
                        auto neighbors = chunkManager.getNeighbors(pos);
                        for (auto& n : neighbors) {
                            if (n && n->getState() != ChunkState::UNLOADED) {
                                n->setState(ChunkState::MESH_BUILD);
                            }
                        }
                    }
                    generated++;
                    
                    // Update loading screen
                    if (generated % 5 == 0) {
                        float progress = static_cast<float>(generated) / static_cast<float>(totalChunks) * 0.5f;
                        renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
                        window->swapBuffers();
                        window->pollEvents();
                    }
                }
            }
        }
        
        // Refine spawn position to ensure we are not inside a block (e.g. tree or mountain peak)
        // We scan downwards from our high vantage point to find the first solid block.
        // Since we started high (terrain + 30), we should be in air.
        // We look for the first non-air block below us.
        int scanStartY = static_cast<int>(camera.getPosition().y);
        int currentSpawnX = static_cast<int>(camera.getPosition().x);
        int currentSpawnZ = static_cast<int>(camera.getPosition().z);
        
        bool foundGround = false;
        for (int y = scanStartY; y > 0; --y) {
            Block block = chunkManager.getBlockAt(currentSpawnX, y, currentSpawnZ);
            if (block.getType() != BlockType::AIR) {
                // Found the highest block (could be leaves, wood, or ground)
                // Set spawn point 2 blocks above it
                camera.setPosition(glm::vec3(static_cast<float>(currentSpawnX), static_cast<float>(y) + 2.5f, static_cast<float>(currentSpawnZ)));
                LOG_INFO("Spawn position refined to Y=" + std::to_string(y + 2.5f));
                foundGround = true;
                break;
            }
        }
        
        // Fallback if something went wrong (e.g. chunks not loaded), though unlikely
        if (!foundGround) {
             LOG_INFO("Could not find ground via raycast, using default height.");
        }

        // Now wait for initial meshing of this small radius
        LOG_INFO("Building initial meshes...");
        bool initialLoadDone = false;
        int meshedCount = 0;
        int totalInitialChunks = (initialRadius * 2 + 1) * (initialRadius * 2 + 1) * 5; // Approx count
        
        while (!initialLoadDone && !window->shouldClose()) {
            auto chunksToMesh = chunkManager.getChunksToMesh(camera.getPosition(), 100); // Mesh as many as possible
            
            if (chunksToMesh.empty()) {
                initialLoadDone = true;
            } else {
                for (auto& chunk : chunksToMesh) {
                    auto neighbors = chunkManager.getNeighbors(chunk->getPosition());
                    
                    MeshData meshData = meshBuilder.buildChunkMesh(chunk, 
                        neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], chunk->getCurrentLOD());
                        
                    renderer.uploadChunkMesh(chunk->getPosition(), 
                        meshData.vertices, meshData.indices, 
                        meshData.waterVertices, meshData.waterIndices);
                        
                    chunk->setState(ChunkState::GPU_UPLOADED);
                    meshedCount++;
                }
                
                float progress = 0.5f + (static_cast<float>(meshedCount) / static_cast<float>(totalInitialChunks)) * 0.5f;
                if (progress > 1.0f) progress = 1.0f;
                
                renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
                window->swapBuffers();
                window->pollEvents();
            }
        }
    }
    
    bool loadWorld(const std::string& name = "world.dat") {
        LOG_INFO("Loading world: " + name);
        
        // Clear existing world
        chunkManager.unloadAll();
        chunkManager.clear();
        
        glm::vec3 playerPos;
        long seed;
        
        if (WorldSerializer::loadWorld(name, chunkManager, playerPos, seed)) {
            camera.setPosition(playerPos);
            currentWorldName = name;
            currentSeed = seed;
            worldGenerator.setSeed(static_cast<unsigned int>(seed));
            
            LOG_INFO("World loaded successfully");
            return true;
        } else {
            LOG_ERROR("Failed to load world");
            return false;
        }
    }
    
    void run() {
        LOG_INFO("Starting main loop");
        
        Time::instance().reset();
        
        while (!window->shouldClose() && running) {
            Time::instance().update();
            float deltaTime = Time::instance().getDeltaTime();
            
            processInput(deltaTime);
            update(deltaTime);

            // Update Debug Info
            // Use a smoothed FPS for display to avoid 0 or flickering
            static float displayFPS = 0.0f;
            static float fpsAccumulator = 0.0f;
            static int frameAccumulator = 0;
            static float fpsUpdateTimer = 0.0f;
            
            fpsAccumulator += Time::instance().getFPS();
            frameAccumulator++;
            fpsUpdateTimer += deltaTime;
            
            if (fpsUpdateTimer >= 0.5f) {
                displayFPS = fpsAccumulator / frameAccumulator;
                fpsAccumulator = 0.0f;
                frameAccumulator = 0;
                fpsUpdateTimer = 0.0f;
            }

            std::string blockName = "None";
            
            // Increase raycast distance to ensure we hit the ground even from high up
            auto result = chunkManager.rayCast(camera.getPosition(), camera.getFront(), 100.0f);
            if (result.hit) {
                glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
                int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
                int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
                Block block = chunkManager.getBlockAt(x, y, z);
                
                switch (block.getType()) {
                    case BlockType::AIR: blockName = "Air"; break;
                    case BlockType::GRASS: blockName = "Grass"; break;
                    case BlockType::DIRT: blockName = "Dirt"; break;
                    case BlockType::STONE: blockName = "Stone"; break;
                    case BlockType::SAND: blockName = "Sand"; break;
                    case BlockType::WATER: blockName = "Water"; break;
                    case BlockType::WOOD: blockName = "Wood"; break;
                    case BlockType::LEAVES: blockName = "Leaves"; break;
                    case BlockType::SNOW: blockName = "Snow"; break;
                    case BlockType::ICE: blockName = "Ice"; break;
                    case BlockType::GRAVEL: blockName = "Gravel"; break;
                    case BlockType::SANDSTONE: blockName = "Sandstone"; break;
                    default: blockName = "Unknown"; break;
                }
            }
            
            uiManager.updateDebugInfo(displayFPS, blockName, camera.getPosition());

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
    UIManager uiManager;
    WorldSerializer worldSerializer;
    
    std::mutex meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>> pendingMeshes;

    double lastX, lastY;
    double lastSpaceTime = 0.0;
    bool firstMouse;
    bool running;
    
    // Game State
    std::string currentWorldName = "New World";
    long currentSeed = 12345;
    
    void applySettings() {
        auto& s = Settings::instance();
        camera.setFov(s.fov);
        camera.setSensitivity(s.mouseSensitivity);
        window->setVSync(s.vsync);
        window->setFullscreen(s.fullscreen);
        // Render distance is handled in ChunkManager::update
        // AO and Gamma are handled in Renderer::render
    }

    void onMouseButton(int button, int action, int /*mods*/) {
        if (uiManager.isMenuOpen()) return;

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
        // Mouse input
        double xpos, ypos;
        glfwGetCursorPos(window->getNative(), &xpos, &ypos);
        
        // Handle DPI scaling for UI only
        int winW, winH;
        glfwGetWindowSize(window->getNative(), &winW, &winH);
        int fbW, fbH;
        glfwGetFramebufferSize(window->getNative(), &fbW, &fbH);
        
        double uiX = xpos;
        double uiY = ypos;
        
        if (winW > 0 && winH > 0) {
            uiX *= (double)fbW / winW;
            uiY *= (double)fbH / winH;
        }

        bool mousePressed = glfwGetMouseButton(window->getNative(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        
        if (uiManager.isMenuOpen()) {
            uiManager.update(deltaTime, uiX, uiY, mousePressed);
            firstMouse = true; // Reset mouse look when returning to game
            return;
        }

        bool forward = window->isKeyPressed(GLFW_KEY_W);
        bool backward = window->isKeyPressed(GLFW_KEY_S);
        bool left = window->isKeyPressed(GLFW_KEY_A);
        bool right = window->isKeyPressed(GLFW_KEY_D);
        bool up = window->isKeyPressed(GLFW_KEY_SPACE);
        bool down = window->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
        
        camera.processInput(forward, backward, left, right, up, down, deltaTime);
        
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        
        // Use raw coordinates for camera movement to avoid DPI scaling artifacts
        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(lastY - ypos); // Reversed
        
        lastX = xpos;
        lastY = ypos;
        
        // Ignore micro-movements (jitter) from high-DPI mice or sensor noise
        if (std::abs(xoffset) < 0.1f) xoffset = 0.0f;
        if (std::abs(yoffset) < 0.1f) yoffset = 0.0f;

        camera.processMouseMovement(xoffset, yoffset);
    }
    
    void update(float deltaTime) {
        if (uiManager.isMenuOpen()) return;

        // Day/Night Cycle
        // Full cycle = 2400 seconds (40 minutes) - Drastically increased
        // 0 = Sunrise, 600 = Noon, 1200 = Sunset, 1800 = Midnight
        constexpr float DAY_DURATION = 2400.0f; 
        
        // Debug Controls
        static bool f1Pressed = false;
        if (window->isKeyPressed(GLFW_KEY_F1)) {
            if (!f1Pressed) {
                uiManager.toggleDebug();
                f1Pressed = true;
            }
        } else {
            f1Pressed = false;
        }

        static bool f2Pressed = false;
        if (window->isKeyPressed(GLFW_KEY_F2)) {
            if (!f2Pressed) {
                uiManager.isDayNightPaused = !uiManager.isDayNightPaused;
                f2Pressed = true;
            }
        } else {
            f2Pressed = false;
        }
        
        static bool f3Pressed = false;
        if (window->isKeyPressed(GLFW_KEY_F3)) {
            if (!f3Pressed) {
                Settings::instance().enableShadows = !Settings::instance().enableShadows;
                f3Pressed = true;
            }
        } else {
            f3Pressed = false;
        }
        
        // renderer.setShowShadows(uiManager.showShadows); // Removed, Renderer uses Settings directly

        if (!uiManager.isDayNightPaused) {
            uiManager.timeOfDay += deltaTime * 10.0f; // Still sped up slightly for gameplay, but slower than before
        }
        
        // Manual Time Control
        if (window->isKeyPressed(GLFW_KEY_RIGHT)) uiManager.timeOfDay += deltaTime * 100.0f;
        if (window->isKeyPressed(GLFW_KEY_LEFT)) uiManager.timeOfDay -= deltaTime * 100.0f;
        
        if (uiManager.timeOfDay >= DAY_DURATION) uiManager.timeOfDay -= DAY_DURATION;
        if (uiManager.timeOfDay < 0.0f) uiManager.timeOfDay += DAY_DURATION;
        
        float angle = (uiManager.timeOfDay / DAY_DURATION) * glm::two_pi<float>();
        
        // Sun moves East (X+) -> Up (Y+) -> West (X-) -> Down (Y-)
        // We start at sunrise (X+, Y=0)
        float sunX = cos(angle);
        float sunY = sin(angle);
        float sunZ = 0.2f; // Slight tilt
        
        glm::vec3 sunDir = glm::normalize(glm::vec3(sunX, sunY, sunZ));
        
        // Night Logic: If sun is below horizon, use Moon
        if (sunY < -0.1f) {
            // Moon is opposite to sun
            glm::vec3 moonDir = -sunDir;
            renderer.setLightDirection(moonDir);
            // Moon is dimmer
            // We handle intensity in shader or by color, but direction is key for shadows
        } else {
            renderer.setLightDirection(sunDir);
        }
        
        // Calculate sky color based on sun height (sunY)
        glm::vec3 dayColor(0.53f, 0.81f, 0.92f);
        glm::vec3 nightColor(0.05f, 0.05f, 0.1f);
        glm::vec3 sunsetColor(0.8f, 0.4f, 0.2f);
        
        glm::vec3 currentSkyColor;
        
        if (sunY > 0.2f) {
            // Day
            currentSkyColor = dayColor;
        } else if (sunY < -0.2f) {
            // Night
            currentSkyColor = nightColor;
        } else {
            // Transition (Sunrise/Sunset)
            float t = (sunY + 0.2f) / 0.4f; // Map -0.2..0.2 to 0..1
            if (sunX > 0) {
                // Sunrise (Night -> Day)
                // sunY goes -0.2 -> 0.2
                currentSkyColor = glm::mix(nightColor, dayColor, t);
                // Add some orange glow
                float glow = 1.0f - abs(t - 0.5f) * 2.0f;
                currentSkyColor = glm::mix(currentSkyColor, sunsetColor, glow * 0.5f);
            } else {
                // Sunset (Day -> Night)
                // sunY goes 0.2 -> -0.2
                currentSkyColor = glm::mix(nightColor, dayColor, t);
                // Add some orange glow
                float glow = 1.0f - abs(t - 0.5f) * 2.0f;
                currentSkyColor = glm::mix(currentSkyColor, sunsetColor, glow * 0.5f);
            }
        }

        // If player is deep underground, fade sky color to black
        // This prevents seeing "bright sky" when looking into the void/unloaded chunks underground
        if (camera.getPosition().y < 40.0f) {
            float depthFactor = std::clamp((40.0f - camera.getPosition().y) / 20.0f, 0.0f, 1.0f);
            currentSkyColor = glm::mix(currentSkyColor, glm::vec3(0.0f), depthFactor);
        }

        renderer.setSkyColor(currentSkyColor);

        updatePhysics(deltaTime);
        camera.update(deltaTime);
        chunkManager.update(camera.getPosition());
        
        // Generate chunks
        auto chunksToGenerate = chunkManager.getChunksToGenerate(camera.getPosition(), Settings::instance().renderDistance, 10);
        for (const auto& pos : chunksToGenerate) {
            chunkManager.requestChunkGeneration(pos);
            auto chunk = chunkManager.getChunk(pos);
            if (chunk && chunk->getState() == ChunkState::UNLOADED) {
                chunk->setState(ChunkState::GENERATING);
                
                // Generate in thread pool
                threadPool.enqueue([this, chunk]() {
                    if (chunkManager.hasPreloadedData(chunk->getPosition())) {
                        auto blocks = chunkManager.getPreloadedData(chunk->getPosition());
                        std::copy(blocks.begin(), blocks.end(), chunk->getBlocks().begin());
                        chunk->setModified(true);
                    } else {
                        worldGenerator.generate(chunk);
                    }
                    
                    // Scan for water to initialize fluid simulation
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                            for (int z = 0; z < CHUNK_SIZE; ++z) {
                                if (chunk->getBlock(x, y, z).getType() == BlockType::WATER) {
                                    glm::vec3 worldPos = ChunkManager::chunkToWorld(chunk->getPosition());
                                    chunkManager.scheduleFluidUpdate(
                                        static_cast<int>(worldPos.x) + x,
                                        static_cast<int>(worldPos.y) + y,
                                        static_cast<int>(worldPos.z) + z
                                    );
                                }
                            }
                        }
                    }
                    
                    chunk->setState(ChunkState::MESH_BUILD);
                });
            }
        }
        
        // Build meshes
        auto chunksToMesh = chunkManager.getChunksToMesh(camera.getPosition(), MAX_MESHES_PER_FRAME);
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
            
            int lod = chunk->getCurrentLOD();

            threadPool.enqueue([this, chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg, lod]() {
                auto meshData = meshBuilder.buildChunkMesh(chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg, lod);
                
                std::lock_guard<std::mutex> lock(meshMutex);
                pendingMeshes.emplace_back(chunk->getPosition(), std::move(meshData));
            });
        }

        // Upload meshes
        {
            std::lock_guard<std::mutex> lock(meshMutex);
            for (auto& [pos, meshData] : pendingMeshes) {
                if (!meshData.isEmpty()) {
                    renderer.uploadChunkMesh(pos, meshData.vertices, meshData.indices, meshData.waterVertices, meshData.waterIndices);
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
                    // Ensure we clear any existing mesh for this chunk
                    renderer.uploadChunkMesh(pos, {}, {}, {}, {});
                }
            }
            pendingMeshes.clear();
        }
    }
    
    void render() {
        renderer.render(chunkManager, camera, window->getWidth(), window->getHeight());
        uiManager.render();
    }
    
    void updatePhysics(float deltaTime) {
        if (camera.getFlightMode()) return;
        
        // Check if in water
        bool inWater = false;
        glm::vec3 camPos = camera.getPosition();
        // Check eye level and feet level
        Block headBlock = chunkManager.getBlockAt(static_cast<int>(floor(camPos.x)), static_cast<int>(floor(camPos.y)), static_cast<int>(floor(camPos.z)));
        Block feetBlock = chunkManager.getBlockAt(static_cast<int>(floor(camPos.x)), static_cast<int>(floor(camPos.y - 1.5f)), static_cast<int>(floor(camPos.z)));
        
        if (headBlock.isWater() || feetBlock.isWater()) {
            inWater = true;
        }
        
        if (inWater) {
            // Water physics
            // Drag
            float drag = 1.0f - (2.0f * deltaTime);
            drag = std::max(0.0f, drag);
            camera.velocity.x *= drag;
            camera.velocity.z *= drag;
            camera.velocity.y *= drag;
            
            // Buoyancy / Swim
            if (window->isKeyPressed(GLFW_KEY_SPACE)) {
                camera.velocity.y += 10.0f * deltaTime;
            } else if (window->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                camera.velocity.y -= 10.0f * deltaTime;
            }
            
            // Slight gravity if not swimming
            if (!window->isKeyPressed(GLFW_KEY_SPACE)) {
                 camera.velocity.y -= 2.0f * deltaTime;
            }
            
            // Terminal velocity in water
            camera.velocity.y = std::max(-4.0f, std::min(4.0f, camera.velocity.y));
            
        } else {
            // Normal gravity
            camera.velocity.y -= 18.0f * deltaTime;
        }
        
        // Apply velocity
        glm::vec3 pos = camera.getPosition();
        glm::vec3 vel = camera.velocity * deltaTime;
        
        // Try X movement
        if (checkCollision(glm::vec3(pos.x + vel.x, pos.y, pos.z))) {
            vel.x = 0;
            camera.velocity.x = 0;
        }
        pos.x += vel.x;
        
        // Try Z movement
        if (checkCollision(glm::vec3(pos.x, pos.y, pos.z + vel.z))) {
            vel.z = 0;
            camera.velocity.z = 0;
        }
        pos.z += vel.z;
        
        // Try Y movement
        if (checkCollision(glm::vec3(pos.x, pos.y + vel.y, pos.z))) {
            if (vel.y < 0) camera.onGround = true;
            vel.y = 0;
            camera.velocity.y = 0;
        } else {
            camera.onGround = false;
        }
        pos.y += vel.y;
        
        camera.setPosition(pos);
        
        // Friction
        float friction = camera.onGround ? 10.0f : 2.0f;
        float damping = 1.0f / (1.0f + friction * deltaTime);
        camera.velocity.x *= damping;
        camera.velocity.z *= damping;
    }
    
    bool checkCollision(const glm::vec3& pos) {
        float minX = pos.x - 0.3f;
        float maxX = pos.x + 0.3f;
        float minY = pos.y - 1.6f;
        float maxY = pos.y + 0.2f;
        float minZ = pos.z - 0.3f;
        float maxZ = pos.z + 0.3f;
        
        for (int x = static_cast<int>(floor(minX)); x <= static_cast<int>(floor(maxX)); x++) {
            for (int y = static_cast<int>(floor(minY)); y <= static_cast<int>(floor(maxY)); y++) {
                for (int z = static_cast<int>(floor(minZ)); z <= static_cast<int>(floor(maxZ)); z++) {
                    Block block = chunkManager.getBlockAt(x, y, z);
                    if (block.isSolid()) return true;
                }
            }
        }
        return false;
    }
};

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
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
