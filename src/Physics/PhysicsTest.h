// PhysicsTest.h - Isolated physics testing module
// To disable physics testing, comment out the ENABLE_PHYSICS_TEST define below
// or set it to 0

#pragma once

// ============================================================================
// PHYSICS TEST TOGGLE - Set to 0 to disable all physics testing code
// ============================================================================
#define ENABLE_PHYSICS_TEST 1

#if ENABLE_PHYSICS_TEST

#include "PhysicsWorld.h"
#include "ExplosionSystem.h"
#include "../Entity/DebrisEntity.h"
#include "../World/ChunkManager.h"
#include "../Render/Camera.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Render/Renderer.h"
#include "../Audio/AudioManager.h"
#include "../Core/Logger.h"
#include "../Entity/Entity.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <functional>
#include <vector>

namespace Physics {

/**
 * PhysicsTestSystem - Isolated testing harness for the physics engine
 * 
 * Test Controls:
 *   X - Trigger small explosion at crosshair
 *   C - Trigger large explosion at crosshair  
 *   V - Create debris at crosshair position
 *   P - Toggle physics test system on/off
 */
class PhysicsTestSystem {
public:
    PhysicsTestSystem() = default;
    ~PhysicsTestSystem() = default;
    
    void initialize(ChunkManager* chunkMgr, Camera* cam, ExplosionVolumeSystem* explosionVfx,
                    std::function<std::vector<Entity*>()> entityProviderFunc,
                    std::function<void(float, const glm::vec3&)> playerDamageFunc,
                    std::function<void(const glm::ivec3&)> fireStartFunc) {
        chunkManager = chunkMgr;
        camera = cam;
        explosionVolumes = explosionVfx;
        entityProvider = std::move(entityProviderFunc);
        playerDamage = std::move(playerDamageFunc);
        fireStart = std::move(fireStartFunc);
        
        PhysicsConfig config;
        config.gravity = glm::vec3(0.0f, -20.0f, 0.0f);
        config.fixedTimeStep = 1.0f / 60.0f;
        config.maxSubSteps = 4;
        config.velocityIterations = 8;
        
        physicsWorld = std::make_unique<PhysicsWorld>();
        physicsWorld->setConfig(config);
        
        explosionSystem = std::make_unique<ExplosionSystem>();
        
        explosionSystem->setBlockQuery([this](int x, int y, int z) -> Block {
            if (chunkManager) {
                return chunkManager->getBlockAt(x, y, z);
            }
            return Block(BlockType::AIR);
        });
        
        explosionSystem->setBlockSet([this](int x, int y, int z, Block block) {
            if (chunkManager) {
                chunkManager->setBlockAt(x, y, z, block);
            }
        });
        
        explosionSystem->setDebrisSpawn([this](const glm::vec3& pos, BlockType type, 
                                               const glm::vec3& vel, const glm::vec3& angVel, float scale) {
            createDebrisInternal(pos, vel, angVel, type, scale);
        });
        
        // Set up explosion sound callback
        explosionSystem->setSoundPlay([](const glm::vec3& pos, const std::string&, float volume) {
            // Play explosion sound at position with extended hearing range (128 blocks)
            // Explosions are loud and should be heard from far away
            Audio::AudioManager::instance().playSoundAtWithRange(
                Audio::SoundType::EXPLOSION, pos, volume, 64.0f, 1.0f);
        });
        
        // Set up screen shake callback
        explosionSystem->setScreenShake([this](const glm::vec3& explosionPos, float power) {
            if (!camera) {
                LOG_INFO("[PhysicsTest] Screen shake: camera is null!");
                return;
            }
            
            // Calculate distance from player to explosion
            float distance = glm::length(explosionPos - camera->getPosition());
            
            // Max shake range scales with explosion power
            float maxShakeRange = power * 10.0f;  // e.g., power 4 = 40 blocks, power 8 = 80 blocks
            
            LOG_INFO("[PhysicsTest] Screen shake: dist=" + std::to_string(distance) + 
                     " maxRange=" + std::to_string(maxShakeRange) + " power=" + std::to_string(power));
            
            if (distance < maxShakeRange) {
                // Intensity falls off with distance (inverse square-ish)
                float normalizedDist = distance / maxShakeRange;
                float distanceFalloff = (1.0f - normalizedDist);
                float distanceCurve = distanceFalloff * distanceFalloff; // softer falloff

                // Calmer, more explosion-like shake
                float baseIntensity = (power <= 4.0f) ? 0.12f : 0.20f;   // X vs C
                float intensity = baseIntensity * power * distanceCurve;
                float closeBoostStrong = distanceCurve * distanceCurve; // sharper boost when very close
                intensity *= (1.0f + closeBoostStrong * 1.8f);

                // Subtle variation to avoid identical shakes
                float jitter = 0.85f + (static_cast<float>(std::rand() % 30) / 100.0f); // 0.85..1.14
                intensity *= jitter;
                
                // Duration scales gently with power and proximity
                float baseDuration = (power <= 4.0f) ? 0.28f : 0.40f;
                float duration = baseDuration + (power * 0.05f) * distanceCurve + closeBoostStrong * 0.7f;
                
                LOG_INFO("[PhysicsTest] Adding shake: intensity=" + std::to_string(intensity) + 
                         " duration=" + std::to_string(duration));
                
                // Add the shake to camera
                camera->addScreenShake(intensity, duration);

                // Trigger explosion muffle/beep when very close
                float nearRange = power * 6.0f;
                if (distance < nearRange) {
                    float nearNorm = 1.0f - (distance / nearRange); // 0..1 (closer = 1)
                    float closeBoost = nearNorm * nearNorm;
                    float closeBoostStrong = closeBoost * closeBoost;

                    float muffleStrength = std::clamp(closeBoostStrong * (power / 7.0f), 0.3f, 1.0f);
                    float muffleDuration = 0.45f + power * 0.07f + closeBoostStrong * 0.9f; // longer when closer

                    float beepVolume = std::clamp(0.6f + closeBoostStrong * 1.0f, 0.6f, 1.6f);
                    float beepPitch = std::clamp(0.9f + closeBoost * 0.15f, 0.9f, 1.15f);

                    Audio::AudioManager::instance().triggerExplosionMuffle(
                        muffleStrength, muffleDuration, beepVolume, beepPitch);
                }
            }
        });

        // Set up volumetric explosion VFX callback
        explosionSystem->setExplosionVfx([this](const glm::vec3& pos, float power) {
            if (explosionVolumes) {
                explosionVolumes->spawn(pos, power);
            }
        });

        // Set up entity damage callback
        explosionSystem->setEntityDamage([this](const glm::vec3& pos, float radius, float damage, const glm::vec3& center) {
            if (radius <= 0.01f) return;

            // Damage player
            if (playerDamage && camera) {
                glm::vec3 playerPos = camera->getPosition();
                float dist = glm::length(playerPos - pos);
                if (dist < radius) {
                    float falloff = 1.0f - (dist / radius);
                    float appliedDamage = damage * falloff;
                    glm::vec3 knockDir = (playerPos - center);
                    if (glm::length(knockDir) > 0.001f) knockDir = glm::normalize(knockDir);
                    playerDamage(appliedDamage, knockDir * (falloff * 8.0f));
                }
            }

            // Damage mobs/entities
            if (entityProvider) {
                auto entities = entityProvider();
                for (auto* entity : entities) {
                    if (!entity || entity->isDead()) continue;
                    float dist = glm::length(entity->getPosition() - pos);
                    if (dist < radius) {
                        float falloff = 1.0f - (dist / radius);
                        float appliedDamage = damage * falloff;
                        glm::vec3 knockDir = (entity->getPosition() - center);
                        if (glm::length(knockDir) > 0.001f) knockDir = glm::normalize(knockDir);
                        entity->takeDamage(appliedDamage, knockDir * (falloff * 8.0f));
                    }
                }
            }
        });
        
        debrisManager.setTerrainQuery([this](int x, int y, int z) -> Block {
            if (chunkManager) {
                return chunkManager->getBlockAt(x, y, z);
            }
            return Block(BlockType::AIR);
        });
        
        // Configure debris with longer lifetime
        auto debrisCfg = debrisManager.getDefaultConfig();
        debrisCfg.lifetime = 30.0f;           // 30 seconds
        debrisCfg.fadeTime = 3.0f;            // 3 second fade
        debrisCfg.bounceRestitution = 0.3f;
        debrisCfg.friction = 0.8f;
        debrisCfg.linearDamping = 0.1f;
        debrisCfg.angularDamping = 0.3f;
        debrisCfg.minVelocityToRest = 0.1f;
        debrisCfg.collideWithTerrain = true;
        debrisManager.setDefaultConfig(debrisCfg);
        
        initialized = true;
        LOG_INFO("[PhysicsTest] Initialized - X/C=explosion, V=debris, B=fire, P=toggle");
    }
    
    void update(float deltaTime) {
        if (!initialized || !enabled) return;
        
        physicsAccumulator += deltaTime;
        const float fixedDt = 1.0f / 60.0f;
        
        while (physicsAccumulator >= fixedDt) {
            physicsWorld->step(fixedDt);
            debrisManager.physicsUpdate(fixedDt);
            physicsAccumulator -= fixedDt;
        }
        
        debrisManager.update(deltaTime);
        explosionSystem->update(deltaTime);
    }
    
    bool handleInput(GLFWwindow* window, const Camera& camera, float) {
        if (!initialized) return false;
        
        static bool pPressed = false;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pPressed) {
                enabled = !enabled;
                LOG_INFO("[PhysicsTest] " + std::string(enabled ? "ENABLED" : "DISABLED"));
                pPressed = true;
                return true;
            }
        } else { pPressed = false; }
        
        if (!enabled) return false;
        
        static bool xPressed = false;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
            if (!xPressed) {
                triggerExplosionAtCrosshair(camera, 4.0f);
                xPressed = true;
                return true;
            }
        } else { xPressed = false; }
        
        static bool cPressed = false;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            if (!cPressed) {
                triggerExplosionAtCrosshair(camera, 8.0f);
                cPressed = true;
                return true;
            }
        } else { cPressed = false; }
        
        static bool vPressed = false;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
            if (!vPressed) {
                createDebrisAtCrosshair(camera);
                vPressed = true;
                return true;
            }
        } else { vPressed = false; }

        static bool bPressed = false;
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
            if (!bPressed) {
                startFireAtCrosshair(camera);
                bPressed = true;
                return true;
            }
        } else { bPressed = false; }
        
        return false;
    }
    
    void triggerExplosionAtCrosshair(const Camera& camera, float power) {
        if (!chunkManager) return;
        
        glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
        auto result = chunkManager->rayCast(eyePos, camera.getFront(), 50.0f);
        
        glm::vec3 explosionPos;
        if (result.hit) {
            glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
            explosionPos = chunkOrigin + glm::vec3(result.blockPos) + glm::vec3(0.5f);
        } else {
            explosionPos = eyePos + camera.getFront() * 20.0f;
        }
        
        ExplosionParams params;
        params.center = explosionPos;
        params.power = power;
        params.breakBlocks = true;
        params.createDebris = true;
        params.chainReaction = true;
        
        ExplosionResult res = explosionSystem->explode(params);
        
        LOG_INFO("[PhysicsTest] BOOM! blocks=" + std::to_string(res.destroyedBlocks.size()) +
                 " debris=" + std::to_string(res.debrisCreated));
    }
    
    void createDebrisAtCrosshair(const Camera& camera) {
        if (!chunkManager) return;
        
        glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
        auto result = chunkManager->rayCast(eyePos, camera.getFront(), 50.0f);
        
        if (result.hit) {
            glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
            glm::vec3 blockPos = chunkOrigin + glm::vec3(result.blockPos) + glm::vec3(0.5f);
            
            int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
            int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
            int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
            BlockType blockType = chunkManager->getBlockAt(x, y, z).getType();
            
            if (blockType != BlockType::AIR && blockType != BlockType::WATER) {
                glm::vec3 vel(
                    (rand() % 100 - 50) / 10.0f,
                    10.0f + (rand() % 50) / 10.0f,
                    (rand() % 100 - 50) / 10.0f
                );
                glm::vec3 angVel(
                    (rand() % 100 - 50) / 5.0f,
                    (rand() % 100 - 50) / 5.0f,
                    (rand() % 100 - 50) / 5.0f
                );
                
                createDebrisInternal(blockPos, vel, angVel, blockType, 0.4f);
                chunkManager->setBlockAt(x, y, z, Block(BlockType::AIR));
                
                LOG_INFO("[PhysicsTest] Debris created, total: " + 
                         std::to_string(debrisManager.getStats().activeDebris));
            }
        }
    }

    void startFireAtCrosshair(const Camera& camera) {
        if (!chunkManager || !fireStart) return;

        glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
        auto result = chunkManager->rayCast(eyePos, camera.getFront(), 50.0f);

        if (result.hit) {
            glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
            int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
            int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
            int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
            fireStart(glm::ivec3(x, y, z));
        }
    }
    
    // Get debris data for rendering
    std::vector<Renderer::DebrisRenderData> getDebrisRenderData() const {
        std::vector<Renderer::DebrisRenderData> result;
        for (const auto& debris : debrisManager.getActiveDebris()) {
            if (debris && !debris->isExpired()) {
                Renderer::DebrisRenderData data;
                data.position = debris->getPosition();
                data.rotation = debris->getOrientationQuat();
                data.scale = debris->getDebrisScale();
                data.blockType = debris->getBlockType();
                data.alpha = debris->getFadeAlpha();
                result.push_back(data);
            }
        }
        return result;
    }
    
    // Spawn a dropped item when a block is mined (survival mode)
    // Unlike explosion debris, these are:
    // - Smaller (like pickup items)
    // - Gentle drop (no explosion velocity)
    // - Much longer lifetime (can be picked up)
    void spawnDroppedItem(const glm::vec3& blockCenter, BlockType blockType) {
        if (!initialized || blockType == BlockType::AIR || blockType == BlockType::WATER) return;
        
        // Small random offset so item doesn't spawn exactly at block center
        float offsetX = ((rand() % 100) - 50) / 200.0f; // -0.25 to 0.25
        float offsetZ = ((rand() % 100) - 50) / 200.0f;
        glm::vec3 spawnPos = blockCenter + glm::vec3(offsetX, 0.0f, offsetZ);
        
        // Gentle upward pop with slight random horizontal
        glm::vec3 velocity(
            ((rand() % 100) - 50) / 50.0f,  // -1 to 1
            2.0f + (rand() % 30) / 10.0f,    // 2 to 5 upward
            ((rand() % 100) - 50) / 50.0f   // -1 to 1
        );
        
        // Slow tumble
        glm::vec3 angularVel(
            ((rand() % 100) - 50) / 20.0f,  // -2.5 to 2.5
            ((rand() % 100) - 50) / 20.0f,
            ((rand() % 100) - 50) / 20.0f
        );
        
        // Smaller scale - like a pickup item (0.25 = quarter block size)
        float scale = 0.25f;
        
        ::DebrisEntity* item = debrisManager.createDebris(spawnPos, blockType, velocity, angularVel, scale);
        if (item) {
            ::DebrisEntity::Config cfg;
            cfg.lifetime = 300.0f;          // 5 minutes - long time to pick up
            cfg.fadeTime = 10.0f;           // 10 second fade when expiring
            cfg.bounceRestitution = 0.2f;   // Low bounce - more like dropped item
            cfg.friction = 0.9f;            // High friction - stops quickly
            cfg.linearDamping = 0.1f;       // Some air resistance
            cfg.angularDamping = 0.15f;     // Slow spin decay
            cfg.collideWithTerrain = true;
            item->setConfig(cfg);
        }
    }
    
    bool isEnabled() const { return enabled && initialized; }
    void setEnabled(bool e) { enabled = e; }
    
    std::string getDebugInfo() const {
        if (!initialized) return "";
        if (!enabled) return "[PhysicsTest] OFF (P=toggle)";
        return "[PhysicsTest] Debris:" + std::to_string(debrisManager.getStats().activeDebris);
    }
    
private:
    void createDebrisInternal(const glm::vec3& pos, const glm::vec3& vel, 
                              const glm::vec3& angVel, BlockType type, float scale) {
        ::DebrisEntity* debris = debrisManager.createDebris(pos, type, vel, angVel, scale);
        if (debris) {
            ::DebrisEntity::Config cfg;
            cfg.lifetime = 30.0f;          // Long lifetime
            cfg.fadeTime = 8.0f;           // Long fade time for smooth disappearance
            cfg.bounceRestitution = 0.35f; // Less bouncy for realism
            cfg.friction = 0.7f;           // More friction
            cfg.linearDamping = 0.03f;
            cfg.angularDamping = 0.08f;
            cfg.collideWithTerrain = true;
            debris->setConfig(cfg);
        }
    }
    
    bool initialized = false;
    bool enabled = true;
    float physicsAccumulator = 0.0f;
    
    ChunkManager* chunkManager = nullptr;
    Camera* camera = nullptr;
    ExplosionVolumeSystem* explosionVolumes = nullptr;
    std::function<std::vector<Entity*>()> entityProvider;
    std::function<void(float, const glm::vec3&)> playerDamage;
    std::function<void(const glm::ivec3&)> fireStart;
    std::unique_ptr<PhysicsWorld> physicsWorld;
    std::unique_ptr<ExplosionSystem> explosionSystem;
    ::DebrisManager debrisManager;
};

} // namespace Physics

#else

namespace Physics {
class PhysicsTestSystem {
public:
    template <typename... Args>
    void initialize(Args&&...) {}
    void update(float) {}
    bool handleInput(void*, const void&, float) { return false; }
    bool isEnabled() const { return false; }
    void setEnabled(bool) {}
    std::string getDebugInfo() const { return ""; }
    std::vector<int> getDebrisRenderData() const { return {}; }
};
} // namespace Physics

#endif
