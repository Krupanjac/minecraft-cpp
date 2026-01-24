#pragma once

#include "Entity.h"
#include "../Audio/AudioManager.h"
#include <random>
#include <string>
#include <memory>

namespace ModelSystem { class Model; }
class ChunkManager;

using EntityId = uint32_t;

// Base class for passive mobs (pigs, chickens, sheep, etc.)
class PassiveMob : public Entity {
public:
    PassiveMob(const glm::vec3& startPos);
    PassiveMob(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id = 0);
    ~PassiveMob() override = default;

    // Update AI behavior
    virtual void updateAI(float deltaTime, ChunkManager& chunkManager);

    // Check if mob is dead
    bool isDead() const { return dead; }
    float getDeathFadeAlpha() const override { return deathFadeAlpha; }
    bool shouldBeRemoved() const { return dead && deathTimer >= (DEATH_STAY_TIME + DEATH_FADE_TIME); }
    
    // Apply damage
    void takeDamage(float amount, const glm::vec3& knockbackDir = glm::vec3(0.0f)) override;
    
    // Play death animation
    void playDeathAnimation();
    
    // Entity ID for network sync
    EntityId getEntityId() const { return entityId; }

protected:
    EntityId entityId = 0;
    
    enum class State { Idle, Wander, Flee };
    State state = State::Idle;

    std::mt19937 rng;
    float stateTimer = 0.0f;
    bool onGround = false;
    bool dead = false;
    float health = 10.0f;
    float maxHealth = 10.0f;
    
    // Death fade-out
    float deathTimer = 0.0f;
    float deathFadeAlpha = 1.0f;
    static constexpr float DEATH_STAY_TIME = 2.0f;
    static constexpr float DEATH_FADE_TIME = 1.5f;
    
    float moveSpeed = 2.0f;
    float fleeSpeed = 4.0f;

    glm::vec3 desiredDir = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 rotationOffset = glm::vec3(0.0f);

    std::string idleAnim;
    std::string walkAnim;
    std::string runAnim;
    std::string deathAnim;
    std::string idleEatingAnim;  // Idle_Eating for pig/sheep
    std::string headbuttAnim;    // Headbutt for pig/sheep
    std::string jumpStartAnim;   // Jump_Start for pig/sheep
    std::string jumpLoopAnim;    // Jump_Loop for pig/sheep
    std::string attackAnim;      // Attack for chicken
    std::string idlePeckAnim;    // Idle_Peck for chicken
    
    // Animation state
    bool isDeathPlaying = false;
    bool isIdleEating = false;
    float idleEatingTimer = 0.0f;
    
    // Sound system
    float soundTimer = 0.0f;
    float nextSoundTime = 5.0f;

    // To be implemented by subclasses
    virtual void loadModel() = 0;
    virtual void pickAnimations();
    virtual Audio::SoundType getAmbientSound() const { return Audio::SoundType::NONE; }
    virtual Audio::SoundType getHurtSound() const { return Audio::SoundType::NONE; }
    virtual Audio::SoundType getDeathSound() const { return Audio::SoundType::NONE; }
    
    void setState(State s, float minTime, float maxTime);
    void chooseRandomWanderDir();
    bool checkCollision(ChunkManager& chunkManager, const glm::vec3& feetPos);
};
