#pragma once

#include "Entity.h"
#include <random>
#include <string>

class ChunkManager;

// Base class for passive mobs (pigs, chickens, sheep, etc.)
class PassiveMob : public Entity {
public:
    PassiveMob(const glm::vec3& startPos);
    ~PassiveMob() override = default;

    // Update AI behavior
    virtual void updateAI(float deltaTime, ChunkManager& chunkManager);

    // Check if mob is dead
    bool isDead() const { return dead; }
    
    // Apply damage
    void takeDamage(float amount);

protected:
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

    // To be implemented by subclasses
    virtual void loadModel() = 0;
    virtual void pickAnimations();
    
    void setState(State s, float minTime, float maxTime);
    void chooseRandomWanderDir();
    bool checkCollision(ChunkManager& chunkManager, const glm::vec3& feetPos);
};
