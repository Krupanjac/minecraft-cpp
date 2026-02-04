#pragma once

#include "Entity.h"
#include "../World/Block.h"
#include "../Physics/RigidBody.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

/**
 * DebrisEntity - Physics-simulated block fragment
 * 
 * Created when blocks are destroyed, these entities simulate
 * physical debris with:
 * - Rigid body physics (collision, gravity, rotation)
 * - Variable scale based on fracture pattern
 * - Lifetime with fade out
 * - Block texture for rendering
 * - Optional collision with terrain
 */
class DebrisEntity : public Entity {
public:
    // Configuration for debris behavior
    struct Config {
        float lifetime = 5.0f;           // Base lifetime in seconds
        float fadeTime = 1.0f;           // Fade out duration
        float bounceRestitution = 0.3f;  // Bounciness on collision
        float friction = 0.8f;           // Ground friction
        float linearDamping = 0.1f;      // Air resistance (linear)
        float angularDamping = 0.3f;     // Air resistance (angular)
        float waterLinearDamping = 1.8f; // Water resistance (linear)
        float waterAngularDamping = 1.9f; // Water resistance (angular)
        float waterBuoyancy = 1.15f;     // Buoyancy factor (0 = none, 1 = neutral)
        float waterFlowStrength = 4.0f;  // Strength of flow drift
        float minVelocityToRest = 0.1f;  // Velocity threshold to stop physics
        bool collideWithTerrain = true;  // Enable terrain collision
        bool collideWithEntities = false; // Enable entity collision (expensive)
    };

    DebrisEntity(const glm::vec3& position,
                 BlockType blockType,
                 const glm::vec3& initialVelocity = glm::vec3(0.0f),
                 const glm::vec3& initialAngularVelocity = glm::vec3(0.0f),
                 float scale = 0.5f);
    
    ~DebrisEntity() override = default;

    // Entity interface
    void update(float deltaTime) override;
    void render(Shader& shader) override;
    
    // Physics update - separate from regular update for fixed timestep
    void physicsUpdate(float fixedDeltaTime);
    
    // Setters
    void setLinearVelocity(const glm::vec3& vel);
    void setAngularVelocity(const glm::vec3& angVel);
    void applyImpulse(const glm::vec3& impulse);
    void applyTorqueImpulse(const glm::vec3& torque);
    
    // Getters
    const glm::vec3& getLinearVelocity() const { return linearVelocity; }
    const glm::vec3& getAngularVelocity() const { return angularVelocity; }
    const glm::quat& getOrientationQuat() const { return orientationQuat; }
    BlockType getBlockType() const { return blockType; }
    float getDebrisScale() const { return debrisScale; }
    
    // Lifetime
    float getLifetime() const { return lifetime; }
    float getRemainingLife() const { return lifetime - age; }
    float getFadeAlpha() const;
    bool isExpired() const { return age >= lifetime; }
    bool isAtRest() const { return atRest; }
    bool isInWater() const { return inWater; }
    float getWaterSubmersion() const { return waterSubmersion; }
    
    // Physics state
    Physics::AABB getAABB() const;
    
    // Configuration
    void setConfig(const Config& cfg) { 
        config = cfg; 
        lifetime = cfg.lifetime;  // Update lifetime member from config
    }
    const Config& getConfig() const { return config; }
    
    // Terrain collision callback
    using TerrainQueryFunc = std::function<Block(int, int, int)>;
    void setTerrainQuery(TerrainQueryFunc func) { terrainQuery = func; }

private:
    // Block data
    BlockType blockType;
    float debrisScale;
    
    // Physics state
    glm::vec3 linearVelocity;
    glm::vec3 angularVelocity;
    glm::quat orientationQuat;
    glm::vec3 prevLinearVelocity;
    
    // Inertia tensor (approximated as uniform box)
    glm::mat3 inertiaTensor;
    glm::mat3 inverseInertiaTensor;
    float mass;
    
    // Lifetime tracking
    float lifetime;
    float age;
    bool atRest;
    int restFrames; // Count consecutive frames at rest

    // Water state
    bool inWater;
    float waterSubmersion;
    
    // Configuration
    Config config;
    
    // Terrain query for collision
    TerrainQueryFunc terrainQuery;
    
    // Rendering
    glm::mat4 cachedModelMatrix;
    bool modelMatrixDirty;
    
    // Physics helpers
    void integratePhysics(float dt);
    void applyGravity(float dt);
    void applyDamping(float dt);
    void applyWaterFlow(float dt);
    void handleTerrainCollision(float dt);
    void checkRestState();
    void updateModelMatrix();
    void updateWaterState();
    
    // Collision detection
    bool checkBoxTerrainCollision(const glm::vec3& pos, float halfSize, glm::vec3& normal, float& penetration);
};

/**
 * DebrisManager - Handles creation and pooling of debris entities
 */
class DebrisManager {
public:
    struct Stats {
        size_t activeDebris = 0;
        size_t pooledDebris = 0;
        size_t totalCreated = 0;
        size_t totalRecycled = 0;
    };

    DebrisManager(size_t maxDebris = 500);
    ~DebrisManager() = default;
    
    // Create new debris
    DebrisEntity* createDebris(const glm::vec3& position,
                               BlockType blockType,
                               const glm::vec3& velocity = glm::vec3(0.0f),
                               const glm::vec3& angularVelocity = glm::vec3(0.0f),
                               float scale = 0.5f);
    
    // Update all debris
    void update(float deltaTime);
    
    // Physics update (fixed timestep)
    void physicsUpdate(float fixedDeltaTime);
    
    // Render all debris
    void render(Shader& shader);
    
    // Remove all debris
    void clear();
    
    // Get active debris for iteration
    const std::vector<std::unique_ptr<DebrisEntity>>& getActiveDebris() const { return activeDebris; }
    std::vector<std::unique_ptr<DebrisEntity>>& getActiveDebris() { return activeDebris; }
    
    // Stats
    const Stats& getStats() const { return stats; }
    
    // Configuration
    void setMaxDebris(size_t max) { maxDebris = max; }
    size_t getMaxDebris() const { return maxDebris; }
    
    void setDefaultConfig(const DebrisEntity::Config& cfg) { defaultConfig = cfg; }
    const DebrisEntity::Config& getDefaultConfig() const { return defaultConfig; }
    
    // Terrain query for all debris
    void setTerrainQuery(DebrisEntity::TerrainQueryFunc func) { terrainQuery = func; }

private:
    std::vector<std::unique_ptr<DebrisEntity>> activeDebris;
    std::vector<std::unique_ptr<DebrisEntity>> debrisPool;
    
    size_t maxDebris;
    Stats stats;
    DebrisEntity::Config defaultConfig;
    DebrisEntity::TerrainQueryFunc terrainQuery;
    
    // Get debris from pool or create new
    DebrisEntity* getFromPool();
    void returnToPool(std::unique_ptr<DebrisEntity> debris);
};
