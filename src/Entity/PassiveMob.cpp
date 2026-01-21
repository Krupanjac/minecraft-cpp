#include "PassiveMob.h"

#include "../Core/Logger.h"
#include "../Model/Model.h"
#include "../World/ChunkManager.h"

#include <algorithm>
#include <cctype>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string pickAnimByKeywords(const std::vector<std::string>& names, std::initializer_list<const char*> keys) {
    for (const auto& n : names) {
        std::string ln = toLower(n);
        for (auto* k : keys) {
            if (ln.find(k) != std::string::npos) return n;
        }
    }
    return "";
}

PassiveMob::PassiveMob(const glm::vec3& startPos) : Entity(startPos) {
    // Seed RNG from position
    unsigned int seed = 1337u
        ^ (unsigned int)(std::abs((int)startPos.x) * 73856093)
        ^ (unsigned int)(std::abs((int)startPos.z) * 19349663);
    rng.seed(seed);
}

void PassiveMob::pickAnimations() {
    if (!model) return;
    auto names = model->getAnimationNames();
    if (names.empty()) {
        LOG_WARNING("PassiveMob: no animations found");
        return;
    }

    idleAnim = pickAnimByKeywords(names, {"idle"});
    walkAnim = pickAnimByKeywords(names, {"walk", "run"});

    if (idleAnim.empty()) idleAnim = names[0];
    if (walkAnim.empty()) walkAnim = (names.size() > 1) ? names[1] : names[0];

    LOG_INFO("PassiveMob animations: idle='" + idleAnim + "' walk='" + walkAnim + "'");
    model->playAnimation(idleAnim, true);
}

void PassiveMob::setState(State s, float minTime, float maxTime) {
    state = s;
    std::uniform_real_distribution<float> dis(minTime, maxTime);
    stateTimer = dis(rng);
    if (state == State::Wander) chooseRandomWanderDir();
}

void PassiveMob::chooseRandomWanderDir() {
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    float a = dis(rng) * 6.2831853f;
    desiredDir = glm::normalize(glm::vec3(std::cos(a), 0.0f, std::sin(a)));
}

bool PassiveMob::checkCollision(ChunkManager& chunkManager, const glm::vec3& feetPos) {
    constexpr float HALF_W = 0.30f;
    constexpr float HEIGHT = 1.0f;

    float minX = feetPos.x - HALF_W;
    float maxX = feetPos.x + HALF_W;
    float minY = feetPos.y;
    float maxY = feetPos.y + HEIGHT;
    float minZ = feetPos.z - HALF_W;
    float maxZ = feetPos.z + HALF_W;

    for (int x = (int)std::floor(minX); x <= (int)std::floor(maxX); ++x) {
        for (int y = (int)std::floor(minY); y <= (int)std::floor(maxY); ++y) {
            for (int z = (int)std::floor(minZ); z <= (int)std::floor(maxZ); ++z) {
                Block b = chunkManager.getBlockAt(x, y, z);
                if (b.isSolid()) return true;
            }
        }
    }
    return false;
}

void PassiveMob::takeDamage(float amount) {
    health -= amount;
    if (health <= 0.0f) {
        dead = true;
        health = 0.0f;
    }
}

void PassiveMob::updateAI(float deltaTime, ChunkManager& chunkManager) {
    if (dead) return;

    // Update animation
    if (model) {
        model->updateAnimation(deltaTime);
    }

    // Gravity
    velocity.y -= 25.0f * deltaTime;
    velocity.y = std::max(velocity.y, -50.0f);

    // Check ground
    glm::vec3 feetCheck = position + glm::vec3(0.0f, velocity.y * deltaTime - 0.1f, 0.0f);
    if (checkCollision(chunkManager, feetCheck)) {
        velocity.y = 0.0f;
        onGround = true;
    } else {
        onGround = false;
    }

    // State machine
    stateTimer -= deltaTime;
    if (stateTimer <= 0.0f) {
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        float r = dis(rng);
        if (state == State::Idle) {
            if (r < 0.6f) {
                setState(State::Wander, 2.0f, 5.0f);
                if (model) model->playAnimation(walkAnim, true);
            } else {
                setState(State::Idle, 1.0f, 3.0f);
            }
        } else if (state == State::Wander) {
            if (r < 0.5f) {
                setState(State::Idle, 2.0f, 4.0f);
                if (model) model->playAnimation(idleAnim, true);
            } else {
                setState(State::Wander, 2.0f, 5.0f);
            }
        } else if (state == State::Flee) {
            setState(State::Idle, 1.0f, 2.0f);
            if (model) model->playAnimation(idleAnim, true);
        }
    }

    // Movement
    glm::vec3 moveVec(0.0f);
    float speed = (state == State::Flee) ? fleeSpeed : moveSpeed;

    if (state == State::Wander || state == State::Flee) {
        moveVec = desiredDir * speed * deltaTime;
    }

    // Try to move
    if (glm::length(glm::vec2(moveVec.x, moveVec.z)) > 0.001f) {
        glm::vec3 newPos = position + glm::vec3(moveVec.x, 0.0f, moveVec.z);
        
        if (!checkCollision(chunkManager, newPos)) {
            position = newPos;
            
            // Rotate to face movement direction
            float targetYaw = glm::degrees(std::atan2(-moveVec.x, -moveVec.z));
            float currentYaw = rotation.y - rotationOffset.y;
            float diff = targetYaw - currentYaw;
            while (diff > 180.0f) diff -= 360.0f;
            while (diff < -180.0f) diff += 360.0f;
            currentYaw += diff * 8.0f * deltaTime;
            rotation.y = currentYaw + rotationOffset.y;
        } else {
            // Hit wall, try new direction
            chooseRandomWanderDir();
        }
    }

    // Apply vertical movement
    position.y += velocity.y * deltaTime;

    // Clamp to ground
    if (position.y < 1.0f) {
        position.y = 1.0f;
        velocity.y = 0.0f;
    }
}
