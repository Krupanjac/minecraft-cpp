#pragma once

#include "Entity.h"
#include <random>
#include <string>
#include <vector>
#include <memory>

namespace ModelSystem { class Model; }
class ChunkManager;

using EntityId = uint32_t;

class SkeletonEntity : public Entity {
public:
    // Original constructor (loads model internally - causes stutter)
    SkeletonEntity(const glm::vec3& startPos);
    
    // New constructor with pre-loaded model (no stutter)
    SkeletonEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id = 0);
    
    ~SkeletonEntity() override = default;

    // Update AI + animation. Returns true if an "attack" happened this frame.
    bool updateAI(float deltaTime, ChunkManager& chunkManager, const glm::vec3& playerPos);

    glm::vec3 consumeAttackImpulse();
    bool isDead() const { return dead; }
    void takeDamage(float amount);
    float getHealth() const { return health; }
    
    // Entity ID for network sync
    EntityId getEntityId() const { return entityId; }

private:
    EntityId entityId = 0;
    
    void initializeCommon(const glm::vec3& startPos);
    
    enum class State { Idle, Wander, Chase };
    State state = State::Idle;

    std::mt19937 rng;
    float stateTimer = 0.0f;
    float attackCooldown = 0.0f;
    bool onGround = false;
    bool dead = false;
    float health = 20.0f;

    glm::vec3 desiredDir = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 attackImpulse = glm::vec3(0.0f);
    glm::vec3 rotationOffset = glm::vec3(0.0f);

    std::string idleAnim;
    std::string walkAnim;
    std::string attackAnim;

    // Simple pathfinding
    float pathReplanTimer = 0.0f;
    std::vector<glm::vec3> pathPoints;
    size_t pathIndex = 0;

    void pickAnimations();
    void setState(State s, float minTime, float maxTime);
    void chooseRandomWanderDir();
};
