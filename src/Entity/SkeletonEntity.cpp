#include "SkeletonEntity.h"

#include "../Core/Logger.h"
#include "../Model/Model.h"
#include "../World/ChunkManager.h"

#include <algorithm>
#include <cctype>

static std::string toLowerSkel(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string pickAnimByKeywordsSkel(const std::vector<std::string>& names, std::initializer_list<const char*> keys) {
    for (const auto& n : names) {
        std::string ln = toLowerSkel(n);
        for (auto* k : keys) {
            if (ln.find(k) != std::string::npos) return n;
        }
    }
    return "";
}

static bool checkSkeletonCollision(ChunkManager& chunkManager, const glm::vec3& feetPos) {
    constexpr float HALF_W = 0.30f;
    constexpr float HEIGHT = 1.80f;

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

static bool trySkeletonStepUp(ChunkManager& chunkManager, glm::vec3& pos, float dx, float dz) {
    constexpr float STEP = 1.0f;
    glm::vec3 tryPos = pos;
    tryPos.y += STEP;
    if (checkSkeletonCollision(chunkManager, tryPos)) return false;
    tryPos.x += dx;
    tryPos.z += dz;
    if (checkSkeletonCollision(chunkManager, tryPos)) return false;
    pos = tryPos;
    return true;
}

// Original constructor - loads model internally (causes stutter on spawn)
SkeletonEntity::SkeletonEntity(const glm::vec3& startPos) : Entity(startPos), entityId(0) {
    std::string modelPath = "assets/models/Skeleton/Skeleton.gltf";
    auto skeletonModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(skeletonModel);
    initializeCommon(startPos);
}

// New constructor with pre-loaded model (no stutter)
SkeletonEntity::SkeletonEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id)
    : Entity(startPos), entityId(id) {
    if (cachedModel) {
        setModel(cachedModel);
    } else {
        std::string modelPath = "assets/models/Skeleton/Skeleton.gltf";
        auto skeletonModel = std::make_shared<ModelSystem::Model>(modelPath);
        setModel(skeletonModel);
    }
    initializeCommon(startPos);
}

void SkeletonEntity::initializeCommon(const glm::vec3& startPos) {
    // Quaternius model scale
    setScale(glm::vec3(0.5f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);

    unsigned int seed = 1337u
        ^ (unsigned int)(std::abs((int)startPos.x) * 73856093)
        ^ (unsigned int)(std::abs((int)startPos.z) * 19349663);
    rng.seed(seed);

    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
    
    LOG_INFO("Skeleton entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

void SkeletonEntity::pickAnimations() {
    if (!model) return;
    auto names = model->getAnimationNames();
    if (names.empty()) {
        LOG_WARNING("Skeleton: no animations found");
        return;
    }

    idleAnim = pickAnimByKeywordsSkel(names, {"idle"});
    walkAnim = pickAnimByKeywordsSkel(names, {"walk"});
    attackAnim = pickAnimByKeywordsSkel(names, {"attack"});

    if (idleAnim.empty()) idleAnim = names[0];
    if (walkAnim.empty()) walkAnim = (names.size() > 1) ? names[1] : names[0];

    LOG_INFO("Skeleton animations: idle='" + idleAnim + "' walk='" + walkAnim + "' attack='" + attackAnim + "'");
    model->playAnimation(idleAnim, true);
}

void SkeletonEntity::setState(State s, float minTime, float maxTime) {
    state = s;
    std::uniform_real_distribution<float> dis(minTime, maxTime);
    stateTimer = dis(rng);
    if (state == State::Wander) chooseRandomWanderDir();
}

void SkeletonEntity::chooseRandomWanderDir() {
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    float a = dis(rng) * 6.2831853f;
    desiredDir = glm::normalize(glm::vec3(std::cos(a), 0.0f, std::sin(a)));
}

glm::vec3 SkeletonEntity::consumeAttackImpulse() {
    glm::vec3 out = attackImpulse;
    attackImpulse = glm::vec3(0.0f);
    return out;
}

void SkeletonEntity::takeDamage(float amount) {
    health -= amount;
    if (health <= 0.0f) {
        dead = true;
        health = 0.0f;
    }
}

bool SkeletonEntity::updateAI(float deltaTime, ChunkManager& chunkManager, const glm::vec3& playerPos) {
    if (dead) return false;
    
    prevPosition = position;
    prevRotation = rotation;
    prevScale = scale;

    attackImpulse = glm::vec3(0.0f);
    bool attacked = false;

    attackCooldown = std::max(0.0f, attackCooldown - deltaTime);
    stateTimer -= deltaTime;

    glm::vec3 toPlayer = playerPos - position;
    float distXZ = glm::length(glm::vec2(toPlayer.x, toPlayer.z));

    constexpr float CHASE_RANGE = 16.0f;
    constexpr float GIVE_UP_RANGE = 22.0f;
    constexpr float ATTACK_RANGE = 1.8f;

    // State transitions
    if (state != State::Chase && distXZ < CHASE_RANGE) {
        state = State::Chase;
        stateTimer = 9999.0f;
    } else if (state == State::Chase && distXZ > GIVE_UP_RANGE) {
        setState(State::Idle, 0.5f, 2.0f);
    } else if (state != State::Chase && stateTimer <= 0.0f) {
        if (state == State::Idle) setState(State::Wander, 1.5f, 4.0f);
        else setState(State::Idle, 0.8f, 2.5f);
    }

    glm::vec3 dir(0.0f);
    float speed = 0.0f;

    if (state == State::Chase) {
        speed = 1.4f; // Skeletons slightly faster than zombies
        
        glm::vec3 toT = playerPos - position;
        if (glm::length(glm::vec2(toT.x, toT.z)) > 0.001f) 
            dir = glm::normalize(glm::vec3(toT.x, 0.0f, toT.z));
    } else if (state == State::Wander) {
        dir = desiredDir;
        speed = 0.8f;
    }

    // Face movement/player
    if (glm::length(glm::vec2(dir.x, dir.z)) > 0.001f) {
        glm::vec3 faceDir = dir;
        if (state == State::Chase && distXZ > 0.001f) {
            faceDir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
        }
        float yaw = std::atan2(-faceDir.x, -faceDir.z);
        rotation.x = rotationOffset.x;
        rotation.y = glm::degrees(yaw) + rotationOffset.y;
        rotation.z = rotationOffset.z;
    }

    // Escape stuck in blocks
    for (int i = 0; i < 8; ++i) {
        if (!checkSkeletonCollision(chunkManager, position)) break;
        position.y += 1.0f;
        velocity.y = 0.0f;
    }

    // Attack when close
    if (state == State::Chase && distXZ < ATTACK_RANGE && attackCooldown <= 0.0f) {
        glm::vec3 away = (distXZ > 0.001f) ? glm::normalize(glm::vec3(-toPlayer.x, 0.0f, -toPlayer.z)) : glm::vec3(0.0f, 0.0f, 1.0f);
        attackImpulse = away * 4.0f + glm::vec3(0.0f, 2.5f, 0.0f);
        attackCooldown = 1.0f;
        attacked = true;
    }

    // Physics
    velocity.x = dir.x * speed;
    velocity.z = dir.z * speed;
    velocity.y -= 32.0f * deltaTime;
    velocity.y = std::max(-78.4f, velocity.y);

    glm::vec3 pos = position;
    glm::vec3 step = velocity * deltaTime;

    // X collision
    if (checkSkeletonCollision(chunkManager, glm::vec3(pos.x + step.x, pos.y, pos.z))) {
        if (!(onGround && trySkeletonStepUp(chunkManager, pos, step.x, 0.0f))) {
            step.x = 0.0f;
            velocity.x = 0.0f;
            if (state != State::Chase) chooseRandomWanderDir();
        }
    } else {
        pos.x += step.x;
    }

    // Z collision
    if (checkSkeletonCollision(chunkManager, glm::vec3(pos.x, pos.y, pos.z + step.z))) {
        if (!(onGround && trySkeletonStepUp(chunkManager, pos, 0.0f, step.z))) {
            step.z = 0.0f;
            velocity.z = 0.0f;
            if (state != State::Chase) chooseRandomWanderDir();
        }
    } else {
        pos.z += step.z;
    }

    // Y collision
    if (checkSkeletonCollision(chunkManager, glm::vec3(pos.x, pos.y + step.y, pos.z))) {
        if (step.y < 0.0f) onGround = true;
        step.y = 0.0f;
        velocity.y = 0.0f;
    } else {
        onGround = false;
    }
    pos.y += step.y;

    position = pos;

    // Animation
    if (model) {
        float horizSpeed = glm::length(glm::vec2(dir.x, dir.z)) * speed;
        std::string currentAnim = model->getCurrentAnimation();
        if (horizSpeed > 0.05f) {
            model->setAnimationLoopEndFactor(1.0f / 3.0f);
            model->setLockRootMotionXZ(true);
            float animSpeed = std::clamp(speed / 1.4f, 0.8f, 1.0f);
            model->setAnimationSpeed(animSpeed);
            if (!walkAnim.empty() && currentAnim != walkAnim) model->playAnimation(walkAnim, true);
        } else {
            model->setAnimationLoopEndFactor(1.0f);
            model->setLockRootMotionXZ(false);
            model->setAnimationSpeed(1.0f);
            if (!idleAnim.empty() && currentAnim != idleAnim) model->playAnimation(idleAnim, true);
        }
        model->updateAnimation(deltaTime);
    }

    return attacked;
}
