#pragma once

#include "../Mesh/MeshBuilder.h"
#include "../Physics/PhysicsTest.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Renderer;
class ChunkManager;
class WorldGenerator;
class ThreadPool;
class UIManager;
class EntityManager;
class PlayerHealthSystem;
class ExplosionVolumeSystem;
class FireSystem;
class Camera;
class PlayerEntity;
class Entity;
class MobSpawnManager;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;

namespace Network {
class NetworkManager;
}

namespace Physics {
class PhysicsTestSystem;
}

class WorldLifecycleSystem {
public:
    using LoadingProgressCallback = std::function<void(float)>;

    WorldLifecycleSystem(Renderer& renderer,
                         ChunkManager& chunkManager,
                         WorldGenerator& worldGenerator,
                         MeshBuilder& meshBuilder,
                         ThreadPool& threadPool,
                         std::mutex& meshMutex,
                         std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes,
                         UIManager& uiManager,
                         EntityManager& entityManager,
                         Network::NetworkManager& networkManager,
                         PlayerHealthSystem& playerHealthSystem,
                         ExplosionVolumeSystem& explosionVolumes,
                         FireSystem& fireSystem,
                         Camera& camera,
                         std::unique_ptr<PlayerEntity>& playerEntity,
                         std::unique_ptr<MobSpawnManager>& mobSpawnManager,
                         std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                         std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                         std::vector<std::unique_ptr<PigEntity>>& pigs,
                         std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                         std::vector<std::unique_ptr<SheepEntity>>& sheep,
                         bool& useNewEntityManager,
                         Physics::PhysicsTestSystem& physicsTest,
                         std::string& currentWorldName,
                         long& currentSeed);

    void setLoadingCallback(const LoadingProgressCallback& callback);
    void setShouldCloseCallback(const std::function<bool()>& callback);

    void createWorld(const std::string& name, long seed = 12345);
    bool loadWorld(const std::string& name = "world.dat");

private:
#if ENABLE_PHYSICS_TEST
    std::vector<Entity*> collectEntities();
    void initializePhysicsTest();
#endif

    void renderLoadingProgress(float progress);

    Renderer& renderer;
    ChunkManager& chunkManager;
    WorldGenerator& worldGenerator;
    MeshBuilder& meshBuilder;
    ThreadPool& threadPool;
    std::mutex& meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes;
    UIManager& uiManager;
    EntityManager& entityManager;
    Network::NetworkManager& networkManager;
    PlayerHealthSystem& playerHealthSystem;
    ExplosionVolumeSystem& explosionVolumes;
    FireSystem& fireSystem;
    Camera& camera;
    std::unique_ptr<PlayerEntity>& playerEntity;
    std::unique_ptr<MobSpawnManager>& mobSpawnManager;
    std::vector<std::unique_ptr<ZombieEntity>>& zombies;
    std::vector<std::unique_ptr<SkeletonEntity>>& skeletons;
    std::vector<std::unique_ptr<PigEntity>>& pigs;
    std::vector<std::unique_ptr<ChickenEntity>>& chickens;
    std::vector<std::unique_ptr<SheepEntity>>& sheep;
    bool& useNewEntityManager;
    Physics::PhysicsTestSystem& physicsTest;
    std::string& currentWorldName;
    long& currentSeed;

    LoadingProgressCallback loadingCallback;
    std::function<bool()> shouldCloseCallback;
};
