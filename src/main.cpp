#define NOMINMAX
#include "Core/Window.h"
#include "Core/Time.h"
#include "Core/Logger.h"
#include "Core/ThreadPool.h"
#include "Core/Settings.h"
#include "Core/HardwareInfo.h"
#include "Render/Renderer.h"
#include "Render/Camera.h"
#include "Render/HeldItemRenderer.h"
#include "World/ChunkManager.h"
#include "World/WorldGenerator.h"
#include "World/Item.h"
#include "Mesh/MeshBuilder.h"
#include "Util/Config.h"
#include "UI/UIManager.h"
#include "UI/Console.h"
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
#include "Physics/PhysicsTest.h"
#include "Render/ExplosionVolumeSystem.h"
#include "World/FireSystem.h"
#include "Game/StatusEffectsSystem.h"
#include "Game/PlayerHealthSystem.h"
#include "Game/MenuWorldSystem.h"
#include "Game/WorldLifecycleSystem.h"
#include "Game/InputSystem.h"
#include "Game/ChunkUpdateSystem.h"
#include "Game/TimeOfDaySystem.h"
#include "Game/PlayerPhysicsSystem.h"
#include "Game/DebugInputSystem.h"
#include "Game/MobUpdateSystem.h"
#include "Game/RenderPipelineSystem.h"
#include "Game/AudioSystem.h"

#include <memory>
#include <iostream>
#include <mutex>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <random>

class Application {
public:
    Application() 
        : camera(glm::vec3(0.0f, 80.0f, 0.0f)),
          threadPool(HardwareInfo::getOptimalThreadCount()),
          playerHealthSystem(uiManager, camera),
          menuWorldSystem(chunkManager, worldGenerator, meshBuilder, renderer, threadPool, meshMutex, pendingMeshes),
          statusEffectsSystem(chunkManager, fireSystem, explosionVolumes, camera),
                    worldLifecycleSystem(renderer, chunkManager, worldGenerator, meshBuilder, threadPool, meshMutex, pendingMeshes,
                                                             uiManager, entityManager, networkManager, playerHealthSystem, explosionVolumes, fireSystem,
                                                             camera, playerEntity, mobSpawnManager, zombies, skeletons, pigs, chickens, sheep,
                                                             useNewEntityManager, physicsTest, currentWorldName, currentSeed),
                    inputSystem(uiManager, camera, chunkManager, heldItemRenderer, entityManager, networkManager,
                                            playerEntity, zombies, skeletons, pigs, chickens, sheep, useNewEntityManager,
                                            attackCooldown, isBreakingBlock, blockBreakProgress, breakingBlockPos, breakingBlockType, isUnderwater),
                      chunkUpdateSystem(chunkManager, worldGenerator, meshBuilder, renderer, threadPool, meshMutex, pendingMeshes),
                      timeOfDaySystem(uiManager, renderer, networkManager, camera),
                                            playerPhysicsSystem(camera, chunkManager),
                      debugInputSystem(uiManager),
                    mobUpdateSystem(camera, chunkManager, entityManager, statusEffectsSystem, networkManager,
                                                    mobSpawnManager, playerEntity, zombies, skeletons, pigs, chickens, sheep, useNewEntityManager),
                                        renderPipelineSystem(renderer, chunkManager, menuWorldSystem, camera, entityManager,
                                                                                 worldGenerator, heldItemRenderer, fireSystem, explosionVolumes,
                                                                                 uiManager, networkManager, physicsTest, playerEntity,
                                                                                 zombies, skeletons, pigs, chickens, sheep, useNewEntityManager),
                    lastSpaceTime(0.0),
                    running(true),
                    audioSystem(uiManager, camera, chunkManager, isUnderwater, wasUnderwater) {
                std::random_device rd;
                rng.seed(rd());
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

        inputSystem.setWindow(window.get());
        renderPipelineSystem.setWindow(window.get());
        
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
            // Handle console mouse input first
            if (Console::instance().isVisible()) {
                double mx, my;
                glfwGetCursorPos(window->getNative(), &mx, &my);
                Console::instance().handleMouseButton(button, action, mx, my, window->getHeight());
                // Don't return - still allow game to process if needed
            }
            inputSystem.handleMouseButton(button, action, mods);
        });
        
        window->setCursorPosCallback([this](double xpos, double ypos) {
            // Handle console mouse move for text selection
            if (Console::instance().isVisible() && Console::instance().isSelecting()) {
                Console::instance().handleMouseMove(xpos, ypos, window->getHeight());
            }
            // Normal mouse movement handled elsewhere
        });
        
        window->setScrollCallback([this](double xoffset, double yoffset) {
            // Handle console scroll
            if (Console::instance().isVisible()) {
                Console::instance().handleScroll(yoffset);
                return;
            }
            // Default scroll - hotbar
            if (!uiManager.isMenuOpen()) {
                int slot = uiManager.selectedSlot;
                if (yoffset > 0) {
                    slot = (slot - 1 + 9) % 9;
                } else if (yoffset < 0) {
                    slot = (slot + 1) % 9;
                }
                uiManager.selectHotbarSlot(slot);
            }
        });
        
        window->setKeyCallback([this](int key, int scancode, int action, int mods) {
            // Console toggle with ` or ~ (grave accent key)
            if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS) {
                Console::instance().toggle();
                return;
            }
            
            // If console is open, route input to it
            if (Console::instance().isVisible()) {
                Console::instance().handleKeyInput(key, action, mods);
                return;
            }
            
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                uiManager.handleKeyInput(key);
            }

            if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
                double currentTime = glfwGetTime();
                if (currentTime - lastSpaceTime < 0.3) {
                    // Double-space toggles between creative and survival mode
                    uiManager.isCreativeMode = !uiManager.isCreativeMode;
                    
                    if (uiManager.isCreativeMode) {
                        // Entering creative mode - enable flight
                        if (!camera.getFlightMode()) {
                            camera.toggleFlightMode();
                        }
                        LOG_INFO("Switched to Creative Mode");
                    } else {
                        // Entering survival mode - disable flight
                        if (camera.getFlightMode()) {
                            camera.toggleFlightMode();
                        }
                        LOG_INFO("Switched to Survival Mode");
                    }
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
                
                // Emote keys - G (wave), Y (yes), B (no)
                if (key == GLFW_KEY_G && uiManager.getMenuState() == MenuState::NONE && playerEntity) {
                    playerEntity->playWaveAnimation();
                    if (networkManager.isOnline()) {
                        networkManager.sendPlayerAnimation(Network::PlayerAnimationPacket::ANIM_WAVE);
                    }
                }
                if (key == GLFW_KEY_Y && uiManager.getMenuState() == MenuState::NONE && playerEntity) {
                    playerEntity->playYesAnimation();
                    if (networkManager.isOnline()) {
                        networkManager.sendPlayerAnimation(Network::PlayerAnimationPacket::ANIM_YES);
                    }
                }
                if (key == GLFW_KEY_B && uiManager.getMenuState() == MenuState::NONE && playerEntity) {
                    playerEntity->playNoAnimation();
                    if (networkManager.isOnline()) {
                        networkManager.sendPlayerAnimation(Network::PlayerAnimationPacket::ANIM_NO);
                    }
                }

                if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                    uiManager.selectHotbarSlot(key - GLFW_KEY_1);
                }
            }
        });
        
        window->setCharCallback([this](unsigned int codepoint) {
            // Don't handle backtick character (it toggles the console)
            if (codepoint == '`' || codepoint == '~') return;
            
            // If console is open, route input to it
            if (Console::instance().isVisible()) {
                Console::instance().handleCharInput(codepoint);
                return;
            }
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
        
        // Initialize held item renderer
        if (!heldItemRenderer.initialize()) {
            LOG_WARNING("Failed to initialize held item renderer");
        }
        
        // Apply initial settings
        window->setVSync(Settings::instance().vsync);
        window->setFullscreen(Settings::instance().fullscreen);
        
        // Get actual framebuffer size for initialization
        int fbW, fbH;
        glfwGetFramebufferSize(window->getNative(), &fbW, &fbH);
        uiManager.initialize(fbW, fbH);
        // Pick a main menu tip for this session
        mainMenuTip = pickRandomTip();
        uiManager.setMainMenuTip(mainMenuTip);
        worldLifecycleSystem.setShouldCloseCallback([this]() {
            return window->shouldClose();
        });
        worldLifecycleSystem.setLoadingCallback([this](float progress) {
            renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
            updateLoadingTip();
            uiManager.renderLoadingTip(currentLoadingTip);
            window->swapBuffers();
            window->pollEvents();
        });
        playerHealthSystem.setOnDeathCallback([this]() {
            onPlayerDeath();
        });
        
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
        
        uiManager.setOnRespawn([this]() {
            // Kill player first if coming from menu, then respawn
            if (playerHealthSystem.getHealth() > 0.0f) {
                playerHealthSystem.setHealthSilent(0.0f);
            }
            respawnPlayer();
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
            
            menuWorldSystem.invalidate(); // Force reinitialization
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
            menuWorldSystem.invalidate();
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
        
        // Player damage callback - handle incoming damage from other players
        networkManager.setPlayerDamageCallback([this](uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback) {
            uint32_t localPlayerId = networkManager.getLocalPlayerId();
            
            // Check if we are the target
            if (targetId == localPlayerId) {
                // We received damage from another player
                LOG_INFO("Received " + std::to_string(damage) + " damage from player " + std::to_string(attackerId));
                playerHealthSystem.takeDamage(damage, knockback);
            }
            
            // Find the remote player entity and apply damage for visual effects
            auto remotePlayers = networkManager.getRemotePlayerEntities();
            for (auto* rp : remotePlayers) {
                if (rp->getPlayerId() == targetId) {
                    // Apply damage to remote player (tracks health and plays hit/death animation)
                    rp->applyNetworkDamage(damage, knockback);
                    break;
                }
            }
        });
        
        audioSystem.initialize();
        
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

    bool isDayTime(float normalizedTime) const {
        float angle = normalizedTime * 6.28318530718f;
        float sunY = std::sin(angle);
        return sunY > 0.1f;
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
        auto loadingCallback = [this](float progress) {
            renderer.renderLoadingScreen(window->getWidth(), window->getHeight(), progress);
            updateLoadingTip();
            uiManager.renderLoadingTip(currentLoadingTip);
            window->swapBuffers();
            window->pollEvents();
        };

        menuWorldSystem.initialize(loadingCallback);
    }

    void createWorld(const std::string& name, long seed = 12345) {
        worldLifecycleSystem.createWorld(name, seed);
    }
    
    bool loadWorld(const std::string& name = "world.dat") {
        return worldLifecycleSystem.loadWorld(name);
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
                menuWorldSystem.update(clampedDelta);
            }
            
            inputSystem.processInput(deltaTime); // Input/GUI can use full frame delta
            update(clampedDelta); // Physics/render updates use clamped delta
            audioSystem.update(deltaTime);

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
                
                blockName = uiManager.getBlockName(block.getType());
            }
            
            // Pass estimated TAA metrics to UI for debugging
            float taaMotion = 0.0f, taaHistoryWeight = 0.0f;
            if (renderer.getPostProcess()) {
                taaMotion = renderer.getPostProcess()->getLastTaaMotionMag();
                taaHistoryWeight = renderer.getPostProcess()->getLastTaaBlendEstimate();
            }
            uiManager.updateDebugInfo(displayFPS, blockName, camera.getPosition(), camera.velocity, taaMotion, taaHistoryWeight);

            renderPipelineSystem.renderFrame(isBreakingBlock, blockBreakProgress, breakingBlockPos);
            
            window->pollEvents();
            window->swapBuffers();
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
    PlayerHealthSystem playerHealthSystem;
    WorldSerializer worldSerializer;
    Network::NetworkManager networkManager;
    HeldItemRenderer heldItemRenderer;
    std::mutex meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>> pendingMeshes;
    MenuWorldSystem menuWorldSystem;
    ExplosionVolumeSystem explosionVolumes;
    FireSystem fireSystem;
    StatusEffectsSystem statusEffectsSystem;
    InputSystem inputSystem;
    ChunkUpdateSystem chunkUpdateSystem;
    TimeOfDaySystem timeOfDaySystem;
    PlayerPhysicsSystem playerPhysicsSystem;
    DebugInputSystem debugInputSystem;
    MobUpdateSystem mobUpdateSystem;
    RenderPipelineSystem renderPipelineSystem;
    
    // Physics test system (can be disabled via ENABLE_PHYSICS_TEST in PhysicsTest.h)
    Physics::PhysicsTestSystem physicsTest;
    
    double lastSpaceTime;
    bool running;
    
    // Game State
    std::string currentWorldName = "New World";
    long currentSeed = 12345;
    
    // Combat state
    float attackCooldown = 0.0f;
    float lastAttackTime = 0.0f;
    
    // Block breaking state (for survival mode)
    bool isBreakingBlock = false;
    float blockBreakProgress = 0.0f;
    glm::ivec3 breakingBlockPos = glm::ivec3(0);
    BlockType breakingBlockType = BlockType::AIR;
    
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

    WorldLifecycleSystem worldLifecycleSystem;

    
    bool isUnderwater = false;
    bool wasUnderwater = false;
    bool wasOnGround = true; // For detecting jump
    AudioSystem audioSystem;

    // Loading tips
    std::vector<std::string> loadingTips = {
        "Blaming the compiler just in case.",
        "If this freezes, it's probably intentional.",
        "Optimized enough to still be slow.",
        "Compiling pixels by hand.",
        "Please wait while we overthink everything.",
        "Allocating responsibly.",
        "No frameworks were harmed in the making of this game.",
        "This would be faster in C. Oh wait.",
        "Turning coffee into code.",
        "Debug mode is a lifestyle choice.",
        "Counting CPU cycles.",
        "This game knows what the cache is.",
        "Multithreading: what could go wrong?",
        "Yes, this was written from scratch.",
        "Avoiding unnecessary abstractions.",
        "If it crashes, it's a feature.",
        "Optimized until readability cried.",
        "Trust the math. Fear the bugs.",
        "This engine dislikes magic.",
        "Heap allocations are being judged.",
        "Probably rebuilding something that worked.",
        "Because premature optimization is still optimization.",
        "This loader is faster than the menu.",
        "One more micro-optimization, surely.",
        "Running on pure determination and undefined behavior.",
        "Please wait while we reinvent the wheel.",
        "Garbage collection? Never heard of her.",
        "This game respects your CPU. Mostly.",
        "Manually.",
        "Somewhere, a profiler is smiling.",
        "Debugging is just reverse engineering your own code.",
        "Praying to the memory gods.",
        "Built with care and mild frustration.",
        "This engine was tested on real hardware. Regret followed.",
        "No engines inside the engine.",
        "It worked yesterday.",
        "Undefined behavior detected. Ignoring.",
        "This project hates unnecessary work. Unlike this loader.",
        "Every abstraction here pays rent.",
        "Because computers are complicated.",
        "Powered by caffeine and stubbornness.",
        "The dev chose control over sanity.",
        "Yes, this could be simpler. No, it won't be.",
        "Still faster than Java.",
        "Math is correct. Results may vary.",
        "This game was optimized before it was fun.",
        "Trust the process. Question everything else.",
        "Building character.",
        "Engine started as a \"small experiment\".",
        "Almost ready. Probably."
    };
    std::string currentLoadingTip;
    std::string mainMenuTip;
    double nextLoadingTipSwitchTime = 0.0;

    std::mt19937 rng;
    
    // Menu world cache - O(1) lookup for chunks within session

    std::string pickRandomTip() {
        if (loadingTips.empty()) return std::string();
        std::uniform_int_distribution<size_t> dist(0, loadingTips.size() - 1);
        return loadingTips[dist(rng)];
    }

    void updateLoadingTip() {
        double now = glfwGetTime();
        if (currentLoadingTip.empty() || now >= nextLoadingTipSwitchTime) {
            currentLoadingTip = pickRandomTip();
            std::uniform_real_distribution<double> dist(2.5, 4.5);
            nextLoadingTipSwitchTime = now + dist(rng);
        }
    }
    
    void applySettings() {
        auto& s = Settings::instance();
        camera.setFov(s.fov);
        camera.setSensitivity(s.mouseSensitivity);
        window->setVSync(s.vsync);
        window->setFullscreen(s.fullscreen);
        // Render distance is handled in ChunkManager::update
        // AO and Gamma are handled in Renderer::render
    }

    
    void update(float deltaTime) {
        // Always update network (even in menu for connection handling)
        networkManager.update(deltaTime);
        
        // Update attack cooldown
        if (attackCooldown > 0.0f) {
            attackCooldown -= deltaTime;
            if (attackCooldown < 0.0f) attackCooldown = 0.0f;
        }

        playerHealthSystem.update(deltaTime);
        
        // Update held item from current hotbar selection
        ItemType currentHeldItem = uiManager.getSelectedItem();
        heldItemRenderer.setHeldItem(currentHeldItem);
        
        // Sync held item to playerEntity for animation purposes
        if (playerEntity) {
            playerEntity->setHeldItem(currentHeldItem);
        }
        
        // Update held item renderer
        bool isMoving = glm::length(glm::vec2(camera.velocity.x, camera.velocity.z)) > 0.5f;
        heldItemRenderer.update(deltaTime, camera.velocity, camera.onGround, isMoving);
        
        // Handle survival mode block breaking
        if (isBreakingBlock && !uiManager.isCreativeMode && uiManager.isWorldLoaded()) {
            glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
            auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);
            
            if (result.hit) {
                glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
                int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
                int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
                
                // Check if target block changed
                if (breakingBlockPos != glm::ivec3(x, y, z)) {
                    breakingBlockPos = glm::ivec3(x, y, z);
                    blockBreakProgress = 0.0f;
                    breakingBlockType = chunkManager.getBlockAt(x, y, z).getType();
                }
                
                // Calculate break speed based on tool
                float baseBreakTime = getBlockBreakTime(breakingBlockType);
                float toolMultiplier = ItemRegistry::instance().getMiningMultiplier(currentHeldItem, breakingBlockType);
                float effectiveBreakTime = baseBreakTime / toolMultiplier;
                
                // Progress breaking
                blockBreakProgress += deltaTime / effectiveBreakTime;
                
                // Trigger mining animation
                heldItemRenderer.setMining(true);
                
                // Play dig hit sound periodically
                static float digSoundTimer = 0.0f;
                digSoundTimer += deltaTime;
                if (digSoundTimer >= 0.25f) {
                    Audio::SoundType digHitSound = Audio::getDigSoundForBlock(static_cast<uint8_t>(breakingBlockType));
                    Audio::AudioManager::instance().playSoundAt(digHitSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 0.3f);
                    digSoundTimer = 0.0f;
                }
                
                // Check if block is broken
                if (blockBreakProgress >= 1.0f) {
                    // Break the block
                    Audio::SoundType digSound = Audio::getDigSoundForBlock(static_cast<uint8_t>(breakingBlockType));
                    Audio::AudioManager::instance().playSoundAt(digSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));
                    
                    // Spawn dropped item debris before removing block
                    #if ENABLE_PHYSICS_TEST
                    physicsTest.spawnDroppedItem(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), breakingBlockType);
                    #endif
                    
                    chunkManager.setBlockAt(x, y, z, Block(BlockType::AIR));
                    
                    // Broadcast block change to network
                    if (networkManager.isOnline()) {
                        networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(BlockType::AIR));
                    }
                    
                    blockBreakProgress = 0.0f;
                }
            } else {
                // No block in range, reset progress
                blockBreakProgress = 0.0f;
                heldItemRenderer.setMining(false);
            }
        } else {
            heldItemRenderer.setMining(false);
        }
        
        // Send local player position to network
        // Send foot position (camera is at FEET level)
        if (networkManager.isOnline() && !uiManager.isMenuOpen()) {
            glm::vec3 footPos = camera.getPosition();
            uint8_t heldItemId = static_cast<uint8_t>(uiManager.getSelectedItem());
            networkManager.sendLocalPlayerState(
                footPos,
                camera.getYaw(),
                camera.getPitch(),
                camera.velocity,
                camera.onGround,
                heldItemId
            );
        }
        
        // Check if we should skip player controls but continue world updates
        bool skipPlayerControls = uiManager.isMenuOpen() || Console::instance().isVisible();
        // Always continue world updates in multiplayer OR when world is loaded OR in main menu (for background)
        bool continueWorldUpdates = networkManager.isOnline() || uiManager.isWorldLoaded() || menuWorldSystem.isInitialized();

        // Day/Night Cycle - ALWAYS update for sky effects (even in main menu)
        
        // Debug Controls - only when not in menu
        debugInputSystem.update(skipPlayerControls, *window);
        
        // Physics Test System - Isolated testing controls (X, C, V, P keys)
#if ENABLE_PHYSICS_TEST
        if (!skipPlayerControls && uiManager.isWorldLoaded()) {
            physicsTest.handleInput(window->getNative(), camera, deltaTime);
            physicsTest.update(deltaTime);
        }
#endif
        if (uiManager.isWorldLoaded()) {
            explosionVolumes.update(deltaTime);
            fireSystem.update(deltaTime, chunkManager, &explosionVolumes);
        }
        
        // renderer.setShowShadows(uiManager.showShadows); // Removed, Renderer uses Settings directly

        timeOfDaySystem.update(deltaTime, skipPlayerControls, *window);

        // Physics and player updates - only when not in menu
        if (!skipPlayerControls) {
            playerPhysicsSystem.update(deltaTime, *window);
            camera.update(deltaTime);
        }
        
        // Chunk updates - always run if world is loaded or in multiplayer
        if (continueWorldUpdates) {
            chunkUpdateSystem.updateWorldChunks(
                camera.getPosition(),
                camera.getFront(),
                camera.getViewMatrix(),
                Settings::instance().renderDistance,
                10,
                MAX_MESHES_PER_FRAME
            );
        }
        
        // === Mob Spawning & AI ===
        // Mobs can run in single-player (offline) or server mode
        // In client mode, server handles mob state
        float normalizedTime = uiManager.timeOfDay / TimeOfDaySystem::kDayDuration;
        mobUpdateSystem.update(deltaTime, normalizedTime, skipPlayerControls);

        if (!skipPlayerControls) {
            statusEffectsSystem.applyPlayerFireBurn(
                deltaTime,
                playerHealthSystem.getHealth(),
                isUnderwater,
                [this](float amount, const glm::vec3& knockback, bool playHurtSound) {
                    playerHealthSystem.takeDamage(amount, knockback, playHurtSound);
                }
            );
            statusEffectsSystem.applyPlayerFallDamage(
                playerHealthSystem.getHealth(),
                isUnderwater,
                camera.isFlying,
                uiManager.isCreativeMode,
                [this](float amount, const glm::vec3& knockback, bool playHurtSound) {
                    playerHealthSystem.takeDamage(amount, knockback, playHurtSound);
                }
            );
        }
    }
    
    
    // Get base break time for a block type (in seconds, with bare hands)
    float getBlockBreakTime(BlockType type) const {
        switch (type) {
            // Instant break
            case BlockType::AIR:
            case BlockType::TALL_GRASS:
            case BlockType::ROSE:
                return 0.0f;
            
            // Very soft
            case BlockType::LEAVES:
                return 0.35f;
            
            // Soft blocks (shovel effective)
            case BlockType::DIRT:
            case BlockType::GRASS:
            case BlockType::SAND:
            case BlockType::GRAVEL:
            case BlockType::SNOW:
                return 0.75f;
            
            // Wood (axe effective)
            case BlockType::WOOD:
            case BlockType::LOG:
                return 3.0f;
            
            // Stone (pickaxe effective)
            case BlockType::STONE:
            case BlockType::SANDSTONE:
                return 7.5f;
            
            // Ice (pickaxe effective, but breaks easily)
            case BlockType::ICE:
                return 0.7f;
            
            // Water (can't break)
            case BlockType::WATER:
                return 100000.0f;
            
            // Bedrock (unbreakable)
            case BlockType::BEDROCK:
                return 100000.0f;
            
            default:
                return 1.5f;
        }
    }
    
    void onPlayerDeath() {
        LOG_INFO("Player died!");
        
        // Trigger death animation on third person model
        if (playerEntity) {
            playerEntity->playDeathAnimation();
        }
        
        // Show death screen
        uiManager.setMenuState(MenuState::DEATH_SCREEN);
        window->setCursorMode(GLFW_CURSOR_NORMAL);
    }
    
    void respawnPlayer() {
        // Reset health
        playerHealthSystem.resetToMax();
        
        // Reset player entity death state so animation plays correctly
        if (playerEntity) {
            playerEntity->resetDeathState();
        }
        
        // Respawn at world spawn
        int spawnX = 0;
        int spawnZ = 0;
        int terrainHeight = worldGenerator.getSurfaceHeight(spawnX, spawnZ);
        camera.setPosition(glm::vec3(spawnX, terrainHeight + 2.0f, spawnZ));
        camera.velocity = glm::vec3(0.0f);
        
        LOG_INFO("Player respawned");
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
