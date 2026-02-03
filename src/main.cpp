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
                    lastSpaceTime(0.0),
                    running(true) {
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
        
        // Initialize audio system
        if (!Audio::AudioManager::instance().initialize()) {
            LOG_WARNING("Failed to initialize audio system - continuing without sound");
        } else {
            Audio::AudioManager::instance().loadAllSounds();
            // Set initial volume from settings
            Audio::AudioManager::instance().setMasterVolume(Settings::instance().masterVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, Settings::instance().musicVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, Settings::instance().ambientVolume);
            // soundVolume controls BLOCKS, MOBS, PLAYER, and UI categories
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, Settings::instance().soundVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, Settings::instance().soundVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, Settings::instance().soundVolume);
            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, Settings::instance().soundVolume);
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
            if (camera.onGround && !camera.isFlying && uiManager.isWorldLoaded() && !isUnderwater) {
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

            // Handle swim sounds
            if (uiManager.isWorldLoaded()) {
                if (isUnderwater && !wasUnderwater) {
                    Audio::AudioManager::instance().playSound(Audio::SoundType::WATER_SPLASH, 0.6f);
                    swimTimer = 0.0f;
                }

                float swimSpeed = glm::length(glm::vec2(camera.velocity.x, camera.velocity.z));
                if (isUnderwater && swimSpeed > 0.2f) {
                    swimTimer -= deltaTime;
                    if (swimTimer <= 0.0f) {
                        Audio::AudioManager::instance().playSound(Audio::SoundType::WATER_SWIM, 0.5f);
                        swimInterval = camera.isSprinting ? 0.45f : 0.7f;
                        swimTimer = swimInterval;
                    }
                } else if (!isUnderwater) {
                    swimTimer = 0.0f;
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
                
                blockName = uiManager.getBlockName(block.getType());
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

    
    // Audio - footstep timer
    float footstepTimer = 0.0f;
    float footstepInterval = 0.4f; // Time between footstep sounds
    float swimTimer = 0.0f;
    float swimInterval = 0.6f;
    bool isUnderwater = false;
    bool wasUnderwater = false;
    bool wasOnGround = true; // For detecting jump

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
        
        // Update ambient audio state based on player position
        if (uiManager.isWorldLoaded()) {
            glm::vec3 headPos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
            int hx = static_cast<int>(std::floor(headPos.x));
            int hy = static_cast<int>(std::floor(headPos.y));
            int hz = static_cast<int>(std::floor(headPos.z));

            Block headBlock = chunkManager.getBlockAt(hx, hy, hz);
            bool underwater = headBlock.getType() == BlockType::WATER;
            wasUnderwater = isUnderwater;
            isUnderwater = underwater;

            Block ceilingBlock = chunkManager.getBlockAt(hx, hy + 2, hz);
            bool inCave = ceilingBlock.getType() != BlockType::AIR && ceilingBlock.getType() != BlockType::WATER;

            Audio::AudioManager::instance().setUnderwater(underwater);
            Audio::AudioManager::instance().setInCave(inCave);
        } else {
            wasUnderwater = isUnderwater;
            isUnderwater = false;
            Audio::AudioManager::instance().setUnderwater(false);
            Audio::AudioManager::instance().setInCave(false);
        }
        
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
        
        // NOTE: playerEntity->updateWithCamera() is called earlier in the main loop (around line 1088)
        // which already calls Entity::update() internally. Do NOT call update() again here
        // as it would double-update the animation system.

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

                    auto entities = entityManager.getAllEntities();
                    if (isDayTime(normalizedTime)) {
                        for (auto* entity : entities) {
                            if (dynamic_cast<ZombieEntity*>(entity) || dynamic_cast<SkeletonEntity*>(entity)) {
                                statusEffectsSystem.applyDaylightBurn(entity, deltaTime);
                            }
                        }
                    } else {
                        statusEffectsSystem.clearDaylightBurnStates();
                    }

                    for (auto* entity : entities) {
                        statusEffectsSystem.applyFireBurn(entity, deltaTime);
                        statusEffectsSystem.applyEntityFallDamage(entity);
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
                            if (attacked && !z->isDead()) {
                                camera.velocity += z->consumeAttackImpulse();
                            }
                        }
                        
                        // Update skeleton AI
                        for (auto& s : skeletons) {
                            if (!s) continue;
                            bool attacked = s->updateAI(deltaTime, chunkManager, playerFeet);
                            if (attacked && !s->isDead()) {
                                camera.velocity += s->consumeAttackImpulse();
                            }
                        }

                        if (isDayTime(normalizedTime)) {
                            for (auto& z : zombies) {
                                if (z) statusEffectsSystem.applyDaylightBurn(z.get(), deltaTime);
                            }
                            for (auto& s : skeletons) {
                                if (s) statusEffectsSystem.applyDaylightBurn(s.get(), deltaTime);
                            }
                        } else {
                            statusEffectsSystem.clearDaylightBurnStates();
                        }
                        
                        // Update passive mobs (always update for death timer)
                        for (auto& p : pigs) {
                            if (p) p->updateAI(deltaTime, chunkManager);
                        }
                        for (auto& c : chickens) {
                            if (c) c->updateAI(deltaTime, chunkManager);
                        }
                        for (auto& s : sheep) {
                            if (s) s->updateAI(deltaTime, chunkManager);
                        }

                        for (auto& z : zombies) { if (z) { statusEffectsSystem.applyFireBurn(z.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(z.get()); } }
                        for (auto& s : skeletons) { if (s) { statusEffectsSystem.applyFireBurn(s.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(s.get()); } }
                        for (auto& p : pigs) { if (p) { statusEffectsSystem.applyFireBurn(p.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(p.get()); } }
                        for (auto& c : chickens) { if (c) { statusEffectsSystem.applyFireBurn(c.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(c.get()); } }
                        for (auto& s : sheep) { if (s) { statusEffectsSystem.applyFireBurn(s.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(s.get()); } }
                    }
                }
            }
        } // End of mob update code

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
            renderer.setFireLightPositions({});
            renderer.render(chunkManager, menuWorldSystem.getCamera(), emptyEntities, window->getWidth(), window->getHeight(), {}, nullptr);
            renderer.cleanUnusedMeshes(chunkManager);
            uiManager.render();
            uiManager.renderConsole();
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
            // Legacy: render from individual containers (include dead mobs for death fade)
            for (auto& z : zombies) {
                if (z) entities.push_back(z.get());
            }
            
            for (auto& s : skeletons) {
                if (s) entities.push_back(s.get());
            }
            
            for (auto& p : pigs) {
                if (p) entities.push_back(p.get());
            }
            for (auto& c : chickens) {
                if (c) entities.push_back(c.get());
            }
            for (auto& s : sheep) {
                if (s) entities.push_back(s.get());
            }
        }
        
        // Render remote players from network
        auto remotePlayerEntities = networkManager.getRemotePlayerEntities();
        for (auto* remotePlayer : remotePlayerEntities) {
            entities.push_back(remotePlayer);
        }
        
        // Update biome colors based on camera/player position
        {
            glm::vec3 camPos = camera.getPosition();
            BiomeType currentBiome = worldGenerator.getBiome(camPos.x, camPos.z);
            BiomeInfo biomeInfo = worldGenerator.getBiomeInfo(currentBiome);
            
            renderer.setBiomeGrassColor(glm::vec3(biomeInfo.grassColorR, biomeInfo.grassColorG, biomeInfo.grassColorB));
            renderer.setBiomeFoliageColor(glm::vec3(biomeInfo.foliageColorR, biomeInfo.foliageColorG, biomeInfo.foliageColorB));
            renderer.setUseBiomeColors(true);
        }
        
        // Get debris data for rendering (includes shadow pass)
        std::vector<Renderer::DebrisRenderData> debrisData;
#if ENABLE_PHYSICS_TEST
        if (physicsTest.isEnabled()) {
            debrisData = physicsTest.getDebrisRenderData();
        }
#endif

    std::vector<glm::vec3> fireLights;
    fireSystem.getFireLightPositions(fireLights, 16);
    renderer.setFireLightPositions(fireLights);

    renderer.render(chunkManager, camera, entities, window->getWidth(), window->getHeight(), debrisData, &explosionVolumes);
        // Clean up any GPU meshes for chunks that have been unloaded by ChunkManager
        renderer.cleanUnusedMeshes(chunkManager);
        
        // Blit depth buffer to default framebuffer so held items can properly occlude/be occluded
        renderer.blitDepthToScreen(window->getWidth(), window->getHeight());
        
        // Render block break overlay when breaking blocks in survival mode
        if (isBreakingBlock && !uiManager.isCreativeMode && blockBreakProgress > 0.0f) {
            renderer.renderBlockBreakOverlay(camera, breakingBlockPos, blockBreakProgress, window->getWidth(), window->getHeight());
        }
        
        // Render held items for players (third-person view)
        if (uiManager.isWorldLoaded()) {
            Shader& modelShader = renderer.getModelShader();
            
            // Render held items for remote players using bone-based attachment
            for (auto* remotePlayer : remotePlayerEntities) {
                uint8_t heldItemId = remotePlayer->getHeldItem();
                if (heldItemId != 0) {
                    ItemType itemType = static_cast<ItemType>(heldItemId);
                    
                    // Use bone-based rendering if supported
                    if (remotePlayer->supportsHoldAnimations()) {
                        glm::mat4 handTransform = remotePlayer->getRightHandTransform();
                        heldItemRenderer.renderThirdPersonWithBone(modelShader, camera, handTransform, itemType,
                                                                   window->getWidth(), window->getHeight());
                    } else {
                        // Fallback to simple position-based rendering
                        float playerYaw = remotePlayer->getRotation().y;
                        heldItemRenderer.renderThirdPerson(modelShader, camera, remotePlayer->getPosition(), 
                                                           playerYaw, itemType, 
                                                           window->getWidth(), window->getHeight());
                    }
                }
            }
            
            // Render local player's held item in third-person view
            if (camera.isThirdPerson() && playerEntity) {
                ItemType currentHeldItem = uiManager.getSelectedItem();
                if (currentHeldItem != ItemType::NONE) {
                    if (playerEntity->supportsHoldAnimations()) {
                        glm::mat4 handTransform = playerEntity->getRightHandTransform();
                        heldItemRenderer.renderThirdPersonWithBone(modelShader, camera, handTransform, currentHeldItem,
                                                                   window->getWidth(), window->getHeight());
                    } else {
                        // Fallback for non-Quaternius models
                        float playerYaw = playerEntity->getRotation().y;
                        heldItemRenderer.renderThirdPerson(modelShader, camera, playerEntity->getPosition(), 
                                                           playerYaw, currentHeldItem, 
                                                           window->getWidth(), window->getHeight());
                    }
                }
            }
        }
        
        // Render held item in first-person view (only when not in menu and in first person)
        if (!camera.isThirdPerson() && uiManager.isWorldLoaded() && !uiManager.isMenuOpen()) {
            // Get the model shader from renderer for held item
            Shader& modelShader = renderer.getModelShader();
            heldItemRenderer.renderFirstPerson(modelShader, camera, window->getWidth(), window->getHeight());
        }
        
        uiManager.render();
        uiManager.renderConsole();
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
