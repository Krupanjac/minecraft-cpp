#pragma once

#include "Entity.h"
#include "../Model/Model.h"
#include "../World/Block.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <memory>
#include <string>

/**
 * LimbDebrisEntity - Physics-simulated detached limb
 * 
 * Created when limbs are detached from entities via the ragdoll system.
 * Renders a portion of the original GLTF model (just the detached limb)
 * with physics simulation.
 */
class LimbDebrisEntity : public Entity {
public:
    struct Config {
        float lifetime = 5.0f;           // Base lifetime in seconds
        float fadeTime = 1.0f;           // Fade out duration
        float bounceRestitution = 0.3f;  // Bounciness on collision
        float friction = 0.8f;           // Ground friction
        float linearDamping = 0.1f;      // Air resistance (linear)
        float angularDamping = 0.3f;     // Air resistance (angular)
        float gravity = 20.0f;           // Gravity acceleration
    };

    LimbDebrisEntity(const glm::vec3& position,
                     std::shared_ptr<ModelSystem::Model> sourceModel,
                     int limbNodeIndex,
                     const std::string& limbName,
                     const glm::vec3& initialVelocity = glm::vec3(0.0f),
                     const glm::vec3& initialAngularVelocity = glm::vec3(0.0f),
                     float scale = 1.0f);
    
    ~LimbDebrisEntity() override = default;

    // Entity interface
    void update(float deltaTime) override;
    void render(Shader& shader) override;
    void renderWithOrigin(Shader& shader, const glm::vec3& renderOrigin);
    void physicsUpdate(float fixedDeltaTime);
    
    // Setters
    void setLinearVelocity(const glm::vec3& vel) { linearVelocity = vel; }
    void setAngularVelocity(const glm::vec3& angVel) { angularVelocity = angVel; }
    
    // Getters
    const glm::vec3& getLinearVelocity() const { return linearVelocity; }
    const glm::vec3& getAngularVelocity() const { return angularVelocity; }
    const glm::quat& getOrientationQuat() const { return orientationQuat; }
    const std::string& getLimbName() const { return limbName; }
    
    // Lifetime
    float getLifetime() const { return lifetime; }
    float getRemainingLife() const { return lifetime - age; }
    float getFadeAlpha() const;
    bool isExpired() const { return age >= lifetime; }
    
    // Configuration
    static Config& getConfig() { return config; }

    // Terrain collision support
    void setTerrainQuery(std::function<Block(int, int, int)> query) { terrainQuery = std::move(query); }

private:
    std::shared_ptr<ModelSystem::Model> limbModel;  // Cloned model showing only this limb
    int limbNodeIndex;
    std::string limbName;
    
    // Physics state
    glm::vec3 linearVelocity;
    glm::vec3 angularVelocity;
    glm::quat orientationQuat;
    float debrisScale;
    
    // Lifetime
    float lifetime;
    float age = 0.0f;

    // Terrain query for collision
    std::function<Block(int, int, int)> terrainQuery;
    
    static Config config;
    
    void setupLimbModel(std::shared_ptr<ModelSystem::Model> sourceModel, int nodeIndex);
};
