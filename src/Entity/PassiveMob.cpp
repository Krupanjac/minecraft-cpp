#include "PassiveMob.h"

#include "../Core/Logger.h"
#include "../Model/Model.h"
#include "../World/ChunkManager.h"
#include "../Audio/AudioManager.h"

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

PassiveMob::PassiveMob(const glm::vec3& startPos) : Entity(startPos), entityId(0) {
    // Seed RNG from position
    unsigned int seed = 1337u
        ^ (unsigned int)(std::abs((int)startPos.x) * 73856093)
        ^ (unsigned int)(std::abs((int)startPos.z) * 19349663);
    rng.seed(seed);
}

PassiveMob::PassiveMob(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id)
    : Entity(startPos), entityId(id) {
    if (cachedModel) {
        setModel(cachedModel);
    }
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

    // Pig/Sheep have: Death, Headbutt, Idle, Idle_Eating, Jump_Loop, Jump_Start, Run, Walk
    // Chicken has: Attack, Death, Idle, Idle_Peck, Run
    idleAnim = pickAnimByKeywords(names, {"idle"});
    walkAnim = pickAnimByKeywords(names, {"walk"});
    runAnim = pickAnimByKeywords(names, {"run"});
    deathAnim = pickAnimByKeywords(names, {"death"});
    idleEatingAnim = pickAnimByKeywords(names, {"idle_eating", "idleeating", "eating"});
    headbuttAnim = pickAnimByKeywords(names, {"headbutt"});
    jumpStartAnim = pickAnimByKeywords(names, {"jump_start", "jumpstart"});
    jumpLoopAnim = pickAnimByKeywords(names, {"jump_loop", "jumploop"});
    attackAnim = pickAnimByKeywords(names, {"attack"});
    idlePeckAnim = pickAnimByKeywords(names, {"idle_peck", "idlepeck", "peck"});

    if (idleAnim.empty()) idleAnim = names[0];
    if (walkAnim.empty()) walkAnim = (names.size() > 1) ? names[1] : names[0];
    if (runAnim.empty()) runAnim = walkAnim;

    LOG_INFO("PassiveMob animations: idle='" + idleAnim + "' walk='" + walkAnim + "' run='" + runAnim +
             "' death='" + deathAnim + "' idleEating='" + idleEatingAnim + "' headbutt='" + headbuttAnim + "'");
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

void PassiveMob::takeDamage(float amount, const glm::vec3& knockbackDir) {
    if (dead) return;
    
    health -= amount;
    
    // Apply knockback
    if (glm::length(knockbackDir) > 0.001f) {
        velocity += knockbackDir;
        velocity.y += 4.0f;
    }
    
    // Play hurt sound
    Audio::SoundType hurtSound = getHurtSound();
    if (hurtSound != Audio::SoundType::NONE) {
        Audio::AudioManager::instance().playSoundAt(hurtSound, position);
    }
    
    if (health <= 0.0f) {
        dead = true;
        health = 0.0f;
        
        // Play death sound and animation
        Audio::SoundType deathSound = getDeathSound();
        if (deathSound != Audio::SoundType::NONE) {
            Audio::AudioManager::instance().playSoundAt(deathSound, position);
        }
        
        playDeathAnimation();
    }
}

void PassiveMob::playDeathAnimation() {
    if (!model) return;
    
    dead = true;
    isDeathPlaying = true;
    
    if (!deathAnim.empty()) {
        model->playAnimation(deathAnim, false);
    }
}

void PassiveMob::updateAI(float deltaTime, ChunkManager& chunkManager) {
    // If dead, only update death timer and animation
    if (dead) {
        deathTimer += deltaTime;
        if (deathTimer > DEATH_STAY_TIME) {
            float fadeProgress = (deathTimer - DEATH_STAY_TIME) / DEATH_FADE_TIME;
            deathFadeAlpha = std::max(0.0f, 1.0f - fadeProgress);
        }
        if (model) model->updateAnimation(deltaTime);
        return;
    }

    // Update animation
    if (model) {
        model->updateAnimation(deltaTime);
    }
    
    // Update idle eating timer
    if (isIdleEating && idleEatingTimer > 0.0f) {
        idleEatingTimer -= deltaTime;
        if (idleEatingTimer <= 0.0f) {
            isIdleEating = false;
        }
    }
    
    // Ambient sounds
    soundTimer -= deltaTime;
    if (soundTimer <= 0.0f) {
        Audio::SoundType ambientSound = getAmbientSound();
        if (ambientSound != Audio::SoundType::NONE) {
            Audio::AudioManager::instance().playSoundAt(ambientSound, position, 0.8f);
        }
        // Random interval between sounds
        std::uniform_real_distribution<float> dis(5.0f, 15.0f);
        nextSoundTime = dis(rng);
        soundTimer = nextSoundTime;
    }

    // === Improved Ground Detection & Physics ===
    // First, find the ground below us
    int blockX = static_cast<int>(std::floor(position.x));
    int blockZ = static_cast<int>(std::floor(position.z));
    int blockY = static_cast<int>(std::floor(position.y));
    
    // Find ground level by scanning down
    float groundY = -1000.0f;
    for (int y = blockY + 2; y >= blockY - 5 && y >= 0; --y) {
        Block below = chunkManager.getBlockAt(blockX, y - 1, blockZ);
        Block at = chunkManager.getBlockAt(blockX, y, blockZ);
        if (below.isSolid() && !at.isSolid()) {
            groundY = static_cast<float>(y);
            break;
        }
    }
    
    // Check if on ground
    float feetY = position.y;
    if (groundY > -900.0f && feetY <= groundY + 0.05f && feetY >= groundY - 0.5f) {
        // Snap to ground
        position.y = groundY;
        velocity.y = 0.0f;
        onGround = true;
    } else if (groundY > -900.0f && feetY > groundY + 0.1f) {
        // In air, apply gravity
        velocity.y -= 25.0f * deltaTime;
        velocity.y = std::max(velocity.y, -50.0f);
        position.y += velocity.y * deltaTime;
        onGround = false;
        
        // Don't go below ground
        if (position.y < groundY) {
            position.y = groundY;
            velocity.y = 0.0f;
            onGround = true;
        }
    } else {
        // No ground found, apply gravity
        velocity.y -= 25.0f * deltaTime;
        velocity.y = std::max(velocity.y, -50.0f);
        position.y += velocity.y * deltaTime;
        onGround = false;
    }

    // State machine
    stateTimer -= deltaTime;
    if (stateTimer <= 0.0f && !isIdleEating) {
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        float r = dis(rng);
        if (state == State::Idle) {
            if (r < 0.5f) {
                setState(State::Wander, 2.0f, 5.0f);
                if (model) model->playAnimation(walkAnim, true);
            } else if (r < 0.65f && !idleEatingAnim.empty()) {
                // Start idle eating animation (sheep/pigs)
                isIdleEating = true;
                idleEatingTimer = 3.0f + dis(rng) * 2.0f;
                if (model) model->playAnimation(idleEatingAnim, true);
            } else if (r < 0.8f && !idlePeckAnim.empty()) {
                // Start idle pecking animation (chickens)
                isIdleEating = true;  // Reuse the timer mechanism
                idleEatingTimer = 2.0f + dis(rng) * 1.5f;
                if (model) model->playAnimation(idlePeckAnim, true);
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
        // If fleeing and we have a run animation, switch to it
        if (state == State::Flee && !runAnim.empty()) {
            if (model && model->getCurrentAnimation() != runAnim) {
                model->playAnimation(runAnim, true);
            }
        }
        moveVec = desiredDir * speed * deltaTime;
    }

    // Try to move
    if (glm::length(glm::vec2(moveVec.x, moveVec.z)) > 0.001f) {
        glm::vec3 newPos = position + glm::vec3(moveVec.x, 0.0f, moveVec.z);
        
        if (!checkCollision(chunkManager, newPos)) {
            position.x = newPos.x;
            position.z = newPos.z;
            
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

    // Clamp to minimum height (fallback safety)
    if (position.y < 1.0f) {
        position.y = 1.0f;
        velocity.y = 0.0f;
    }
}
