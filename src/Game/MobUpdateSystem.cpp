#include "MobUpdateSystem.h"

#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"
#include "../Entity/MobSpawnManager.h"
#include "../Entity/PlayerEntity.h"
#include "../Entity/ZombieEntity.h"
#include "../Entity/SkeletonEntity.h"
#include "../Entity/PigEntity.h"
#include "../Entity/ChickenEntity.h"
#include "../Entity/SheepEntity.h"
#include "../Network/NetworkManager.h"
#include "../Render/Camera.h"
#include "../World/ChunkManager.h"
#include "../Game/StatusEffectsSystem.h"
#include "../Core/Logger.h"

namespace {
    bool isDayTime(float normalizedTime) {
        float angle = normalizedTime * 6.28318530718f;
        float sunY = std::sin(angle);
        return sunY > 0.1f;
    }
}

MobUpdateSystem::MobUpdateSystem(Camera& cameraRef,
                                 ChunkManager& chunkManagerRef,
                                 EntityManager& entityManagerRef,
                                 StatusEffectsSystem& statusEffectsSystemRef,
                                 Network::NetworkManager& networkManagerRef,
                                 std::unique_ptr<MobSpawnManager>& mobSpawnManagerRef,
                                 std::unique_ptr<PlayerEntity>& playerEntityRef,
                                 std::vector<std::unique_ptr<ZombieEntity>>& zombiesRef,
                                 std::vector<std::unique_ptr<SkeletonEntity>>& skeletonsRef,
                                 std::vector<std::unique_ptr<PigEntity>>& pigsRef,
                                 std::vector<std::unique_ptr<ChickenEntity>>& chickensRef,
                                 std::vector<std::unique_ptr<SheepEntity>>& sheepRef,
                                 bool& useNewEntityManagerRef)
    : camera(cameraRef),
      chunkManager(chunkManagerRef),
      entityManager(entityManagerRef),
      statusEffectsSystem(statusEffectsSystemRef),
      networkManager(networkManagerRef),
      mobSpawnManager(mobSpawnManagerRef),
      playerEntity(playerEntityRef),
      zombies(zombiesRef),
      skeletons(skeletonsRef),
      pigs(pigsRef),
      chickens(chickensRef),
      sheep(sheepRef),
      useNewEntityManager(useNewEntityManagerRef) {
}

void MobUpdateSystem::update(float deltaTime, float normalizedTime, bool skipPlayerControls) {
    if (skipPlayerControls) {
        return;
    }

    if (useNewEntityManager) {
        updateNewEntityManager(deltaTime, normalizedTime);
    } else {
        updateLegacyEntities(deltaTime, normalizedTime);
    }
}

void MobUpdateSystem::updateNewEntityManager(float deltaTime, float normalizedTime) {
    glm::vec3 playerFeet = camera.getPosition();

    bool isOfflineOrServer = (networkManager.getMode() == Network::NetworkMode::OFFLINE) ||
                              networkManager.isHost();

    static bool loggedOnce = false;
    if (!loggedOnce && networkManager.isOnline()) {
        LOG_INFO("EntityManager: isOfflineOrServer=" + std::to_string(isOfflineOrServer) +
                 ", mode=" + std::to_string(static_cast<int>(networkManager.getMode())) +
                 ", isHost=" + std::to_string(networkManager.isHost()) +
                 ", isClient=" + std::to_string(networkManager.isClient()));
        loggedOnce = true;
    }

    if (isOfflineOrServer) {
        auto attacks = entityManager.update(deltaTime, playerFeet, normalizedTime);

        for (const auto& attack : attacks) {
            camera.velocity += attack.knockback;
        }

        auto entities = entityManager.getAllEntities();
        if (isDayTime(normalizedTime)) {
            for (auto* entity : entities) {
                if (dynamic_cast<ZombieEntity*>(entity) || dynamic_cast<SkeletonEntity*>(entity)) {
                    statusEffectsSystem.applyDaylightBurn(entity, deltaTime);
                }
            }
        } else {
            statusEffectsSystem.clearDaylightBurnStates();
        }

        for (auto* entity : entities) {
            statusEffectsSystem.applyFireBurn(entity, deltaTime);
            statusEffectsSystem.applyEntityFallDamage(entity);
        }

        if (networkManager.isHost()) {
            auto spawnEvents = entityManager.consumeSpawnEvents();
            for (const auto& spawn : spawnEvents) {
                networkManager.broadcastEntitySpawn(
                    spawn.id,
                    static_cast<uint8_t>(spawn.type),
                    spawn.position,
                    spawn.yaw
                );
            }

            auto despawnEvents = entityManager.consumeDespawnEvents();
            for (EntityId id : despawnEvents) {
                networkManager.broadcastEntityDespawn(id);
            }

            static float entitySyncTimer = 0.0f;
            entitySyncTimer += deltaTime;
            if (entitySyncTimer >= 0.1f) {
                entitySyncTimer = 0.0f;

                auto entityStates = entityManager.getEntityStatesForSync();
                for (const auto& state : entityStates) {
                    uint8_t flags = state.isDead ? 1 : 0;

                    networkManager.broadcastEntityUpdate(
                        state.id,
                        state.position, state.velocity, state.yaw, state.health, flags
                    );
                }
            }
        }
    }
}

void MobUpdateSystem::updateLegacyEntities(float deltaTime, float normalizedTime) {
    glm::vec3 playerFeet = camera.getPosition();

    if (networkManager.getMode() == Network::NetworkMode::OFFLINE) {
        if (mobSpawnManager && playerEntity) {
            mobSpawnManager->update(deltaTime, playerFeet, normalizedTime,
                                    zombies, skeletons, pigs, chickens, sheep);
        }

        if (playerEntity) {
            for (auto& z : zombies) {
                if (!z) continue;
                bool attacked = z->updateAI(deltaTime, chunkManager, playerFeet);
                if (attacked && !z->isDead()) {
                    camera.velocity += z->consumeAttackImpulse();
                }
            }

            for (auto& s : skeletons) {
                if (!s) continue;
                bool attacked = s->updateAI(deltaTime, chunkManager, playerFeet);
                if (attacked && !s->isDead()) {
                    camera.velocity += s->consumeAttackImpulse();
                }
            }

            if (isDayTime(normalizedTime)) {
                for (auto& z : zombies) {
                    if (z) statusEffectsSystem.applyDaylightBurn(z.get(), deltaTime);
                }
                for (auto& s : skeletons) {
                    if (s) statusEffectsSystem.applyDaylightBurn(s.get(), deltaTime);
                }
            } else {
                statusEffectsSystem.clearDaylightBurnStates();
            }

            for (auto& p : pigs) {
                if (p) p->updateAI(deltaTime, chunkManager);
            }
            for (auto& c : chickens) {
                if (c) c->updateAI(deltaTime, chunkManager);
            }
            for (auto& s : sheep) {
                if (s) s->updateAI(deltaTime, chunkManager);
            }

            for (auto& z : zombies) { if (z) { statusEffectsSystem.applyFireBurn(z.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(z.get()); } }
            for (auto& s : skeletons) { if (s) { statusEffectsSystem.applyFireBurn(s.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(s.get()); } }
            for (auto& p : pigs) { if (p) { statusEffectsSystem.applyFireBurn(p.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(p.get()); } }
            for (auto& c : chickens) { if (c) { statusEffectsSystem.applyFireBurn(c.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(c.get()); } }
            for (auto& s : sheep) { if (s) { statusEffectsSystem.applyFireBurn(s.get(), deltaTime); statusEffectsSystem.applyEntityFallDamage(s.get()); } }
        }
    }
}
