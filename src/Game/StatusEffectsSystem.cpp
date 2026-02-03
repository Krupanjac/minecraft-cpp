#include "StatusEffectsSystem.h"
#include "../World/ChunkManager.h"
#include "../World/Block.h"
#include "../World/FireSystem.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Render/Camera.h"
#include "../Entity/Entity.h"
#include "../Audio/AudioManager.h"

#include <algorithm>
#include <cmath>

StatusEffectsSystem::StatusEffectsSystem(ChunkManager& chunkMgr,
                                         FireSystem& fireSys,
                                         ExplosionVolumeSystem& explosionVfx,
                                         Camera& cam)
    : chunkManager(chunkMgr),
      fireSystem(fireSys),
      explosionVolumes(explosionVfx),
      camera(cam) {
}

bool StatusEffectsSystem::isExposedToSky(const glm::vec3& pos) const {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));

    for (int checkY = y + 1; checkY < y + 32 && checkY < 256; ++checkY) {
        Block block = chunkManager.getBlockAt(x, checkY, z);
        if (block.isSolid() && !block.isLeaves()) {
            return false;
        }
    }
    return true;
}

void StatusEffectsSystem::applyDaylightBurn(Entity* entity, float deltaTime) {
    if (!entity || entity->isDead()) {
        mobBurnStates.erase(entity);
        return;
    }

    if (!isExposedToSky(entity->getPosition())) {
        mobBurnStates.erase(entity);
        return;
    }

    auto& state = mobBurnStates[entity];
    state.damageTimer += deltaTime;
    state.vfxTimer += deltaTime;

    if (state.damageTimer >= 1.0f) {
        state.damageTimer = 0.0f;
        entity->takeDamage(1.0f);
    }

    if (state.vfxTimer >= 0.35f) {
        state.vfxTimer = 0.0f;
        explosionVolumes.spawnFire(entity->getPosition() + glm::vec3(0.0f, 0.6f, 0.0f), 1.0f, 0.9f);
    }
}

void StatusEffectsSystem::clearDaylightBurnStates() {
    mobBurnStates.clear();
}

bool StatusEffectsSystem::isPositionInFire(const glm::vec3& pos) const {
    glm::ivec3 blockPos(
        static_cast<int>(std::floor(pos.x)),
        static_cast<int>(std::floor(pos.y)),
        static_cast<int>(std::floor(pos.z))
    );

    if (fireSystem.isBurning(blockPos)) return true;
    if (fireSystem.isBurning(blockPos + glm::ivec3(0, 1, 0))) return true;
    if (fireSystem.isBurning(blockPos + glm::ivec3(0, -1, 0))) return true;

    return false;
}

void StatusEffectsSystem::applyFireBurn(Entity* entity, float deltaTime) {
    if (!entity || entity->isDead()) {
        fireBurnStates.erase(entity);
        return;
    }

    glm::vec3 pos = entity->getPosition();
    bool inFire = isPositionInFire(pos + glm::vec3(0.0f, 0.2f, 0.0f)) ||
                  isPositionInFire(pos + glm::vec3(0.0f, 0.9f, 0.0f));

    if (!inFire) {
        fireBurnStates.erase(entity);
        return;
    }

    auto& state = fireBurnStates[entity];
    state.damageTimer += deltaTime;
    state.vfxTimer += deltaTime;

    if (state.damageTimer >= 0.6f) {
        state.damageTimer = 0.0f;
        entity->takeDamage(1.0f);
    }

    if (state.vfxTimer >= 0.4f) {
        state.vfxTimer = 0.0f;
        explosionVolumes.spawnFire(pos + glm::vec3(0.0f, 0.6f, 0.0f), 1.0f, 0.9f);
    }
}

void StatusEffectsSystem::applyPlayerFireBurn(float deltaTime,
                                              float playerHealth,
                                              bool isUnderwater,
                                              const std::function<void(float, const glm::vec3&, bool)>& playerDamage) {
    if (playerHealth <= 0.0f) {
        playerFireState = BurnState{};
        return;
    }

    if (isUnderwater) {
        playerFireState = BurnState{};
        return;
    }

    glm::vec3 pos = camera.getPosition();
    bool inFire = isPositionInFire(pos + glm::vec3(0.0f, 0.2f, 0.0f)) ||
                  isPositionInFire(pos + glm::vec3(0.0f, 1.0f, 0.0f));

    if (!inFire) {
        playerFireState = BurnState{};
        return;
    }

    playerFireState.damageTimer += deltaTime;
    playerFireState.vfxTimer += deltaTime;

    if (playerFireState.damageTimer >= 0.6f) {
        playerFireState.damageTimer = 0.0f;
        if (playerDamage) {
            playerDamage(1.0f, glm::vec3(0.0f), true);
        }
    }

    if (playerFireState.vfxTimer >= 0.4f) {
        playerFireState.vfxTimer = 0.0f;
        explosionVolumes.spawnFire(pos + glm::vec3(0.0f, 0.6f, 0.0f), 1.0f, 0.9f);
    }
}

bool StatusEffectsSystem::isGroundedAt(const glm::vec3& pos) const {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y - 0.1f));
    int z = static_cast<int>(std::floor(pos.z));
    Block below = chunkManager.getBlockAt(x, y, z);
    return below.isSolid();
}

void StatusEffectsSystem::applyPlayerFallDamage(float playerHealth,
                                                bool isUnderwater,
                                                bool isFlying,
                                                bool isCreative,
                                                const std::function<void(float, const glm::vec3&, bool)>& playerDamage) {
    if (playerHealth <= 0.0f) {
        playerFallState = FallState{};
        return;
    }

    if (isCreative || isFlying || isUnderwater) {
        playerFallState = FallState{};
        return;
    }

    bool onGround = camera.onGround || isGroundedAt(camera.getPosition());
    float y = camera.getPosition().y;

    if (!playerFallState.falling && !onGround) {
        playerFallState.falling = true;
        playerFallState.startY = y;
    }

    if (playerFallState.falling && y > playerFallState.startY) {
        playerFallState.startY = y;
    }

    if (playerFallState.falling && onGround) {
        float fallDist = playerFallState.startY - y;
        if (fallDist >= 4.0f) {
            float damage = std::max(0.0f, fallDist - 4.0f);
            if (playerDamage) {
                playerDamage(damage, glm::vec3(0.0f), false);
            }
            Audio::AudioManager::instance().playSound(Audio::SoundType::PLAYER_FALL_BIG, 0.9f);
        } else if (fallDist > 0.1f) {
            Audio::AudioManager::instance().playSound(Audio::SoundType::PLAYER_FALL_SMALL, 0.7f);
        }
        playerFallState.falling = false;
    }
}

void StatusEffectsSystem::applyEntityFallDamage(Entity* entity) {
    if (!entity || entity->isDead()) {
        entityFallStates.erase(entity);
        return;
    }

    glm::vec3 pos = entity->getPosition();
    bool onGround = isGroundedAt(pos);
    auto& state = entityFallStates[entity];

    if (!state.falling && !onGround) {
        state.falling = true;
        state.startY = pos.y;
    }

    if (state.falling && pos.y > state.startY) {
        state.startY = pos.y;
    }

    if (state.falling && onGround) {
        float fallDist = state.startY - pos.y;
        if (fallDist >= 4.0f) {
            float damage = std::max(0.0f, fallDist - 4.0f);
            entity->takeDamage(damage);
            Audio::AudioManager::instance().playSoundAt(Audio::SoundType::PLAYER_FALL_BIG, pos, 0.8f);
        } else if (fallDist > 0.1f) {
            Audio::AudioManager::instance().playSoundAt(Audio::SoundType::PLAYER_FALL_SMALL, pos, 0.6f);
        }
        state.falling = false;
    }
}
