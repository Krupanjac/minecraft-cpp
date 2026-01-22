#define NOMINMAX
#include "Core/Window.h"
#include "Core/Time.h"
#include "Core/Logger.h"
#include "Core/ThreadPool.h"
#include "Core/Settings.h"
#include "Core/HardwareInfo.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
#include "World/ChunkManager.h"
#include "World/WorldGenerator.h"
#include "Mesh/MeshBuilder.h"
#include "Util/Config.h"
#include "UI/UIManager.h"
#include "World/WorldSerializer.h"
#include "Entity/PlayerEntity.h"
#include "Entity/EntityManager.h"
#include "Entity/ZombieEntity.h"
#include "Entity/SkeletonEntity.h"
#include "Entity/PigEntity.h"
#include "Entity/ChickenEntity.h"
#include "Entity/SheepEntity.h"
#include "Entity/MobSpawnManager.h"
#include "Network/NetworkManager.h"
#include "Audio/AudioManager.h"

#include <memory>
#include <iostream>
#include <mutex>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <unordered_map>

class Application {
public:
    Application() 
        : camera(glm::vec3(0.0f, 80.0f, 0.0f)),
          threadPool(HardwareInfo::getOptimalThreadCount()),
          lastX(0.0), lastY(0.0), lastSpaceTime(0.0), firstMouse(true),
          running(true) {
    }
    
    ~Application() = default;
    
    bool initialize() {
        LOG_INFO("Initializing Minecraft C++ Engine");
        
        // Log hardware information
        auto cpuInfo = HardwareInfo::getCPUInfo();
        auto memInfo = HardwareInfo::getMemoryInfo();
        LOG_INFO("CPU: " + cpuInfo.name);
        LOG_INFO("CPU Cores: " + std::to_string(cpuInfo.physicalCores) + " physical, " + std::to_string(cpuInfo.logicalCores) + " logical");
        LOG_INFO("RAM: " + std::to_string(memInfo.totalPhysicalMB) + " MB total, " + std::to_string(memInfo.availablePhysicalMB) + " MB available");
        LOG_INFO("Thread pool size: " + std::to_string(HardwareInfo::getOptimalThreadCount()));
        
        try {
            window = std::make_unique<Window>(1280, 720, "Minecraft C++");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create window: " + std::string(e.what()));
            return false;
        }
        
        // Get GPU info now that OpenGL context is created
        const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        HardwareInfo::setGPUInfo(
            glRenderer ? glRenderer : "Unknown GPU",
            glVendor ? glVendor : "Unknown",
            glVersion ? glVersion : "Unknown"
        );
        LOG_INFO("GPU: " + std::string(glRenderer ? glRenderer : "Unknown"));
        LOG_INFO("OpenGL: " + std::string(glVersion ? glVersion : "Unknown"));
        
        // Start with cursor visible for menu
        window->setCursorMode(GLFW_CURSOR_NORMAL);
        
        window->setMouseButtonCallback([this](int button, int action, int mods) {
            onMouseButton(button, action, mods);
        });
        
        window->setKeyCallback([this](int key, int, int action, int) {
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
            
            if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
                camera.toggleThirdPerson();
            }

            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                // Handle chat close first
                if (uiManager.isChatOpen()) {
                    uiManager.closeChat();
                    window->setCursorMode(GLFW_CURSOR_DISABLED);
                    return;
                }
                
                // Only allow closing menu to game if a world is loaded
                if (uiManager.isWorldLoaded()) {
                    bool isMenuOpen = uiManager.isMenuOpen();
                    uiManager.setMenuState(isMenuOpen ? MenuState::NONE : MenuState::IN_GAME_MENU);
                    window->setCursorMode(isMenuOpen ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                } else {
                    // In main menu without world - ESC does nothing or goes back in menu hierarchy
                    MenuState currentState = uiManager.getMenuState();
                    if (currentState == MenuState::SETTINGS || currentState == MenuState::VIDEO_SETTINGS ||
                        currentState == MenuState::AUDIO_SETTINGS || currentState == MenuState::CONTROLS || 
                        currentState == MenuState::NEW_GAME || currentState == MenuState::LOAD_GAME || 
                        currentState == MenuState::MULTIPLAYER || currentState == MenuState::HOST_GAME || 
                        currentState == MenuState::JOIN_GAME || currentState == MenuState::PLAYER_SETTINGS) {
                        // Go back to main menu from sub-menus
                        if (currentState == MenuState::VIDEO_SETTINGS || currentState == MenuState::CONTROLS ||
                            currentState == MenuState::AUDIO_SETTINGS || currentState == MenuState::PLAYER_SETTINGS) {
                            uiManager.setMenuState(MenuState::SETTINGS);
                        } else if (currentState == MenuState::HOST_GAME || currentState == MenuState::JOIN_GAME) {
                            uiManager.setMenuState(MenuState::MULTIPLAYER);
                        } else {
                            uiManager.setMenuState(MenuState::MAIN_MENU);
                        }
                    }
                }
            }

            if (action == GLFW_PRESS) {
                if (key == Settings::instance().keys.inventory) {
                    if (uiManager.getMenuState() == MenuState::INVENTORY) {
                        uiManager.setMenuState(MenuState::NONE);
                        window->setCursorMode(GLFW_CURSOR_DISABLED);
                    } else if (uiManager.getMenuState() == MenuState::NONE) {
                        uiManager.setMenuState(MenuState::INVENTORY);
                        window->setCursorMode(GLFW_CURSOR_NORMAL);
                    }
                }
                
                // M key toggles map
                if (key == GLFW_KEY_M) {
                    if (uiManager.getMenuState() == MenuState::MAP) {
                        uiManager.setMenuState(MenuState::NONE);
                        window->setCursorMode(GLFW_CURSOR_DISABLED);
                    } else if (uiManager.getMenuState() == MenuState::NONE) {
                        uiManager.setMenuState(MenuState::MAP);
                        window->setCursorMode(GLFW_CURSOR_NORMAL);
                    }
                }
                
                // T key opens chat (multiplayer only)
                if (key == GLFW_KEY_T && networkManager.isOnline()) {
                    if (uiManager.getMenuState() == MenuState::NONE) {
                        uiManager.openChat();
                        window->setCursorMode(GLFW_CURSOR_NORMAL);
                    }
                }

                if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                    uiManager.selectHotbarSlot(key - GLFW_KEY_1);
                }
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
            uiManager.setWorldLoaded(true);
            uiManager.setMenuState(MenuState::NONE);
            window->setCursorMode(GLFW_CURSOR_DISABLED);
            // Stop menu music
            Audio::AudioManager::instance().stopMusic(2.0f);
        });
        
        uiManager.setOnLoadGame([this](std::string name) {
            if (loadWorld(name)) {
                uiManager.setWorldLoaded(true);
                uiManager.setMenuState(MenuState::NONE);
                window->setCursorMode(GLFW_CURSOR_DISABLED);
                // Stop menu music
                Audio::AudioManager::instance().stopMusic(2.0f);
            }
        });
        
        uiManager.setOnSave([this]() {
            // Save world data
            WorldSerializer::saveWorld(currentWorldName, chunkManager, camera.getPosition(), currentSeed);
            
            // Capture screenshot for world preview
            int w = window->getWidth();
            int h = window->getHeight();
            std::vector<unsigned char> pixels(w * h * 3);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            WorldSerializer::saveScreenshot(currentWorldName, pixels.data(), w, h);
            
            LOG_INFO("Game Saved with preview screenshot");
        });
        
        uiManager.setOnExit([this]() {
            window->close();
        });
        
        uiManager.setOnSettingsChanged([this]() {
            applySettings();
        });
        
        // Return to main menu callback - reinitialize menu world
        uiManager.setOnReturnToMainMenu([this]() {
            LOG_INFO("Returning to main menu, reinitializing menu world...");
            
            // Clear entities from new system
            entityManager.clear();
            
            // Clear legacy containers
            zombies.clear();
            skeletons.clear();
            pigs.clear();
            chickens.clear();
            sheep.clear();
            
            menuWorldInitialized = false; // Force reinitialization
            initializeMenuWorld();
        });
        
        // Setup teleport callback for map
        uiManager.setOnTeleport([this](float x, float z) {
            // Get height at target location (from generator which is deterministic)
            float height = worldGenerator.getHeight(x, z);
            // Teleport camera high above the target so we can safely prefetch chunks below
            camera.setPosition(glm::vec3(x, height + 30.0f, z));
            // Reset velocity to avoid immediate fall-through on teleports
            camera.velocity = glm::vec3(0.0f);
            LOG_INFO("Teleported to: " + std::to_string(x) + ", " + std::to_string(z));

            // Immediately generate & mesh nearby chunks synchronously to avoid falling through and missing water/shadows
            const int teleportPrefetchRadius = 4; // chunks
            // Generate
            auto chunksToGen = chunkManager.getChunksToGenerate(camera.getPosition(), teleportPrefetchRadius, 10000);
            for (const auto& pos : chunksToGen) {
                chunkManager.requestChunkGeneration(pos);
                auto chunk = chunkManager.getChunk(pos);
                if (chunk) {
                    if (chunkManager.hasPreloadedData(pos)) {
                        auto blocks = chunkManager.getPreloadedData(pos);
                        std::copy(blocks.begin(), blocks.end(), chunk->getBlocks().begin());
                        chunk->setModified(true);
                    } else {
                        worldGenerator.generate(chunk);
                    }
                    chunk->setState(ChunkState::MESH_BUILD);
                }
            }
            // Mesh synchronously (avoid large freeze by limiting count)
            auto chunksToMesh = chunkManager.getChunksToMesh(camera.getPosition(), 1000);
            int meshed = 0;
            for (auto& chunk : chunksToMesh) {
                if (meshed > 200) break; // cap work this frame to avoid massive hitch; still much faster than falling through
                auto neighbors = chunkManager.getNeighbors(chunk->getPosition());
                MeshData meshData = meshBuilder.buildChunkMesh(chunk,
                    neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], chunk->getCurrentLOD());

                renderer.uploadChunkMesh(chunk->getPosition(), meshData.vertices, meshData.indices, meshData.waterVertices, meshData.waterIndices);
                chunk->setState(ChunkState::GPU_UPLOADED);
                meshed++;
            }

            // After some synchronous meshing, lower camera to safe spawn height based on blocks now present
            int terrainY = chunkManager.getHeightAt(static_cast<int>(x), static_cast<int>(z));
            camera.setPosition(glm::vec3(x, static_cast<float>(terrainY) + 2.0f, z));
        });
        
        // Give UIManager access to world generator for map
        uiManager.setWorldGenerator(&worldGenerator);
        
        // Setup multiplayer callbacks
        uiManager.setOnHostGame([this](std::string playerName, int port) {
            hostMultiplayerGame(playerName, static_cast<uint16_t>(port));
        });
        
        uiManager.setOnJoinGame([this](std::string playerName, std::string address, int port) {
            joinMultiplayerGame(playerName, address, static_cast<uint16_t>(port));
        });
        
        uiManager.setOnDisconnect([this]() {
            networkManager.disconnect();
            uiManager.setIsOnline(false);
            uiManager.setNetworkStatus("");
        });
        
        // Setup network callbacks
        networkManager.setBlockChangeCallback([this](int x, int y, int z, uint8_t blockType) {
            chunkManager.setBlockAt(x, y, z, Block(static_cast<BlockType>(blockType)));
        });
        
        networkManager.setDisconnectCallback([this](const std::string& reason) {
            LOG_INFO("Disconnected from server: " + reason);
            uiManager.setIsOnline(false);
            uiManager.setNetworkStatus("Disconnected: " + reason);
            uiManager.setWorldLoaded(false);
            // Re-initialize menu world
            entityManager.clear();
            zombies.clear();
            skeletons.clear();
            pigs.clear();
            chickens.clear();
            sheep.clear();
            menuWorldInitialized = false;
            initializeMenuWorld();
            uiManager.setMenuState(MenuState::MAIN_MENU);
        });
        
        networkManager.setConnectedCallback([this]() {
            if (networkManager.isClient()) {
                // Client just connected - use server's world seed
                createWorld("Multiplayer", static_cast<long>(networkManager.getWorldSeed()));
                glm::vec3 spawn = networkManager.getSpawnPosition();
                camera.setPosition(spawn);
            }
            uiManager.setWorldLoaded(true);
            uiManager.setMenuState(MenuState::NONE);
            window->setCursorMode(GLFW_CURSOR_DISABLED);
            uiManager.setIsOnline(true);
            uiManager.setNetworkStatus("Connected");
        });
        
        // Time sync callback (for clients receiving server time)
        // Only set pause state immediately, time will be corrected gradually to avoid jarring jumps
        networkManager.setTimeSyncCallback([this](float serverTime, bool isPaused) {
            constexpr float DAY_LENGTH = 2400.0f;
            uiManager.isDayNightPaused = isPaused;
            
            // Calculate time difference (accounting for day wrap-around)
            float diff = serverTime - uiManager.timeOfDay;
            if (diff > DAY_LENGTH / 2.0f) diff -= DAY_LENGTH;
            if (diff < -DAY_LENGTH / 2.0f) diff += DAY_LENGTH;
            
            // If drift is small (< 50 time units), let local time continue
            // If drift is larger, snap to server time to prevent major desync
            if (std::abs(diff) > 50.0f) {
                uiManager.timeOfDay = serverTime;
            }
            // Otherwise local time continues and will naturally sync
        });
        
        // Entity sync callbacks (for clients receiving host's entity states)
        networkManager.setEntitySpawnCallback([this](uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw) {
            if (!useNewEntityManager) return;
            // Spawn entity on client side
            entityManager.spawnFromNetwork(static_cast<MobType>(mobType), pos, entityId, yaw);
        });
        
        networkManager.setEntityDespawnCallback([this](uint32_t entityId) {
            if (!useNewEntityManager) return;
            entityManager.despawnById(entityId);
        });
        
        networkManager.setEntityUpdateCallback([this](uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags) {
            if (!useNewEntityManager) return;
            // Update entity state on client
            Entity* entity = entityManager.getEntityById(entityId);
            if (entity) {
                entity->setPosition(pos);
                entity->setVelocity(vel);
                entity->setRotation(glm::vec3(0.0f, yaw, 0.0f));
                // flags bit 0 = isDead
                if (flags & 0x01) {
                    entityManager.killEntity(entityId);
                }
            }
            // Note: If entity doesn't exist, we ignore the update.
            // The server will send ENTITY_SPAWN first for new entities.
            // If we missed the spawn (e.g., joined late), we'll get it on the next spawn broadcast.
        });
        
        // Chat callbacks - wire UI to network
        uiManager.setOnSendChat([this](const std::string& message) {
            networkManager.sendChatMessage(message);
        });
        
        networkManager.setChatCallback([this](const std::string& playerName, const std::string& message) {
            uiManager.addChatMessage(playerName, message);
        });
        
        // Player join callback - send all existing entities to new player
        networkManager.setPlayerJoinCallback([this](uint32_t playerId, const std::string& name) {
            if (!useNewEntityManager) return;
            
            // Send all existing entity spawns to the new player
            // (broadcasts go to all players, but that's fine - they'll just update positions)
            auto entityStates = entityManager.getEntityStatesForSync();
            LOG_INFO("Syncing " + std::to_string(entityStates.size()) + " entities to new player " + name);
            
            for (const auto& state : entityStates) {
                networkManager.broadcastEntitySpawn(
                    state.id,
                    static_cast<uint8_t>(state.type),
                    state.position,
                    state.yaw
                );
            }
        });
        
        // Initialize audio system
        if (!Audio::AudioManager::instance().initialize()) {
            LOG_WARNING("Failed to initialize audio system - continuing without sound");
        } else {
            Audio::AudioManager::instance().loadAllSounds();
            // Set initial volume from settings
            Audio::AudioManager::instance().setMasterVolume(Settings::instance().masterVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, Settings::instance().musicVolume);
        }
        
        // Apply initial settings
        applySettings();
        
        // Start in Main Menu
        uiManager.setMenuState(MenuState::MAIN_MENU);
        
        LOG_INFO("Application initialized successfully");
        return true;
    }
    
    void hostMultiplayerGame(const std::string& playerName, uint16_t port) {
        // Create a new world for hosting
        long seed = static_cast<long>(time(nullptr));
        createWorld("Multiplayer_" + std::to_string(seed), seed);
        
        // Clear mobs - multiplayer uses server-authoritative mobs
        entityManager.clear();
        entityManager.setNetworkMode(true, false); // Server mode
        zombies.clear();
        skeletons.clear();
        pigs.clear();
        chickens.clear();
        sheep.clear();
        
        // Start the server with player's model index
        uint8_t modelIdx = static_cast<uint8_t>(Settings::instance().playerModelIndex);
        if (networkManager.hostGame(port, seed, camera.getPosition(), playerName, modelIdx)) {
            uiManager.setMenuState(MenuState::NONE);
            window->setCursorMode(GLFW_CURSOR_DISABLED);
            uiManager.setIsOnline(true);
            uiManager.setNetworkStatus("Hosting on port " + std::to_string(port));
            LOG_INFO("Hosting multiplayer game on port " + std::to_string(port));
        } else {
            uiManager.setNetworkStatus("Failed to start server");
            LOG_ERROR("Failed to host multiplayer game");
        }
    }
    
    void joinMultiplayerGame(const std::string& playerName, const std::string& address, uint16_t port) {
        uiManager.setNetworkStatus("Connecting...");
        
        // Clear mobs - client receives mob state from server
        entityManager.clear();
        entityManager.setNetworkMode(false, true); // Client mode
        zombies.clear();
        skeletons.clear();
        pigs.clear();
        chickens.clear();
        sheep.clear();
        
        uint8_t modelIdx = static_cast<uint8_t>(Settings::instance().playerModelIndex);
        if (networkManager.joinGame(address, port, playerName, modelIdx)) {
            // Connection initiated - wait for connected callback
            LOG_INFO("Connecting to " + address + ":" + std::to_string(port));
        } else {
            uiManager.setNetworkStatus("Failed to connect");
            LOG_ERROR("Failed to connect to server");
        }
    }
    
    void initializeMenuWorld() {
        LOG_INFO("Initializing menu background world...");
        
        // CRITICAL: Wait for all pending thread pool tasks to complete before clearing
        // This prevents race conditions where game world chunks get mixed with menu world
        threadPool.wait();
        
        // Clear any existing world data first
        chunkManager.unloadAll();
        chunkManager.clear();
        renderer.clear();
        
        // Initialize seed ONCE per .exe session (O(1) lookup for subsequent calls)
        if (!menuSeedInitialized) {
            menuWorldSeed = static_cast<long>(std::chrono::system_clock::now().time_since_epoch().count());
            menuSeedInitialized = true;
            LOG_INFO("Menu world seed initialized: " + std::to_string(menuWorldSeed));
        } else {
            LOG_INFO("Reusing cached menu world seed: " + std::to_string(menuWorldSeed));
        }
        worldGenerator.setSeed(static_cast<unsigned int>(menuWorldSeed));
        
        // Find a nice scenic spot - keep all camera positions within a reasonable area
        int centerX = 0;
        int centerZ = 0;
        int terrainHeight = worldGenerator.getSurfaceHeight(centerX, centerZ);
        float baseY = static_cast<float>(terrainHeight) + 25.0f; // Higher above ground
        
        // Scene positions with more movement but still within pre-loaded area (~50 block radius)
        menuScenePositions[0] = glm::vec3(centerX, baseY, centerZ);
        menuScenePositions[1] = glm::vec3(centerX + 40.0f, baseY + 8.0f, centerZ + 35.0f);
        menuScenePositions[2] = glm::vec3(centerX - 35.0f, baseY + 5.0f, centerZ + 25.0f);
        menuScenePositions[3] = glm::vec3(centerX + 25.0f, baseY + 3.0f, centerZ - 40.0f);
        
        menuCamera.setPosition(menuScenePositions[0]);
        menuCamera.setYaw(menuCameraYaw);
        menuCamera.setPitch(-15.0f); // Looking down at the terrain
        menuCameraPitch = -15.0f;
        menuCamera.setFov(70.0f);
        
        // Pre-generate a good sized area with loading screen
        // Radius of 6 gives 13x13 chunks = ~208 blocks coverage, enough for 50 block camera movement
        int menuRadius = 6;
        int totalChunks = (menuRadius * 2 + 1) * (menuRadius * 2 + 1) * 8;
        int generatedChunks = 0;
        
        // Check if we have cached data (O(1) cache lookup)
        bool usingCache = menuCacheValid && !menuChunkCache.empty();
        if (usingCache) {
            LOG_INFO("Restoring menu world from cache (" + std::to_string(menuChunkCache.size()) + " chunks)...");
        } else {
            LOG_INFO("Generating menu world chunks with loading screen...");
        }
        
        // Generate or restore all chunks with loading screen feedback
        for (int x = -menuRadius; x <= menuRadius; x++) {
            for (int z = -menuRadius; z <= menuRadius; z++) {
                for (int y = 0; y < 8; y++) {
                    ChunkPos pos = ChunkManager::worldToChunk(glm::vec3(centerX, 0, centerZ) + glm::vec3(x * CHUNK_SIZE, y * CHUNK_SIZE - 64, z * CHUNK_SIZE));
                    chunkManager.requestChunkGeneration(pos);
                    auto chunk = chunkManager.getChunk(pos);
                    
                    // Check for UNLOADED state (new chunks start as UNLOADED after requestChunkGeneration)
                    if (chunk && chunk->getState() == ChunkState::UNLOADED) {
                        // O(1) cache lookup - check if this chunk is already cached
                        auto cacheIt = menuChunkCache.find(pos);
                        if (usingCache && cacheIt != menuChunkCache.end()) {
                            // Restore from cache (O(1) lookup + O(n) copy where n = blocks in chunk)
                            chunk->setBlocks(cacheIt->second);
                        } else {
                            // Generate new chunk and cache it
                            worldGenerator.generate(chunk);
                            
                            // Store in cache for future use (O(1) insert)
                            menuChunkCache[pos] = chunk->getBlocks();
                        }
                        // Set to MESH_BUILD so getChunksToMesh will pick it up
                        chunk->setState(ChunkState::MESH_BUILD);
                    }
                    generatedChunks++;
                    
                    // Update loading screen periodically
                    if (generatedChunks % 20 == 0) {
                        float progress = static_cast<float>(generatedChunks) / static_cast<float>(totalChunks) * 0.6f;
                        renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
                        window->swapBuffers();
                        window->pollEvents();
                    }
                }
            }
        }
        
        // Mark cache as valid for future reloads
        menuCacheValid = true;
        
        // Build meshes for all generated chunks with loading screen
        LOG_INFO("Building menu world meshes...");
        auto chunksToMesh = chunkManager.getChunksToMesh(menuCamera.getPosition(), 1000);
        int totalMeshes = static_cast<int>(chunksToMesh.size());
        int meshedCount = 0;
        
        for (auto& chunk : chunksToMesh) {
            auto neighbors = chunkManager.getNeighbors(chunk->getPosition());
            MeshData meshData = meshBuilder.buildChunkMesh(chunk, 
                neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], chunk->getCurrentLOD());
            renderer.uploadChunkMesh(chunk->getPosition(), 
                meshData.vertices, meshData.indices, 
                meshData.waterVertices, meshData.waterIndices);
            chunk->setState(ChunkState::GPU_UPLOADED);
            meshedCount++;
            
            // Update loading screen periodically
            if (meshedCount % 10 == 0) {
                float progress = 0.6f + (static_cast<float>(meshedCount) / static_cast<float>(totalMeshes)) * 0.4f;
                renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
                window->swapBuffers();
                window->pollEvents();
            }
        }
        
        // Show final 100% loading screen before transitioning
        renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), 1.0f);
        window->swapBuffers();
        window->pollEvents();
        
        menuWorldInitialized = true;
        LOG_INFO("Menu background world initialized with " + std::to_string(generatedChunks) + " chunks" + 
                 (usingCache ? " (from cache)" : " (newly generated)"));
    }
    
    void updateMenuCamera(float deltaTime) {
        // Slowly rotate camera for panoramic effect
        menuCameraYaw += deltaTime * 3.0f; // 3 degrees per second (slower rotation)
        if (menuCameraYaw > 360.0f) menuCameraYaw -= 360.0f;
        
        menuCamera.setYaw(menuCameraYaw);
        menuCamera.setPitch(menuCameraPitch);
        
        // Change scene every 20 seconds with smooth transition
        menuCameraTimer += deltaTime;
        if (menuCameraTimer >= 20.0f) {
            menuCameraTimer = 0.0f;
            menuSceneIndex = (menuSceneIndex + 1) % NUM_MENU_SCENES;
        }
        
        // Smooth interpolation to target position (within pre-loaded area)
        glm::vec3 targetPos = menuScenePositions[menuSceneIndex];
        glm::vec3 currentPos = menuCamera.getPosition();
        glm::vec3 newPos = currentPos + (targetPos - currentPos) * deltaTime * 0.3f;
        menuCamera.setPosition(newPos);
        
        // NO dynamic chunk generation - everything is pre-loaded
        // Just update the view matrix for rendering
    }

    void createWorld(const std::string& name, long seed = 12345) {
        LOG_INFO("Creating new world: " + name + " with seed: " + std::to_string(seed));
        
        currentSeed = seed;
        currentWorldName = name.empty() ? "World_" + std::to_string(seed) : name;
        
        // CRITICAL: Wait for all pending thread pool tasks to complete before clearing
        // This prevents race conditions where old world chunks get inserted after clear()
        threadPool.wait();
        
        // Set seed BEFORE clearing so any residual generation uses new seed
        worldGenerator.setSeed(static_cast<unsigned int>(seed));
        
        // Clear existing world data
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
                // Reset velocities when spawning
                camera.velocity = glm::vec3(0.0f);
        
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
        
        // Spawn player entity
        glm::vec3 spawnPos = camera.getPosition();
        // Move slightly in front to see it initially, or at 0,0,0
        spawnPos.z -= 5.0f; 
        spawnPos.y = chunkManager.getHeightAt(static_cast<int>(spawnPos.x), static_cast<int>(spawnPos.z)) + 2.0f;
        
        playerEntity = std::make_unique<PlayerEntity>(spawnPos);
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
                // Reset velocity to avoid falling when refinement happened during a long frame
                camera.velocity = glm::vec3(0.0f);
                LOG_INFO("Spawn position refined to Y=" + std::to_string(y + 2.5f));
                foundGround = true;
                break;
            }
        }
        
        // Fallback if something went wrong (e.g. chunks not loaded), though unlikely
        if (!foundGround) {
             LOG_INFO("Could not find ground via raycast, using default height.");
        }
        
        // Initialize EntityManager (new system - stutter-free)
        if (useNewEntityManager) {
            entityManager.initialize(chunkManager, worldGenerator);
            
            // Preload models during loading screen to avoid runtime stutters
            LOG_INFO("Preloading mob models...");
            renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), 0.95f);
            window->swapBuffers();
            window->pollEvents();
            entityManager.preloadModels();
            
            // Set network mode based on current state
            entityManager.setNetworkMode(networkManager.isHost(), networkManager.isClient());
        }
        
        // Initialize legacy mob spawn manager (fallback)
        mobSpawnManager = std::make_unique<MobSpawnManager>(chunkManager, worldGenerator);
    }
    
    bool loadWorld(const std::string& name = "world.dat") {
        LOG_INFO("Loading world: " + name);
        
        // CRITICAL: Wait for all pending thread pool tasks to complete before clearing
        threadPool.wait();
        
        // Clear existing world
        chunkManager.unloadAll();
        chunkManager.clear();
        renderer.clear();
        entityManager.clear();
        
        glm::vec3 playerPos;
        long seed;
        
        if (WorldSerializer::loadWorld(name, chunkManager, playerPos, seed)) {
            camera.setPosition(playerPos);
            currentWorldName = name;
            currentSeed = seed;
            worldGenerator.setSeed(static_cast<unsigned int>(seed));
            
            // Initialize player entity at loaded position
            playerEntity = std::make_unique<PlayerEntity>(playerPos);
            
            // Initialize EntityManager for loaded world
            if (useNewEntityManager) {
                entityManager.initialize(chunkManager, worldGenerator);
                if (!entityManager.isModelPreloadComplete()) {
                    entityManager.preloadModels();
                }
            }
            
            // Initialize legacy mob spawn manager for loaded world
            mobSpawnManager = std::make_unique<MobSpawnManager>(chunkManager, worldGenerator);

            LOG_INFO("World loaded successfully");
            return true;
        } else {
            LOG_ERROR("Failed to load world");
            return false;
        }
    }
    
    void run() {
        LOG_INFO("Starting main loop");
        
        // Initialize menu background world
        initializeMenuWorld();
        
        Time::instance().reset();
        
        while (!window->shouldClose() && running) {
            Time::instance().update();
            float deltaTime = Time::instance().getDeltaTime();
            // Clamp physics/update delta to avoid large stalls causing physics explosions (teleport/new world)
            const float MAX_PHYSICS_DELTA = 0.1f; // 100 ms
            float clampedDelta = std::min(deltaTime, MAX_PHYSICS_DELTA);
            
            // Check if we're in the main menu (no world loaded)
            bool inMainMenu = !uiManager.isWorldLoaded();
            
            if (inMainMenu) {
                // Update menu camera for panoramic effect
                updateMenuCamera(clampedDelta);
            }
            
            processInput(deltaTime); // Input/GUI can use full frame delta
            update(clampedDelta); // Physics/render updates use clamped delta

            // Update Debug Info
            // Use a smoothed FPS for display to avoid 0 or flickering
            static float displayFPS = 0.0f;
            static float fpsAccumulator = 0.0f;
            static int frameAccumulator = 0;
            static float fpsUpdateTimer = 0.0f;
            
            fpsAccumulator += Time::instance().getFPS();
            frameAccumulator++;
            fpsUpdateTimer += deltaTime;
            
            if (playerEntity) {
                // Sync velocity from camera before update so animation state machine can use it
                playerEntity->setVelocity(camera.velocity);
                playerEntity->updateWithCamera(deltaTime, camera);
            }
            
            // Handle footstep sounds
            if (camera.onGround && !camera.isFlying && uiManager.isWorldLoaded()) {
                float horizontalSpeed = glm::length(glm::vec2(camera.velocity.x, camera.velocity.z));
                if (horizontalSpeed > 0.5f) {
                    footstepTimer -= deltaTime;
                    if (footstepTimer <= 0.0f) {
                        // Get block below player
                        glm::vec3 playerPos = camera.getPosition();
                        int blockX = static_cast<int>(std::floor(playerPos.x));
                        int blockY = static_cast<int>(std::floor(playerPos.y - 0.1f));
                        int blockZ = static_cast<int>(std::floor(playerPos.z));
                        Block blockBelow = chunkManager.getBlockAt(blockX, blockY, blockZ);
                        
                        // Play footstep sound based on block type
                        Audio::SoundType stepSound = Audio::getStepSoundForBlock(static_cast<uint8_t>(blockBelow.getType()));
                        Audio::AudioManager::instance().playSound(stepSound, 0.5f);
                        
                        // Adjust interval based on sprint
                        footstepInterval = camera.isSprinting ? 0.3f : 0.45f;
                        footstepTimer = footstepInterval;
                    }
                } else {
                    footstepTimer = 0.0f; // Reset timer when not moving
                }
            }
            
            if (fpsUpdateTimer >= 0.5f) {
                displayFPS = fpsAccumulator / frameAccumulator;
                fpsAccumulator = 0.0f;
                frameAccumulator = 0;
                fpsUpdateTimer = 0.0f;
            }

            std::string blockName = "None";
            
            // Increase raycast distance to ensure we hit the ground even from high up
            glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
            auto result = chunkManager.rayCast(eyePos, camera.getFront(), 100.0f);
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
            
            // Pass estimated TAA metrics to UI for debugging
            float taaMotion = 0.0f, taaHistoryWeight = 0.0f;
            if (renderer.getPostProcess()) {
                taaMotion = renderer.getPostProcess()->getLastTaaMotionMag();
                taaHistoryWeight = renderer.getPostProcess()->getLastTaaBlendEstimate();
            }
            uiManager.updateDebugInfo(displayFPS, blockName, camera.getPosition(), camera.velocity, taaMotion, taaHistoryWeight);

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
        networkManager.disconnect();
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
    Network::NetworkManager networkManager;
    
    std::mutex meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>> pendingMeshes;

    double lastX, lastY;
    double lastSpaceTime;
    bool firstMouse;
    bool running;
    
    // Game State
    std::string currentWorldName = "New World";
    long currentSeed = 12345;
    
    std::unique_ptr<PlayerEntity> playerEntity;
    
    // New EntityManager for stutter-free mob spawning
    EntityManager entityManager;
    
    // Legacy mob containers (kept for backward compatibility during transition)
    std::vector<std::unique_ptr<ZombieEntity>> zombies;
    std::vector<std::unique_ptr<SkeletonEntity>> skeletons;
    std::vector<std::unique_ptr<PigEntity>> pigs;
    std::vector<std::unique_ptr<ChickenEntity>> chickens;
    std::vector<std::unique_ptr<SheepEntity>> sheep;
    std::unique_ptr<MobSpawnManager> mobSpawnManager;
    
    // Flag to use new EntityManager vs legacy system
    bool useNewEntityManager = true;
    
    // Menu background world
    bool menuWorldInitialized = false;
    Camera menuCamera{glm::vec3(0.0f, 100.0f, 0.0f)};
    float menuCameraYaw = 0.0f;
    float menuCameraPitch = -15.0f;
    float menuCameraTimer = 0.0f;
    int menuSceneIndex = 0;
    static constexpr int NUM_MENU_SCENES = 4;
    glm::vec3 menuScenePositions[NUM_MENU_SCENES] = {
        glm::vec3(0.0f, 100.0f, 0.0f),
        glm::vec3(200.0f, 85.0f, 200.0f),
        glm::vec3(-150.0f, 110.0f, 100.0f),
        glm::vec3(100.0f, 90.0f, -200.0f)
    };
    
    // Audio - footstep timer
    float footstepTimer = 0.0f;
    float footstepInterval = 0.4f; // Time between footstep sounds
    
    // Menu world cache - O(1) lookup for chunks within session
    long menuWorldSeed = 0; // Set once at startup, reused for entire session
    bool menuSeedInitialized = false;
    std::unordered_map<ChunkPos, std::array<Block, CHUNK_VOLUME>> menuChunkCache;
    bool menuCacheValid = false; // True if cache contains valid data for current seed
    
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
                glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
                auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);
                if (result.hit) {
                    glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                    int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
                    int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
                    int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
                    
                    // Get block type before breaking for sound
                    Block block = chunkManager.getBlockAt(x, y, z);
                    uint8_t blockTypeVal = static_cast<uint8_t>(block.getType());
                    
                    // Play dig sound at block position
                    Audio::SoundType digSound = Audio::getDigSoundForBlock(blockTypeVal);
                    Audio::AudioManager::instance().playSoundAt(digSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));
                    
                    chunkManager.setBlockAt(x, y, z, Block(BlockType::AIR));
                    
                    // Broadcast block change to network
                    if (networkManager.isOnline()) {
                        networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(BlockType::AIR));
                    }
                }
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                // Place block
                glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
                auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);
                if (result.hit) {
                    glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                    int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x + result.normal.x;
                    int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y + result.normal.y;
                    int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z + result.normal.z;
                    
                    // Check intersection with any entity or player to prevent getting stuck
                    bool entityCollision = false;
                    
                    // Block AABB
                    float bx1 = static_cast<float>(x);
                    float by1 = static_cast<float>(y);
                    float bz1 = static_cast<float>(z);
                    float bx2 = bx1 + 1.0f;
                    float by2 = by1 + 1.0f;
                    float bz2 = bz1 + 1.0f;

                    // Function to check AABB overlap
                    auto checkOverlap = [&](const glm::vec3& pos, float width, float height) -> bool {
                        float ex1 = pos.x - width/2.0f;
                        float ey1 = pos.y;
                        float ez1 = pos.z - width/2.0f;
                        float ex2 = pos.x + width/2.0f;
                        float ey2 = pos.y + height;
                        float ez2 = pos.z + width/2.0f;
                        
                        return (bx1 < ex2 && bx2 > ex1) &&
                               (by1 < ey2 && by2 > ey1) &&
                               (bz1 < ez2 && bz2 > ez1);
                    };

                    // Check Player
                    if (checkOverlap(camera.getPosition(), 0.6f, 1.8f)) {
                        entityCollision = true;
                    }

                    // Check Mobs
                    if (!entityCollision) {
                         // Collect entities to check
                         std::vector<Entity*> entities;
                         if (useNewEntityManager) {
                             auto managed = entityManager.getAllEntities();
                             entities.insert(entities.end(), managed.begin(), managed.end());
                         } else {
                             for(auto& m : zombies) if(m) entities.push_back(m.get());
                             for(auto& m : skeletons) if(m && !m->isDead()) entities.push_back(m.get());
                             for(auto& m : pigs) if(m && !m->isDead()) entities.push_back(m.get());
                             for(auto& m : chickens) if(m && !m->isDead()) entities.push_back(m.get());
                             for(auto& m : sheep) if(m && !m->isDead()) entities.push_back(m.get());
                         }
                         
                         for (Entity* e : entities) {
                             // Assuming standard human/animal size for now (0.6w, 1.8h or smaller)
                             if (checkOverlap(e->getPosition(), 0.6f, 1.5f)) {
                                 entityCollision = true;
                                 break;
                             }
                         }
                    }

                    if (!entityCollision) { 
                        BlockType blockType = uiManager.getSelectedBlock();
                        chunkManager.setBlockAt(x, y, z, Block(blockType));
                        
                        // Play place sound at block position
                        Audio::SoundType placeSound = Audio::getPlaceSoundForBlock(static_cast<uint8_t>(blockType));
                        Audio::AudioManager::instance().playSoundAt(placeSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));
                        
                        // Broadcast block change to network
                        if (networkManager.isOnline()) {
                            networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(blockType));
                        }
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
        bool rightMousePressed = glfwGetMouseButton(window->getNative(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        
        if (uiManager.isMenuOpen()) {
            uiManager.update(deltaTime, uiX, uiY, mousePressed, rightMousePressed);
            firstMouse = true; // Reset mouse look when returning to game
            return;
        }

        const auto& keys = Settings::instance().keys;
        bool forward = window->isKeyPressed(keys.forward);
        bool backward = window->isKeyPressed(keys.backward);
        bool left = window->isKeyPressed(keys.left);
        bool right = window->isKeyPressed(keys.right);
        bool up = window->isKeyPressed(keys.jump);
        
        bool sprint = window->isKeyPressed(keys.sprint);
        bool sneak = window->isKeyPressed(keys.sneak);
        bool down = sneak; // Use sneak key for flying down

        camera.processInput(forward, backward, left, right, up, down, sprint, sneak, deltaTime);
        
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
        // Always update network (even in menu for connection handling)
        networkManager.update(deltaTime);
        
        // Update audio system with listener position
        Audio::AudioManager::instance().setListenerPosition(
            camera.getPosition(),
            camera.getFront(),
            camera.getUp()
        );
        Audio::AudioManager::instance().update(deltaTime);
        
        // Send local player position to network
        // Send foot position (camera is at FEET level)
        if (networkManager.isOnline() && !uiManager.isMenuOpen()) {
            glm::vec3 footPos = camera.getPosition();
            networkManager.sendLocalPlayerState(
                footPos,
                camera.getYaw(),
                camera.getPitch(),
                camera.velocity,
                camera.onGround
            );
        }
        
        // Check if we should skip player controls but continue world updates
        bool skipPlayerControls = uiManager.isMenuOpen();
        // Always continue world updates in multiplayer OR when world is loaded OR in main menu (for background)
        bool continueWorldUpdates = networkManager.isOnline() || uiManager.isWorldLoaded() || menuWorldInitialized;

        // Day/Night Cycle - ALWAYS update for sky effects (even in main menu)
        constexpr float DAY_DURATION = 2400.0f; 
        
        // Debug Controls - only when not in menu
        static bool f1Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F1)) {
            if (!f1Pressed) {
                uiManager.toggleDebug();
                f1Pressed = true;
            }
        } else {
            f1Pressed = false;
        }

        static bool f2Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F2)) {
            if (!f2Pressed) {
                uiManager.isDayNightPaused = !uiManager.isDayNightPaused;
                f2Pressed = true;
            }
        } else {
            f2Pressed = false;
        }
        
        static bool f3Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F3)) {
            if (!f3Pressed) {
                Settings::instance().enableShadows = !Settings::instance().enableShadows;
                f3Pressed = true;
            }
        } else {
            f3Pressed = false;
        }

        static bool f4Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F4)) {
            if (!f4Pressed) {
                Settings::instance().debugShowTAA = !Settings::instance().debugShowTAA;
                f4Pressed = true;
            }
        } else {
            f4Pressed = false;
        }

        static bool f8Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F8)) {
            if (!f8Pressed) {
                Settings::instance().debugNoTexture = !Settings::instance().debugNoTexture;
                f8Pressed = true;
            }
        } else {
            f8Pressed = false;
        }

        static bool f6Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F6)) {
            if (!f6Pressed) {
                Settings::instance().debugWireframe = !Settings::instance().debugWireframe;
                f6Pressed = true;
            }
        } else {
            f6Pressed = false;
        }

        static bool f7Pressed = false;
        if (!skipPlayerControls && window->isKeyPressed(GLFW_KEY_F7)) {
            if (!f7Pressed) {
                Settings::instance().debugShowNormals = !Settings::instance().debugShowNormals;
                f7Pressed = true;
            }
        } else {
            f7Pressed = false;
        }
        
        // renderer.setShowShadows(uiManager.showShadows); // Removed, Renderer uses Settings directly

        // Time control - host/offline can manually control, all players update time locally for smoothness
        bool canControlTime = !networkManager.isOnline() || networkManager.isHost();
        
        // All players update time locally for smooth progression
        if (!uiManager.isDayNightPaused) {
            uiManager.timeOfDay += deltaTime * 10.0f; // Still sped up slightly for gameplay, but slower than before
        }
        
        // Manual Time Control (only for host/offline, and not in menu)
        bool timeChanged = false;
        if (canControlTime && !skipPlayerControls) {
            if (window->isKeyPressed(GLFW_KEY_RIGHT)) {
                uiManager.timeOfDay += deltaTime * 100.0f;
                timeChanged = true;
            }
            if (window->isKeyPressed(GLFW_KEY_LEFT)) {
                uiManager.timeOfDay -= deltaTime * 100.0f;
                timeChanged = true;
            }
        }
        
        if (uiManager.timeOfDay >= DAY_DURATION) uiManager.timeOfDay -= DAY_DURATION;
        if (uiManager.timeOfDay < 0.0f) uiManager.timeOfDay += DAY_DURATION;
        
        // Sync time to clients if hosting (every 2 seconds or when manually changed)
        static float timeSyncTimer = 0.0f;
        timeSyncTimer += deltaTime;
        if (networkManager.isHost() && (timeChanged || timeSyncTimer >= 2.0f)) {
            networkManager.sendTimeSync(uiManager.timeOfDay, uiManager.isDayNightPaused);
            timeSyncTimer = 0.0f;
        }
        // Note: Clients also update time locally for smoothness, server syncs correct drift
        
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
        renderer.setSunHeight(sunY);
        renderer.setTimeOfDay(uiManager.timeOfDay);

        // Physics and player updates - only when not in menu
        if (!skipPlayerControls) {
            updatePhysics(deltaTime);
            camera.update(deltaTime);
        }
        
        // Chunk updates - always run if world is loaded or in multiplayer
        if (continueWorldUpdates) {
            chunkManager.update(camera.getPosition(), camera.getFront(), camera.getViewMatrix());
        }
        
        if (!skipPlayerControls && playerEntity) {
            playerEntity->update(deltaTime);
        }

        // === Mob Spawning & AI ===
        // Mobs can run in single-player (offline) or server mode
        // In client mode, server handles mob state
        if (!skipPlayerControls) {
            glm::vec3 playerFeet = camera.getPosition(); // Camera is at feet
            float normalizedTime = uiManager.timeOfDay / DAY_DURATION;
            
            if (useNewEntityManager) {
                // New EntityManager system - handles spawning, AI, despawning with no stutters
                bool isOfflineOrServer = (networkManager.getMode() == Network::NetworkMode::OFFLINE) || 
                                          networkManager.isHost();
                
                static bool loggedOnce = false;
                if (!loggedOnce && networkManager.isOnline()) {
                    LOG_INFO("EntityManager: isOfflineOrServer=" + std::to_string(isOfflineOrServer) + 
                             ", mode=" + std::to_string(static_cast<int>(networkManager.getMode())) +
                             ", isHost=" + std::to_string(networkManager.isHost()) +
                             ", isClient=" + std::to_string(networkManager.isClient()));
                    loggedOnce = true;
                }
                
                if (isOfflineOrServer) {
                    // Update EntityManager - returns attack events
                    auto attacks = entityManager.update(deltaTime, playerFeet, normalizedTime);
                    
                    // Apply knockback from attacks
                    for (const auto& attack : attacks) {
                        camera.velocity += attack.knockback;
                    }
                    
                    // If hosting, broadcast entity events to clients
                    if (networkManager.isHost()) {
                        // Broadcast spawn events immediately
                        auto spawnEvents = entityManager.consumeSpawnEvents();
                        for (const auto& spawn : spawnEvents) {
                            networkManager.broadcastEntitySpawn(
                                spawn.id,
                                static_cast<uint8_t>(spawn.type),
                                spawn.position,
                                spawn.yaw
                            );
                        }
                        
                        // Broadcast despawn events immediately
                        auto despawnEvents = entityManager.consumeDespawnEvents();
                        for (EntityId id : despawnEvents) {
                            networkManager.broadcastEntityDespawn(id);
                        }
                        
                        // Broadcast position updates at 10Hz
                        static float entitySyncTimer = 0.0f;
                        entitySyncTimer += deltaTime;
                        if (entitySyncTimer >= 0.1f) { // 10Hz sync rate
                            entitySyncTimer = 0.0f;
                            
                            // Get all entity states and broadcast them
                            auto entityStates = entityManager.getEntityStatesForSync();
                            for (const auto& state : entityStates) {
                                uint8_t flags = state.isDead ? 1 : 0;
                                
                                networkManager.broadcastEntityUpdate(
                                    state.id,
                                    state.position, state.velocity, state.yaw, state.health, flags
                                );
                            }
                        }
                    }
                } else if (networkManager.isClient()) {
                    // Client mode - just render, server handles AI
                    // EntityManager state comes from server sync packets
                }
            } else {
                // Legacy system - uses MobSpawnManager (can cause stutters)
                if (networkManager.getMode() == Network::NetworkMode::OFFLINE) {
                    if (mobSpawnManager && playerEntity) {
                        mobSpawnManager->update(deltaTime, playerFeet, normalizedTime,
                                               zombies, skeletons, pigs, chickens, sheep);
                    }
                    
                    // Update zombie AI
                    if (playerEntity) {
                        for (auto& z : zombies) {
                            if (!z) continue;
                            bool attacked = z->updateAI(deltaTime, chunkManager, playerFeet);
                            if (attacked) {
                                camera.velocity += z->consumeAttackImpulse();
                            }
                        }
                        
                        // Update skeleton AI
                        for (auto& s : skeletons) {
                            if (!s || s->isDead()) continue;
                            bool attacked = s->updateAI(deltaTime, chunkManager, playerFeet);
                            if (attacked) {
                                camera.velocity += s->consumeAttackImpulse();
                            }
                        }
                        
                        // Update passive mobs
                        for (auto& p : pigs) {
                            if (p && !p->isDead()) p->updateAI(deltaTime, chunkManager);
                        }
                        for (auto& c : chickens) {
                            if (c && !c->isDead()) c->updateAI(deltaTime, chunkManager);
                        }
                        for (auto& s : sheep) {
                            if (s && !s->isDead()) s->updateAI(deltaTime, chunkManager);
                        }
                    }
                }
            }
        } // End of mob update code
        
        // Generate chunks - always run if world is loaded
        if (continueWorldUpdates) {
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
        } // End of continueWorldUpdates block
    }
    
    void render() {
        // Check if we're in main menu without a world loaded
        bool inMainMenu = !uiManager.isWorldLoaded() && 
                          (uiManager.getMenuState() == MenuState::MAIN_MENU ||
                           uiManager.getMenuState() == MenuState::MULTIPLAYER ||
                           uiManager.getMenuState() == MenuState::HOST_GAME ||
                           uiManager.getMenuState() == MenuState::JOIN_GAME ||
                           uiManager.getMenuState() == MenuState::NEW_GAME ||
                           uiManager.getMenuState() == MenuState::LOAD_GAME ||
                           uiManager.getMenuState() == MenuState::SETTINGS ||
                           uiManager.getMenuState() == MenuState::VIDEO_SETTINGS ||
                           uiManager.getMenuState() == MenuState::PLAYER_SETTINGS ||
                           uiManager.getMenuState() == MenuState::CONTROLS ||
                           uiManager.getMenuState() == MenuState::ABOUT);
        
        // Hide crosshair when in any menu
        renderer.setShowCrosshair(!uiManager.isMenuOpen());
        
        if (inMainMenu) {
            // Render menu background world
            std::vector<Entity*> emptyEntities;
            renderer.render(chunkManager, menuCamera, emptyEntities, window->getWidth(), window->getHeight());
            renderer.cleanUnusedMeshes(chunkManager);
            uiManager.render();
            return;
        }
        
        std::vector<Entity*> entities;
        
        // Sync player entity if it exists
        if (playerEntity) {
            // Visual sync: We want the model to be exactly where the player is.
            // Camera position is FEET pos. 
            // Model origin is usually at feet. 
            // So we use camera position directly.
            glm::vec3 footPos = camera.getPosition();
            playerEntity->setPosition(footPos);
            
            // Yaw: Camera yaw 0 is safe, but GLTF might need rotation. 
            // If camera looks -Z (yaw -90), model should face -Z.
            // glTF default facing is usually +Z. So rotate 180?
            // Let's try matching camera yaw + 180 or 90. 
            // Camera::yaw is in degrees.
            playerEntity->setRotation(glm::vec3(0.0f, -camera.getYaw() + 90.0f, 0.0f));
            playerEntity->setVelocity(camera.velocity);

            // Only render if we are in third person OR we want to see body parts (requires careful culling)
            // User requested toggle.
            if (camera.isThirdPerson()) {
                entities.push_back(playerEntity.get());
            }
        }

        // Get entities from EntityManager (new system) or legacy containers
        if (useNewEntityManager) {
            auto managedEntities = entityManager.getAllEntities();
            entities.insert(entities.end(), managedEntities.begin(), managedEntities.end());
        } else {
            // Legacy: render from individual containers
            for (auto& z : zombies) {
                if (z) entities.push_back(z.get());
            }
            
            for (auto& s : skeletons) {
                if (s && !s->isDead()) entities.push_back(s.get());
            }
            
            for (auto& p : pigs) {
                if (p && !p->isDead()) entities.push_back(p.get());
            }
            for (auto& c : chickens) {
                if (c && !c->isDead()) entities.push_back(c.get());
            }
            for (auto& s : sheep) {
                if (s && !s->isDead()) entities.push_back(s.get());
            }
        }
        
        // Render remote players from network
        auto remotePlayerEntities = networkManager.getRemotePlayerEntities();
        for (auto* remotePlayer : remotePlayerEntities) {
            entities.push_back(remotePlayer);
        }
        
        renderer.render(chunkManager, camera, entities, window->getWidth(), window->getHeight());
        // Clean up any GPU meshes for chunks that have been unloaded by ChunkManager
        renderer.cleanUnusedMeshes(chunkManager);
        
        uiManager.render();
    }
    
    void updatePhysics(float deltaTime) {
        if (camera.getFlightMode()) return;
        
        // Check if in water
        bool inWater = false;
        glm::vec3 camPos = camera.getPosition(); // Use feet position
        
        // Check eye level (Feet + 1.6) and feet level (Feet)
        Block headBlock = chunkManager.getBlockAt(static_cast<int>(floor(camPos.x)), static_cast<int>(floor(camPos.y + 1.6f)), static_cast<int>(floor(camPos.z)));
        Block feetBlock = chunkManager.getBlockAt(static_cast<int>(floor(camPos.x)), static_cast<int>(floor(camPos.y)), static_cast<int>(floor(camPos.z)));
        
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
            // Minecraft gravity is roughly 32 m/s^2
            camera.velocity.y -= 32.0f * deltaTime;
            
            // Terminal velocity
            camera.velocity.y = std::max(-78.4f, camera.velocity.y);
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
        
        // Friction is now handled in Camera::update() for better control
        // float friction = camera.onGround ? 10.0f : 2.0f;
        // ...
    }
    
    bool checkCollision(const glm::vec3& pos) {
        // Player Bounding Box relative to feet position
        // Width: 0.6m (-0.3 to +0.3)
        // Height: 1.8m (0.0 to 1.8)
        
        float minX = pos.x - 0.3f;
        float maxX = pos.x + 0.3f;
        float minY = pos.y;         // Feet level
        float maxY = pos.y + 1.8f;  // Head level
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
