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
    
    // Apply damage
    void takeDamage(float amount);
    
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
    
    float moveSpeed = 2.0f;
    float fleeSpeed = 4.0f;

    glm::vec3 desiredDir = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 rotationOffset = glm::vec3(0.0f);

    std::string idleAnim;
    std::string walkAnim;
    
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
