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
#include "../Render/Renderer.h"
#include "../Core/Logger.h"

#include <GLFW/glfw3.h>
#include <memory>

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
    
    void initialize(ChunkManager* chunkMgr) {
        chunkManager = chunkMgr;
        
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
        LOG_INFO("[PhysicsTest] Initialized - X/C=explosion, V=debris, P=toggle");
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
    std::unique_ptr<PhysicsWorld> physicsWorld;
    std::unique_ptr<ExplosionSystem> explosionSystem;
    ::DebrisManager debrisManager;
};

} // namespace Physics

#else

namespace Physics {
class PhysicsTestSystem {
public:
    void initialize(void*) {}
    void update(float) {}
    bool handleInput(void*, const void&, float) { return false; }
    bool isEnabled() const { return false; }
    void setEnabled(bool) {}
    std::string getDebugInfo() const { return ""; }
    std::vector<int> getDebrisRenderData() const { return {}; }
};
} // namespace Physics

#endif
