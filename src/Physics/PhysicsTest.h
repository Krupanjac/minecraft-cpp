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
#include "../Entity/LimbDebrisEntity.h"
#include "../World/ChunkManager.h"
#include "../Render/Camera.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Render/Renderer.h"
#include "../Audio/AudioManager.h"
#include "../Core/Logger.h"
#include "../Entity/Entity.h"
#include "../Model/Model.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
        physicsWorld->setBlockQueryFunction([this](int x, int y, int z) {
            if (!chunkManager) return false;
            Block block = chunkManager->getBlockAt(x, y, z);
            return block.isSolid();
        });
        
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

        // Set up fire-start callback for explosions
        explosionSystem->setFireStart([this](const glm::ivec3& pos) {
            if (fireStart) {
                fireStart(pos);
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
                    if (!entity) continue;
                    
                    float dist = glm::length(entity->getPosition() - pos);
                    if (dist < radius) {
                        float falloff = 1.0f - (dist / radius);
                        float appliedDamage = damage * falloff;
                        glm::vec3 knockDir = (entity->getPosition() - center);
                        if (glm::length(knockDir) > 0.001f) knockDir = glm::normalize(knockDir);
                        
                        // Apply damage to living entities
                        if (!entity->isDead()) {
                            entity->takeDamage(appliedDamage, knockDir * (falloff * 8.0f));
                        }
                        
                        // Also damage ragdolls (dead or just killed)
                        applyExplosionToRagdoll(entity, center, radius, appliedDamage, knockDir);
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
        
        // Auto-create ragdolls for dead entities
        checkForDeadEntities();
        
        physicsAccumulator += deltaTime;
        const float fixedDt = 1.0f / 60.0f;
        
        while (physicsAccumulator >= fixedDt) {
            physicsWorld->step(fixedDt);
            applyRagdollConstraints(fixedDt);
            debrisManager.physicsUpdate(fixedDt);
            for (auto& limb : limbDebris) {
                if (limb) limb->physicsUpdate(fixedDt);
            }
            physicsAccumulator -= fixedDt;
        }
        
        debrisManager.update(deltaTime);
        explosionSystem->update(deltaTime);
        updateRagdollPose();
        
        // Update limb debris
        for (auto& limb : limbDebris) {
            if (limb) limb->update(deltaTime);
        }
        
        // Remove expired limb debris
        limbDebris.erase(
            std::remove_if(limbDebris.begin(), limbDebris.end(),
                [](const std::unique_ptr<LimbDebrisEntity>& l) { return !l || l->isExpired(); }),
            limbDebris.end()
        );
    }
    
    // Check for dead entities and auto-create ragdolls
    void checkForDeadEntities() {
        if (!entityProvider) return;
        
        auto entities = entityProvider();
        for (auto* entity : entities) {
            if (!entity || !entity->isDead()) continue;
            if (!entity->getModel()) continue;
            
            // Already has ragdoll?
            if (ragdolls.find(entity) != ragdolls.end()) continue;
            
            // Create ragdoll for this dead entity
            LOG_INFO("[Ragdoll] Auto-creating ragdoll for dead entity");
            createRagdollForEntity(entity);
        }
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
        params.createFire = true;
        params.createDebris = true;
        params.chainReaction = true;
        params.fireChanceMultiplier = (power >= 8.0f) ? 3.0f : 1.5f;
        
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
                data.waterFactor = debris->getWaterSubmersion();
                result.push_back(data);
            }
        }
        return result;
    }
    
    // Render limb debris (call after model rendering pass)
    void renderLimbDebris(Shader& shader, const glm::vec3& renderOrigin) {
        static int logCount = 0;
        if (logCount < 5 && !limbDebris.empty()) {
            LOG_INFO("[PhysicsTest] renderLimbDebris called, limbDebris.size()=" + std::to_string(limbDebris.size()));
            logCount++;
        }
        for (const auto& limb : limbDebris) {
            if (limb && !limb->isExpired()) {
                limb->renderWithOrigin(shader, renderOrigin);
            }
        }
    }
    
    // Get limb debris count for debug info
    size_t getLimbDebrisCount() const { return limbDebris.size(); }
    
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
        std::string rag = " Ragdolls:" + std::to_string(ragdolls.size());
        return "[PhysicsTest] Debris:" + std::to_string(debrisManager.getStats().activeDebris) + rag;
    }

    void handleAttackHit(Entity* target, const glm::vec3& rayOrigin, const glm::vec3& rayDir, float damage) {
        LOG_INFO("[RAGDOLL] handleAttackHit called! damage=" + std::to_string(damage));
        
        if (!target) {
            LOG_INFO("[RAGDOLL] No target!");
            return;
        }
        
        if (!target->getModel()) {
            LOG_INFO("[RAGDOLL] Target has no model!");
            return;
        }
        
        LOG_INFO("[RAGDOLL] Entity is " + std::string(target->isDead() ? "DEAD" : "ALIVE"));
        
        // Get or create ragdoll for this entity (works for living OR dead)
        auto* rag = getOrCreateRagdoll(target);
        if (!rag) {
            LOG_INFO("[RAGDOLL] Could not create ragdoll!");
            return;
        }
        
        LOG_INFO("[RAGDOLL] Ragdoll has " + std::to_string(rag->joints.size()) + " joints");
        
        // Update joint positions from current entity state before hit detection
        updateJointPositionsFromEntity(*rag);
        
        // Apply damage to the hit limb
        damageRagdollFromRay(*rag, rayOrigin, rayDir, damage);
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

    // ========================================================================
    // RAGDOLL SYSTEM - Limb damage and detachment for living AND dead entities
    // - Living entities: joints follow animation, but detached limbs fall off
    // - Dead entities: full ragdoll physics on all joints
    // ========================================================================

    struct RagdollJoint {
        int nodeIndex = -1;           // Index in model's node list
        int parentJointIndex = -1;    // Index in ragdoll.joints (-1 = root)
        glm::vec3 position;           // Current world position
        glm::vec3 velocity;           // Current velocity (for detached limbs)
        glm::quat rotation;           // Current rotation
        float radius = 0.15f;         // Collision radius for hit detection
        float health = 10.0f;         // Current limb health
        float maxHealth = 10.0f;      // Max limb health
        bool detached = false;        // Whether this limb has been detached
    };

    struct RagdollConstraint {
        int jointA = -1;              // Index in ragdoll.joints
        int jointB = -1;              // Index in ragdoll.joints
        float restLength = 0.1f;      // Target distance between joints
        bool active = true;           // Whether constraint is still active
    };

    struct RagdollInstance {
        Entity* entity = nullptr;
        std::shared_ptr<ModelSystem::Model> model;
        std::vector<RagdollJoint> joints;
        std::vector<RagdollConstraint> constraints;
        std::vector<std::string> nodeNames;
        std::unordered_map<int, int> nodeToJointMap;  // nodeIndex -> joint index
        bool active = false;
        float entityMaxHealth = 20.0f;
    };

    std::unordered_map<Entity*, RagdollInstance> ragdolls;

    // Helper functions
    static std::string toLowerCopy(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return out;
    }

    static bool containsAny(const std::string& haystack, const std::vector<std::string>& needles) {
        for (const auto& n : needles) {
            if (haystack.find(n) != std::string::npos) return true;
        }
        return false;
    }

    static bool isTorsoCore(const std::string& name) {
        std::string lower = toLowerCopy(name);
        return containsAny(lower, {"spine", "chest", "torso", "hips", "pelvis", "body", "root"});
    }

    // Check if a node name looks like a skeleton bone (not a helper/IK node)
    static bool isSkeletonBone(const std::string& name) {
        std::string lower = toLowerCopy(name);
        
        // Skip helper/IK/control nodes
        if (containsAny(lower, {"ik", "target", "pole", "ctrl", "control", "helper", 
                                "twist", "roll", "offset", "constraint", "driver"})) {
            return false;
        }
        
        // Accept common bone names
        if (containsAny(lower, {"hips", "pelvis", "spine", "chest", "torso", "neck", "head",
                                "shoulder", "clavicle", "arm", "elbow", "forearm", "wrist", "hand", "fist",
                                "leg", "thigh", "knee", "calf", "shin", "ankle", "foot", "toe",
                                "root", "armature", "bone", "joint", "skeleton", "body",
                                "left", "right", "upper", "lower", "mixamo"})) {
            return true;
        }
        
        // Also accept if name contains numbers (likely auto-generated bone)
        for (char c : name) {
            if (std::isdigit(c)) return true;
        }
        
        // Accept any name that doesn't look like a mesh or material
        if (!containsAny(lower, {"mesh", "material", "geometry", "shape", "skin", "object"})) {
            return true;  // Accept most nodes
        }
        
        return false;
    }

    // Get limb health - LOW values so limbs detach in 1-2 sword hits (4-7 damage each)
    static float getLimbHealth(const std::string& name) {
        std::string lower = toLowerCopy(name);
        
        if (containsAny(lower, {"head", "neck"})) return 8.0f;
        if (containsAny(lower, {"spine", "chest", "torso", "hips", "pelvis"})) return 25.0f;  // Body harder to detach
        if (containsAny(lower, {"hand", "fist", "foot", "toe"})) return 5.0f;
        if (containsAny(lower, {"arm", "forearm", "shoulder", "clavicle"})) return 7.0f;
        if (containsAny(lower, {"leg", "thigh", "calf", "shin"})) return 10.0f;
        
        return 8.0f;  // Default
    }

    // Get collision radius based on body part
    static float getJointRadius(const std::string& name, float baseScale) {
        std::string lower = toLowerCopy(name);
        
        if (containsAny(lower, {"head"})) return 0.20f * baseScale;
        if (containsAny(lower, {"spine", "chest", "torso", "hips", "pelvis"})) return 0.25f * baseScale;
        if (containsAny(lower, {"shoulder", "clavicle"})) return 0.15f * baseScale;
        if (containsAny(lower, {"arm", "forearm"})) return 0.12f * baseScale;
        if (containsAny(lower, {"hand", "fist"})) return 0.10f * baseScale;
        if (containsAny(lower, {"thigh", "leg"})) return 0.15f * baseScale;
        if (containsAny(lower, {"calf", "shin"})) return 0.12f * baseScale;
        if (containsAny(lower, {"foot", "ankle"})) return 0.12f * baseScale;
        
        return 0.12f * baseScale;
    }

    // Build entity world transform
    glm::mat4 buildEntityTransform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }

    // Clear and destroy ragdoll
    void clearRagdoll(Entity* entity) {
        if (!entity) return;
        
        auto it = ragdolls.find(entity);
        if (it == ragdolls.end()) return;
        
        auto& ragdoll = it->second;
        if (ragdoll.model) {
            ragdoll.model->clearAllNodeOverrides();
            ragdoll.model->setExternalPoseEnabled(false);
        }
        ragdolls.erase(it);
    }

    // Get or create ragdoll for ANY entity (living or dead)
    RagdollInstance* getOrCreateRagdoll(Entity* entity) {
        if (!entity) return nullptr;
        
        auto it = ragdolls.find(entity);
        if (it != ragdolls.end()) return &it->second;
        
        return createRagdollForEntity(entity);
    }

    // Create ragdoll from entity
    RagdollInstance* createRagdollForEntity(Entity* entity) {
        if (!entity) return nullptr;

        auto model = entity->getModel();
        if (!model) return nullptr;

        const auto names = model->getNodeNames();
        if (names.empty()) {
            LOG_INFO("[Ragdoll] Model has no nodes");
            return nullptr;
        }

        LOG_INFO("[Ragdoll] Model has " + std::to_string(names.size()) + " nodes:");
        for (size_t i = 0; i < names.size() && i < 20; ++i) {
            LOG_INFO("  Node " + std::to_string(i) + ": " + names[i]);
        }

        // Get skin joints - these are the actual skeleton bones
        const auto& skinJoints = model->getActiveSkinJoints();
        LOG_INFO("[Ragdoll] Model has " + std::to_string(skinJoints.size()) + " skin joints");
        
        // Filter to only include real skeleton bones
        std::vector<int> boneIndices;
        if (!skinJoints.empty()) {
            for (int idx : skinJoints) {
                if (idx >= 0 && static_cast<size_t>(idx) < names.size()) {
                    if (isSkeletonBone(names[idx])) {
                        boneIndices.push_back(idx);
                        LOG_INFO("[Ragdoll] Accepted skin joint: " + names[idx]);
                    } else {
                        LOG_INFO("[Ragdoll] Rejected skin joint: " + names[idx]);
                    }
                }
            }
        }
        
        // Fallback: try to find bones by name if skin has no valid bones
        if (boneIndices.empty()) {
            LOG_INFO("[Ragdoll] No skin joints, falling back to node name search");
            for (size_t i = 0; i < names.size(); ++i) {
                if (isSkeletonBone(names[i])) {
                    boneIndices.push_back(static_cast<int>(i));
                    LOG_INFO("[Ragdoll] Accepted node by name: " + names[i]);
                }
            }
        }
        
        if (boneIndices.size() < 2) {
            LOG_INFO("[Ragdoll] Not enough skeleton bones found: " + std::to_string(boneIndices.size()));
            // Last resort: just use all nodes
            LOG_INFO("[Ragdoll] Using ALL nodes as fallback");
            boneIndices.clear();
            for (size_t i = 0; i < names.size(); ++i) {
                boneIndices.push_back(static_cast<int>(i));
            }
        }
        
        if (boneIndices.size() < 2) {
            LOG_INFO("[Ragdoll] Still not enough bones, giving up");
            return nullptr;
        }
        
        LOG_INFO("[Ragdoll] Using " + std::to_string(boneIndices.size()) + " bones for ragdoll");

        // Create ragdoll instance
        ragdolls[entity] = RagdollInstance();
        RagdollInstance& ragdoll = ragdolls[entity];

        ragdoll.entity = entity;
        ragdoll.model = model;
        ragdoll.active = true;
        ragdoll.nodeNames = names;
        ragdoll.entityMaxHealth = std::max(1.0f, entity->getMaxHealth());

        // Get entity transform info
        glm::vec3 entPos = entity->getPosition();
        glm::vec3 entRot = entity->getRotation();
        glm::vec3 entScale = entity->getScale();
        glm::mat4 entityWorld = buildEntityTransform(entPos, entRot, entScale);
        float avgScale = (entScale.x + entScale.y + entScale.z) / 3.0f;

        // Create joints for each bone
        for (int nodeIdx : boneIndices) {
            // Get bone's transform in model space
            glm::mat4 nodeModelSpace = model->getNodeGlobalTransformByIndex(nodeIdx);
            
            // Transform to world space
            glm::mat4 nodeWorld = entityWorld * nodeModelSpace;
            
            // Extract world position
            glm::vec3 worldPos = glm::vec3(nodeWorld[3]);
            
            // Extract rotation
            glm::mat3 rotMat = glm::mat3(nodeWorld);
            glm::vec3 col0 = glm::normalize(rotMat[0]);
            glm::vec3 col1 = glm::normalize(rotMat[1]);
            glm::vec3 col2 = glm::normalize(rotMat[2]);
            glm::quat worldRot = glm::quat_cast(glm::mat3(col0, col1, col2));

            std::string nodeName = (nodeIdx >= 0 && static_cast<size_t>(nodeIdx) < names.size()) ? names[nodeIdx] : "";

            // Create joint
            RagdollJoint joint;
            joint.nodeIndex = nodeIdx;
            joint.parentJointIndex = -1;
            joint.position = worldPos;
            joint.velocity = glm::vec3(0.0f);
            joint.rotation = worldRot;
            joint.radius = getJointRadius(nodeName, avgScale);
            joint.detached = false;
            joint.health = getLimbHealth(nodeName);
            joint.maxHealth = joint.health;

            int jointIndex = static_cast<int>(ragdoll.joints.size());
            ragdoll.nodeToJointMap[nodeIdx] = jointIndex;
            ragdoll.joints.push_back(joint);
        }

        // Set parent joint indices and create constraints
        for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
            int nodeIdx = ragdoll.joints[i].nodeIndex;
            int parentNodeIdx = model->getParentIndex(nodeIdx);
            
            // Find if parent node is also a ragdoll joint
            auto parentIt = ragdoll.nodeToJointMap.find(parentNodeIdx);
            if (parentIt != ragdoll.nodeToJointMap.end()) {
                ragdoll.joints[i].parentJointIndex = parentIt->second;
                
                // Create constraint between this joint and its parent
                RagdollConstraint c;
                c.jointA = static_cast<int>(i);
                c.jointB = parentIt->second;
                c.active = true;
                
                // Calculate rest length from current positions
                glm::vec3 posA = ragdoll.joints[i].position;
                glm::vec3 posB = ragdoll.joints[c.jointB].position;
                c.restLength = std::max(0.02f, glm::length(posA - posB));
                
                ragdoll.constraints.push_back(c);
            }
        }

        LOG_INFO("[Ragdoll] Created with " + std::to_string(ragdoll.joints.size()) + 
                 " joints, " + std::to_string(ragdoll.constraints.size()) + " constraints");
        
        // Log joint positions for debugging
        for (size_t i = 0; i < ragdoll.joints.size() && i < 10; ++i) {
            const auto& j = ragdoll.joints[i];
            std::string name = (j.nodeIndex >= 0 && static_cast<size_t>(j.nodeIndex) < names.size()) 
                              ? names[j.nodeIndex] : "?";
            LOG_INFO("[Ragdoll] Joint " + name + " pos=" + 
                     std::to_string(j.position.x) + "," + std::to_string(j.position.y) + "," + 
                     std::to_string(j.position.z) + " health=" + std::to_string(j.health));
        }
        
        return &ragdoll;
    }

    // Update joint positions from entity's current animation (for living entities)
    void updateJointPositionsFromEntity(RagdollInstance& ragdoll) {
        if (!ragdoll.entity || !ragdoll.model) return;
        
        Entity* entity = ragdoll.entity;
        glm::vec3 entPos = entity->getPosition();
        glm::vec3 entRot = entity->getRotation();
        glm::vec3 entScale = entity->getScale();
        glm::mat4 entityWorld = buildEntityTransform(entPos, entRot, entScale);
        
        for (auto& joint : ragdoll.joints) {
            if (joint.detached) continue;  // Don't update detached limbs
            
            // Get bone's current animation transform
            glm::mat4 nodeModelSpace = ragdoll.model->getNodeGlobalTransformByIndex(joint.nodeIndex);
            glm::mat4 nodeWorld = entityWorld * nodeModelSpace;
            
            // Update position from animation
            joint.position = glm::vec3(nodeWorld[3]);
            
            // Extract rotation
            glm::mat3 rotMat = glm::mat3(nodeWorld);
            glm::vec3 col0 = glm::normalize(rotMat[0]);
            glm::vec3 col1 = glm::normalize(rotMat[1]);
            glm::vec3 col2 = glm::normalize(rotMat[2]);
            joint.rotation = glm::quat_cast(glm::mat3(col0, col1, col2));
        }
    }

    // Simulate ragdoll physics - called from update()
    void simulateRagdolls(float dt) {
        if (ragdolls.empty()) return;
        
        const glm::vec3 gravity(0.0f, -15.0f, 0.0f);
        const float damping = 0.98f;
        
        for (auto& [entity, ragdoll] : ragdolls) {
            if (!ragdoll.active) continue;
            
            bool entityIsDead = entity && entity->isDead();
            
            if (entityIsDead) {
                // DEAD entity - full ragdoll physics on all joints
                ragdoll.model->setExternalPoseEnabled(true);
                
                // Apply gravity and integrate velocities for ALL joints
                for (auto& joint : ragdoll.joints) {
                    joint.velocity += gravity * dt;
                    joint.velocity *= damping;
                    joint.position += joint.velocity * dt;
                }
                
                // Solve distance constraints (multiple iterations)
                const int constraintIterations = 10;
                for (int iter = 0; iter < constraintIterations; ++iter) {
                    for (auto& c : ragdoll.constraints) {
                        if (!c.active) continue;
                        if (c.jointA < 0 || c.jointB < 0) continue;
                        auto& jointA = ragdoll.joints[c.jointA];
                        auto& jointB = ragdoll.joints[c.jointB];
                        
                        if (jointA.detached || jointB.detached) continue;
                        
                        glm::vec3 delta = jointB.position - jointA.position;
                        float dist = glm::length(delta);
                        
                        if (dist < 0.0001f) continue;
                        
                        float error = dist - c.restLength;
                        glm::vec3 correction = (delta / dist) * error * 0.5f;
                        
                        jointA.position += correction;
                        jointB.position -= correction;
                    }
                }
                
                // World collision for all joints
                resolveRagdollWorldCollision(ragdoll);
                
            } else {
                // LIVING entity - only simulate detached limbs
                // First, sync attached joints with entity animation
                updateJointPositionsFromEntity(ragdoll);
                
                // Then simulate detached limbs
                for (auto& joint : ragdoll.joints) {
                    if (joint.detached) {
                        joint.velocity += gravity * dt;
                        joint.velocity *= damping;
                        joint.position += joint.velocity * dt;
                    }
                }
                
                // World collision only for detached limbs
                if (chunkManager) {
                    for (auto& joint : ragdoll.joints) {
                        if (!joint.detached) continue;
                        
                        // Proper collision with blocks
                        glm::vec3 pos = joint.position;
                        float radius = joint.radius;
                        glm::vec3 half(radius);
                        glm::vec3 aabbMin = pos - half;
                        glm::vec3 aabbMax = pos + half;
                        
                        glm::ivec3 minB = glm::ivec3(glm::floor(aabbMin));
                        glm::ivec3 maxB = glm::ivec3(glm::floor(aabbMax));
                        
                        glm::vec3 totalCorrection(0.0f);
                        
                        for (int x = minB.x; x <= maxB.x; ++x) {
                            for (int y = minB.y; y <= maxB.y; ++y) {
                                for (int z = minB.z; z <= maxB.z; ++z) {
                                    Block block = chunkManager->getBlockAt(x, y, z);
                                    if (!block.isSolid()) continue;
                                    
                                    glm::vec3 blockMin(x, y, z);
                                    glm::vec3 blockMax(x + 1.0f, y + 1.0f, z + 1.0f);
                                    
                                    float overlapX = std::min(aabbMax.x, blockMax.x) - std::max(aabbMin.x, blockMin.x);
                                    float overlapY = std::min(aabbMax.y, blockMax.y) - std::max(aabbMin.y, blockMin.y);
                                    float overlapZ = std::min(aabbMax.z, blockMax.z) - std::max(aabbMin.z, blockMin.z);
                                    
                                    if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
                                        glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
                                        glm::vec3 blockCenter = (blockMin + blockMax) * 0.5f;
                                        glm::vec3 diff = center - blockCenter;
                                        
                                        if (overlapX < overlapY && overlapX < overlapZ) {
                                            float dir = (diff.x >= 0.0f) ? 1.0f : -1.0f;
                                            totalCorrection.x += dir * overlapX;
                                            joint.velocity.x = 0.0f;
                                        } else if (overlapY < overlapZ) {
                                            float dir = (diff.y >= 0.0f) ? 1.0f : -1.0f;
                                            totalCorrection.y += dir * overlapY;
                                            joint.velocity.y *= -0.3f;  // Bounce
                                        } else {
                                            float dir = (diff.z >= 0.0f) ? 1.0f : -1.0f;
                                            totalCorrection.z += dir * overlapZ;
                                            joint.velocity.z = 0.0f;
                                        }
                                    }
                                }
                            }
                        }
                        
                        if (glm::length(totalCorrection) > 0.0f) {
                            joint.position += totalCorrection;
                        }
                    }
                }
            }
        }
    }

    // Old constraint function - now just calls simulateRagdolls
    void applyRagdollConstraints(float dt) {
        simulateRagdolls(dt);
    }

    // Ray-sphere intersection test
    bool raySphereHit(const glm::vec3& rayOrigin, const glm::vec3& rayDir, 
                      const glm::vec3& center, float radius, float& tOut) {
        glm::vec3 oc = rayOrigin - center;
        float b = glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = b * b - c;
        
        if (discriminant < 0.0f) return false;
        
        float sqrtDisc = std::sqrt(discriminant);
        float t = -b - sqrtDisc;
        if (t < 0.0f) t = -b + sqrtDisc;
        if (t < 0.0f) return false;
        
        tOut = t;
        return true;
    }

    // Handle attack damage to ragdoll
    void damageRagdollFromRay(RagdollInstance& ragdoll, const glm::vec3& origin, 
                              const glm::vec3& dir, float damage) {
        if (!ragdoll.active) return;
        
        const float maxRange = 12.0f;
        int hitJointIndex = -1;
        float bestT = maxRange;

        LOG_INFO("[RAGDOLL] damageRagdollFromRay: joints=" + std::to_string(ragdoll.joints.size()));

        // Find which joint was hit
        for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
            auto& joint = ragdoll.joints[i];
            if (joint.detached) continue;  // Skip detached joints
            
            float t = 0.0f;
            // Use very large radius for hit detection (5x base radius)
            float hitRadius = joint.radius * 5.0f;
            if (hitRadius < 0.5f) hitRadius = 0.5f;  // Minimum hit radius
            
            if (raySphereHit(origin, dir, joint.position, hitRadius, t)) {
                std::string jointName = (joint.nodeIndex >= 0 && 
                                        static_cast<size_t>(joint.nodeIndex) < ragdoll.nodeNames.size())
                                        ? ragdoll.nodeNames[joint.nodeIndex] : "unknown";
                LOG_INFO("[RAGDOLL] Ray hit " + jointName + " at t=" + std::to_string(t));
                
                if (t < bestT) {
                    bestT = t;
                    hitJointIndex = static_cast<int>(i);
                }
            }
        }

        if (hitJointIndex < 0) {
            LOG_INFO("[RAGDOLL] Ray missed all joints!");
            // Find the nearest joint to the ray for debugging
            float nearestDist = 999.0f;
            std::string nearestName = "none";
            for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
                auto& joint = ragdoll.joints[i];
                if (joint.detached) continue;
                
                // Distance from ray to point
                glm::vec3 toJoint = joint.position - origin;
                float alongRay = glm::dot(toJoint, dir);
                glm::vec3 closest = origin + dir * alongRay;
                float dist = glm::length(joint.position - closest);
                
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearestName = (joint.nodeIndex >= 0 && 
                                  static_cast<size_t>(joint.nodeIndex) < ragdoll.nodeNames.size())
                                  ? ragdoll.nodeNames[joint.nodeIndex] : "unknown";
                }
            }
            LOG_INFO("[RAGDOLL] Nearest joint: " + nearestName + " dist=" + std::to_string(nearestDist));
            return;  // No joint hit
        }

        auto& hitJoint = ragdoll.joints[hitJointIndex];
        
        // Calculate damage to this specific limb
        float dmgToLimb = damage * 1.0f;  // Full damage to the specific limb
        hitJoint.health -= dmgToLimb;

        // Apply small knockback impulse to the hit joint (position-based)
        glm::vec3 impulse = dir * damage * 0.3f;  // Small impulse
        glm::vec3 newVel = hitJoint.velocity + impulse;
        
        // Cap velocity to prevent wild movement
        float speed = glm::length(newVel);
        if (speed > 5.0f) {
            newVel = (newVel / speed) * 5.0f;
        }
        hitJoint.velocity = newVel;

        std::string jointName = (hitJoint.nodeIndex >= 0 && 
                                 static_cast<size_t>(hitJoint.nodeIndex) < ragdoll.nodeNames.size()) 
                                ? ragdoll.nodeNames[hitJoint.nodeIndex] : "unknown";
        
        LOG_INFO("[RAGDOLL] HIT " + jointName + " dmg=" + std::to_string(dmgToLimb) + 
                 " health=" + std::to_string(hitJoint.health) + "/" + std::to_string(hitJoint.maxHealth));

        // Check if limb should detach (never detach torso/core)
        if (hitJoint.health <= 0.0f && !isTorsoCore(jointName)) {
            glm::vec3 entScale = ragdoll.entity ? ragdoll.entity->getScale() : glm::vec3(1.0f);
            float avgScale = (entScale.x + entScale.y + entScale.z) / 3.0f;
            detachJoint(ragdoll, hitJointIndex, avgScale, true);
            LOG_INFO("[RAGDOLL] *** LIMB " + jointName + " DETACHED! ***");
        } else if (hitJoint.health <= 0.0f && isTorsoCore(jointName)) {
            hitJoint.health = 1.0f;
        }
    }

    // Apply explosion damage to all limbs in a ragdoll
    void applyExplosionToRagdoll(Entity* entity, const glm::vec3& center, float radius, float damage, const glm::vec3& knockDir) {
        if (!entity) return;
        
        // Get or create ragdoll for any entity (living or dead)
        RagdollInstance* ragdollPtr = getOrCreateRagdoll(entity);
        if (!ragdollPtr || !ragdollPtr->active) return;
        
        RagdollInstance& ragdoll = *ragdollPtr;
        
        LOG_INFO("[Ragdoll] Explosion hitting ragdoll with " + std::to_string(damage) + " damage");
        
        bool anyAffected = false;

        // Apply damage and knockback to ALL joints based on distance
        for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
            auto& joint = ragdoll.joints[i];
            if (joint.detached) continue;
            
            float distToJoint = glm::length(joint.position - center);
            if (distToJoint > radius * 1.5f) continue;  // Out of range
            
            // Damage falloff based on distance
            float falloff = 1.0f - (distToJoint / (radius * 1.5f));
            falloff = std::max(0.0f, falloff);
            
            // Apply damage to this limb
            float limbDmg = damage * falloff * 2.0f;  // Explosions deal extra limb damage
            joint.health -= limbDmg;
            
            // Apply knockback impulse away from explosion center
            glm::vec3 jointKnockDir = joint.position - center;
            if (glm::length(jointKnockDir) > 0.001f) {
                jointKnockDir = glm::normalize(jointKnockDir);
            } else {
                jointKnockDir = knockDir;
            }
            
            glm::vec3 impulse = jointKnockDir * damage * falloff * 0.8f;
            joint.velocity += impulse;
            
            // Cap velocity
            float speed = glm::length(joint.velocity);
            if (speed > 15.0f) {
                joint.velocity = (joint.velocity / speed) * 15.0f;
            }

            anyAffected = true;
        }

        // Explosions fully disassemble: detach ALL joints if any joint was affected
        if (anyAffected) {
            glm::vec3 entScale = ragdoll.entity ? ragdoll.entity->getScale() : glm::vec3(1.0f);
            float avgScale = (entScale.x + entScale.y + entScale.z) / 3.0f;
            for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
                auto& joint = ragdoll.joints[i];
                if (joint.detached) continue;
                std::string jointName = (joint.nodeIndex >= 0 && 
                                        static_cast<size_t>(joint.nodeIndex) < ragdoll.nodeNames.size())
                                        ? ragdoll.nodeNames[joint.nodeIndex] : "";
                if (isTorsoCore(jointName)) {
                    joint.health = std::max(joint.health, 1.0f);
                    continue;
                }
                joint.health = 0.0f;
                detachJoint(ragdoll, static_cast<int>(i), avgScale, false);
            }
        }
    }

    // Detach a joint from its parent
    void detachJoint(RagdollInstance& ragdoll, int jointIndex, float debrisScale = 1.0f, bool detachChildren = true) {
        if (jointIndex < 0 || static_cast<size_t>(jointIndex) >= ragdoll.joints.size()) return;

        auto& joint = ragdoll.joints[jointIndex];
        if (joint.detached) return;  // Already detached
        
        joint.detached = true;

        // Deactivate all constraints involving this joint
        for (auto& c : ragdoll.constraints) {
            if (c.jointA == jointIndex || c.jointB == jointIndex) {
                c.active = false;
            }
        }

        // Give the detached limb a small kick
        glm::vec3 kick = glm::vec3(
            (std::rand() % 100 - 50) / 50.0f,
            2.0f,
            (std::rand() % 100 - 50) / 50.0f
        );
        glm::vec3 limbVelocity = joint.velocity + kick;
        
        // Get joint name for logging and debris
        std::string jointName = (joint.nodeIndex >= 0 && 
                                static_cast<size_t>(joint.nodeIndex) < ragdoll.nodeNames.size())
                                ? ragdoll.nodeNames[joint.nodeIndex] : "unknown";
        
        // Spawn limb debris FIRST (before hiding nodes, so the clone gets unhidden state)
        if (ragdoll.model && joint.nodeIndex >= 0) {
            spawnLimbDebrisEntity(ragdoll.model, joint.nodeIndex, jointName, joint.position, limbVelocity, debrisScale);
        }
        
        // THEN hide the node in the original model so it doesn't render
        if (ragdoll.model && joint.nodeIndex >= 0) {
            ragdoll.model->setNodeHidden(joint.nodeIndex, true);
            LOG_INFO("[Ragdoll] Hidden node " + std::to_string(joint.nodeIndex) + " (" + jointName + ") in model");
        }
        
        LOG_INFO("[Ragdoll] Detached limb: " + jointName);

        if (detachChildren) {
            // Also detach any children of this joint (but don't spawn separate debris for them - they're part of parent limb)
            for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
                if (ragdoll.joints[i].parentJointIndex == jointIndex) {
                    // Just mark as detached and hide, don't spawn debris
                    auto& childJoint = ragdoll.joints[i];
                    if (!childJoint.detached) {
                        childJoint.detached = true;
                        if (ragdoll.model && childJoint.nodeIndex >= 0) {
                            ragdoll.model->setNodeHidden(childJoint.nodeIndex, true);
                        }
                        // Recursively handle grandchildren
                        for (size_t j = 0; j < ragdoll.joints.size(); ++j) {
                            if (ragdoll.joints[j].parentJointIndex == static_cast<int>(i)) {
                                detachJointChildOnly(ragdoll, static_cast<int>(j));
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Helper: detach a child joint without spawning debris (it's part of parent limb debris)
    void detachJointChildOnly(RagdollInstance& ragdoll, int jointIndex) {
        if (jointIndex < 0 || static_cast<size_t>(jointIndex) >= ragdoll.joints.size()) return;
        
        auto& joint = ragdoll.joints[jointIndex];
        if (joint.detached) return;
        
        joint.detached = true;
        
        // Deactivate constraints
        for (auto& c : ragdoll.constraints) {
            if (c.jointA == jointIndex || c.jointB == jointIndex) {
                c.active = false;
            }
        }
        
        // Hide the node
        if (ragdoll.model && joint.nodeIndex >= 0) {
            ragdoll.model->setNodeHidden(joint.nodeIndex, true);
        }
        
        // Recursively detach children
        for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
            if (ragdoll.joints[i].parentJointIndex == jointIndex) {
                detachJointChildOnly(ragdoll, static_cast<int>(i));
            }
        }
    }
    
    // Spawn a LimbDebrisEntity for a detached limb
    void spawnLimbDebrisEntity(std::shared_ptr<ModelSystem::Model> sourceModel, 
                               int limbNodeIndex,
                               const std::string& limbName,
                               const glm::vec3& position,
                               const glm::vec3& velocity,
                               float scale) {
        if (!sourceModel) {
            LOG_ERROR("[Ragdoll] spawnLimbDebrisEntity: sourceModel is null!");
            return;
        }
        
        LOG_INFO("[Ragdoll] spawnLimbDebrisEntity: Creating debris for " + limbName + 
                 " nodeIndex=" + std::to_string(limbNodeIndex) +
                 " pos=" + std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z));
        
        // Random angular velocity for spinning debris
        glm::vec3 angularVel(
            (std::rand() % 100 - 50) / 10.0f,
            (std::rand() % 100 - 50) / 10.0f,
            (std::rand() % 100 - 50) / 10.0f
        );
        
        auto debris = std::make_unique<LimbDebrisEntity>(
            position,
            sourceModel,
            limbNodeIndex,
            limbName,
            velocity,
            angularVel,
            scale
        );
        
        if (debris) {
            // Terrain collision uses world blocks
            if (chunkManager) {
                debris->setTerrainQuery([this](int x, int y, int z) -> Block {
                    if (chunkManager) {
                        return chunkManager->getBlockAt(x, y, z);
                    }
                    return Block(BlockType::AIR);
                });
            }
            limbDebris.push_back(std::move(debris));
            LOG_INFO("[Ragdoll] Spawned limb debris entity for " + limbName + ", total limbDebris=" + std::to_string(limbDebris.size()));
        } else {
            LOG_ERROR("[Ragdoll] Failed to create LimbDebrisEntity for " + limbName);
        }
    }
    
    // Old block debris spawner (kept for reference, not used)
    void spawnLimbDebris(const glm::vec3& pos, const glm::vec3& vel, const std::string& limbName) {
        // Use red wool for limb debris (looks like flesh)
        BlockType debrisType = BlockType::RED_WOOL;
        
        // Determine debris size based on limb type
        float scale = 0.3f;
        std::string lower = toLowerCopy(limbName);
        if (containsAny(lower, {"head"})) {
            scale = 0.4f;
            debrisType = BlockType::WHITE_WOOL;  // Skull/bone color
        } else if (containsAny(lower, {"body", "torso", "chest"})) {
            scale = 0.5f;
        } else if (containsAny(lower, {"arm", "leg"})) {
            scale = 0.35f;
        } else if (containsAny(lower, {"hand", "foot"})) {
            scale = 0.25f;
        }
        
        // Add some randomness to velocity
        glm::vec3 debrisVel = vel + glm::vec3(
            (std::rand() % 100 - 50) / 25.0f,
            3.0f + (std::rand() % 100) / 50.0f,
            (std::rand() % 100 - 50) / 25.0f
        );
        
        glm::vec3 angVel(
            (std::rand() % 100 - 50) / 10.0f,
            (std::rand() % 100 - 50) / 10.0f,
            (std::rand() % 100 - 50) / 10.0f
        );
        
        createDebrisInternal(pos, debrisVel, angVel, debrisType, scale);
        LOG_INFO("[Ragdoll] Spawned limb debris for " + limbName + " at " + 
                 std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z));
    }

    void resolveRagdollWorldCollision(RagdollInstance& ragdoll) {
        if (!chunkManager) return;

        for (auto& joint : ragdoll.joints) {
            glm::vec3 pos = joint.position;
            float radius = joint.radius;

            glm::vec3 half(radius);
            glm::vec3 aabbMin = pos - half;
            glm::vec3 aabbMax = pos + half;

            glm::ivec3 minB = glm::ivec3(glm::floor(aabbMin));
            glm::ivec3 maxB = glm::ivec3(glm::floor(aabbMax));

            glm::vec3 totalCorrection(0.0f);
            glm::vec3 vel = joint.velocity;

            for (int x = minB.x; x <= maxB.x; ++x) {
                for (int y = minB.y; y <= maxB.y; ++y) {
                    for (int z = minB.z; z <= maxB.z; ++z) {
                        Block block = chunkManager->getBlockAt(x, y, z);
                        if (!block.isSolid()) continue;

                        glm::vec3 blockMin(x, y, z);
                        glm::vec3 blockMax(x + 1.0f, y + 1.0f, z + 1.0f);

                        float overlapX = std::min(aabbMax.x, blockMax.x) - std::max(aabbMin.x, blockMin.x);
                        float overlapY = std::min(aabbMax.y, blockMax.y) - std::max(aabbMin.y, blockMin.y);
                        float overlapZ = std::min(aabbMax.z, blockMax.z) - std::max(aabbMin.z, blockMin.z);

                        if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
                            glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
                            glm::vec3 blockCenter = (blockMin + blockMax) * 0.5f;
                            glm::vec3 diff = center - blockCenter;

                            if (overlapX < overlapY && overlapX < overlapZ) {
                                float dir = (diff.x >= 0.0f) ? 1.0f : -1.0f;
                                totalCorrection.x += dir * overlapX;
                                vel.x = 0.0f;
                            } else if (overlapY < overlapZ) {
                                float dir = (diff.y >= 0.0f) ? 1.0f : -1.0f;
                                totalCorrection.y += dir * overlapY;
                                vel.y = 0.0f;
                            } else {
                                float dir = (diff.z >= 0.0f) ? 1.0f : -1.0f;
                                totalCorrection.z += dir * overlapZ;
                                vel.z = 0.0f;
                            }
                        }
                    }
                }
            }

            if (glm::length(totalCorrection) > 0.0f) {
                joint.position = pos + totalCorrection;
                joint.velocity = vel;
            }
        }
    }

    void updateRagdollPose() {
        if (ragdolls.empty()) return;

        std::vector<Entity*> toRemove;

        for (auto& [entity, ragdoll] : ragdolls) {
            if (!ragdoll.active || !ragdoll.entity || !ragdoll.model) {
                toRemove.push_back(entity);
                continue;
            }

            // Check if entity still exists
            if (entityProvider) {
                bool stillExists = false;
                auto entities = entityProvider();
                for (auto* e : entities) {
                    if (e == ragdoll.entity) { stillExists = true; break; }
                }
                if (!stillExists) {
                    toRemove.push_back(entity);
                    continue;
                }
            }

            bool entityIsDead = ragdoll.entity->isDead();
            
            if (!entityIsDead) {
                // LIVING entity - only render detached limbs, attached follow normal animation
                // Check if any limbs are detached
                bool hasDetached = false;
                for (const auto& joint : ragdoll.joints) {
                    if (joint.detached) { hasDetached = true; break; }
                }
                
                if (!hasDetached) {
                    // No detached limbs - ragdoll not needed for rendering
                    continue;
                }
                
                // Has detached limbs - need to hide the detached parts in the model
                // For now, we just skip pose updates for living entities
                // The detached limbs will be rendered as debris particles
                continue;
            }
            
            // DEAD entity - full ragdoll pose
            ragdoll.model->setExternalPoseEnabled(true);
            ragdoll.model->stopAnimation();
            
            // Stop entity movement
            ragdoll.entity->setVelocity(glm::vec3(0.0f));
            
            // Get current entity transform
            glm::vec3 entPos = ragdoll.entity->getPosition();
            glm::vec3 entRot = ragdoll.entity->getRotation();
            glm::vec3 entScale = ragdoll.entity->getScale();

            // Build entity world transform (WITHOUT scale for physics calculations)
            glm::mat4 entityWorldNoScale = buildEntityTransform(entPos, entRot, glm::vec3(1.0f));
            glm::mat4 invEntityWorldNoScale = glm::inverse(entityWorldNoScale);

            // Calculate model-space transforms for all ragdoll joints
            std::unordered_map<int, std::pair<glm::vec3, glm::quat>> nodeModelSpacePose;
            nodeModelSpacePose.reserve(ragdoll.joints.size());

            for (const auto& joint : ragdoll.joints) {
                if (joint.detached) continue;  // Skip detached joints
                
                // Get world transform from our position-based simulation
                glm::vec3 worldPos = joint.position;
                glm::quat worldRot = joint.rotation;
                
                // Convert to model space (without entity scale)
                glm::vec4 modelPosH = invEntityWorldNoScale * glm::vec4(worldPos, 1.0f);
                glm::vec3 modelPos = glm::vec3(modelPosH);
                
                // Convert rotation to model space
                glm::quat entityRot = glm::quat(glm::radians(entRot));
                glm::quat invEntityRot = glm::inverse(entityRot);
                glm::quat modelRot = invEntityRot * worldRot;
                
                nodeModelSpacePose[joint.nodeIndex] = {modelPos, modelRot};
            }

            // Apply local transforms to each node
            for (const auto& joint : ragdoll.joints) {
                if (joint.detached) continue;  // Skip detached joints
                
                auto it = nodeModelSpacePose.find(joint.nodeIndex);
                if (it == nodeModelSpacePose.end()) continue;

                glm::vec3 modelPos = it->second.first;
                glm::quat modelRot = it->second.second;

                // Get parent's model-space pose
                glm::vec3 parentModelPos(0.0f);
                glm::quat parentModelRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                
                int parentNodeIndex = ragdoll.model->getParentIndex(joint.nodeIndex);
                
                if (parentNodeIndex >= 0) {
                    auto parentIt = nodeModelSpacePose.find(parentNodeIndex);
                    if (parentIt != nodeModelSpacePose.end()) {
                        parentModelPos = parentIt->second.first;
                        parentModelRot = parentIt->second.second;
                    } else {
                        glm::mat4 parentGlobalMat = ragdoll.model->getNodeGlobalTransformByIndex(parentNodeIndex);
                        parentModelPos = glm::vec3(parentGlobalMat[3]);
                        parentModelRot = glm::quat_cast(parentGlobalMat);
                    }
                }

                // Calculate local transform
                glm::quat invParentRot = glm::inverse(parentModelRot);
                glm::vec3 localPos = invParentRot * (modelPos - parentModelPos);
                glm::quat localRot = invParentRot * modelRot;
                
                // Build local transform matrix preserving scale = 1
                glm::vec3 localScale(1.0f);
                
                glm::mat4 localMat = glm::translate(glm::mat4(1.0f), localPos) * 
                                      glm::mat4_cast(localRot) *
                                      glm::scale(glm::mat4(1.0f), localScale);
                
                ragdoll.model->setNodeOverrideLocalTransform(joint.nodeIndex, localMat);
            }
        }

        for (auto* e : toRemove) {
            clearRagdoll(e);
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
    
    // Limb debris (detached body parts)
    std::vector<std::unique_ptr<LimbDebrisEntity>> limbDebris;
};

} // namespace Physics

#else

#include <glm/glm.hpp>

namespace Physics {
class Entity;
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
    void handleAttackHit(Entity*, const glm::vec3&, const glm::vec3&, float) {}
};
} // namespace Physics

#endif
