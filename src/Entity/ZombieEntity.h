#pragma once

#include "Entity.h"
#include <random>
#include <string>
#include <vector>

class ChunkManager;

class ZombieEntity : public Entity {
public:
    ZombieEntity(const glm::vec3& startPos);
    ~ZombieEntity() override = default;

    // Update AI + animation. Returns true if an "attack" happened this frame.
    bool updateAI(float deltaTime, ChunkManager& chunkManager, const glm::vec3& playerPos);

    glm::vec3 consumeAttackImpulse(); // impulse applied to player (knockback), cleared after reading

private:
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

    // Simple pathfinding (2D A* on blocks)
    float pathReplanTimer = 0.0f;
    std::vector<glm::vec3> pathPoints; // feet positions to follow
    size_t pathIndex = 0;

    void pickAnimations();
    void setState(State s, float minTime, float maxTime);
    void chooseRandomWanderDir();
};


