#pragma once

#include "Entity.h"
#include <random>
#include <string>
#include <vector>
#include <memory>

namespace ModelSystem { class Model; }
class ChunkManager;

using EntityId = uint32_t;

class ZombieEntity : public Entity {
public:
    // Original constructor (loads model internally - causes stutter)
    ZombieEntity(const glm::vec3& startPos);
    
    // New constructor with pre-loaded model (no stutter)
    ZombieEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id = 0);
    
    ~ZombieEntity() override = default;

    // Update AI + animation. Returns true if an "attack" happened this frame.
    bool updateAI(float deltaTime, ChunkManager& chunkManager, const glm::vec3& playerPos);

    glm::vec3 consumeAttackImpulse(); // impulse applied to player (knockback), cleared after reading
    
    // Health and death
    bool isDead() const { return dead; }
    void takeDamage(float amount, const glm::vec3& knockbackDir = glm::vec3(0.0f)) override;
    float getHealth() const { return health; }
    float getDeathFadeAlpha() const override { return deathFadeAlpha; }
    bool shouldBeRemoved() const { return dead && deathTimer >= (DEATH_STAY_TIME + DEATH_FADE_TIME); }
    
    // Play specific animations
    void playAttackAnimation();
    void playDeathAnimation();
    void playHitReceiveAnimation();
    
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

    glm::vec3 desiredDir = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 attackImpulse = glm::vec3(0.0f);
    glm::vec3 rotationOffset = glm::vec3(0.0f); // degrees: model axis fix (pitch/yaw/roll)

    std::string idleAnim;
    std::string walkAnim;
    std::string runAnim;
    std::string attackAnim;
    std::string deathAnim;
    std::string hitReceiveAnim;
    std::string jumpAnim;
    
    // Animation state
    bool isAttacking = false;
    float attackAnimTimer = 0.0f;
    bool dead = false;
    float health = 20.0f;
    bool isHitReacting = false;
    float hitReactTimer = 0.0f;
    
    // Death fade-out
    float deathTimer = 0.0f;
    float deathFadeAlpha = 1.0f;
    static constexpr float DEATH_STAY_TIME = 2.0f;   // Stay visible for 2 seconds
    static constexpr float DEATH_FADE_TIME = 1.5f;   // Fade out over 1.5 seconds

    // Simple pathfinding (2D A* on blocks)
    float pathReplanTimer = 0.0f;
    std::vector<glm::vec3> pathPoints; // feet positions to follow
    size_t pathIndex = 0;

    void pickAnimations();
    void setState(State s, float minTime, float maxTime);
    void chooseRandomWanderDir();
};


