#pragma once

#include <memory>
#include <vector>

class Camera;
class ChunkManager;
class EntityManager;
class StatusEffectsSystem;
class MobSpawnManager;
class PlayerEntity;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;

namespace Network {
class NetworkManager;
}

class MobUpdateSystem {
public:
    MobUpdateSystem(Camera& camera,
                    ChunkManager& chunkManager,
                    EntityManager& entityManager,
                    StatusEffectsSystem& statusEffectsSystem,
                    Network::NetworkManager& networkManager,
                    std::unique_ptr<MobSpawnManager>& mobSpawnManager,
                    std::unique_ptr<PlayerEntity>& playerEntity,
                    std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                    std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                    std::vector<std::unique_ptr<PigEntity>>& pigs,
                    std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                    std::vector<std::unique_ptr<SheepEntity>>& sheep,
                    bool& useNewEntityManager);

    void update(float deltaTime, float normalizedTime, bool skipPlayerControls);

private:
    void updateNewEntityManager(float deltaTime, float normalizedTime);
    void updateLegacyEntities(float deltaTime, float normalizedTime);

    Camera& camera;
    ChunkManager& chunkManager;
    EntityManager& entityManager;
    StatusEffectsSystem& statusEffectsSystem;
    Network::NetworkManager& networkManager;
    std::unique_ptr<MobSpawnManager>& mobSpawnManager;
    std::unique_ptr<PlayerEntity>& playerEntity;
    std::vector<std::unique_ptr<ZombieEntity>>& zombies;
    std::vector<std::unique_ptr<SkeletonEntity>>& skeletons;
    std::vector<std::unique_ptr<PigEntity>>& pigs;
    std::vector<std::unique_ptr<ChickenEntity>>& chickens;
    std::vector<std::unique_ptr<SheepEntity>>& sheep;
    bool& useNewEntityManager;
};
