#pragma once

#include <memory>
#include <vector>

#include "../World/Block.h"

class Window;
class Renderer;
class ChunkManager;
class MenuWorldSystem;
class Camera;
class EntityManager;
class WorldGenerator;
class HeldItemRenderer;
class FireSystem;
class ExplosionVolumeSystem;
class BloodSplatterSystem;
class PlayerEntity;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;
class UIManager;

namespace Network {
class NetworkManager;
class RemotePlayerEntity;
}

namespace Physics {
class PhysicsTestSystem;
}

class RenderPipelineSystem {
public:
    RenderPipelineSystem(Renderer& renderer,
                         ChunkManager& chunkManager,
                         MenuWorldSystem& menuWorldSystem,
                         Camera& camera,
                         EntityManager& entityManager,
                         WorldGenerator& worldGenerator,
                         HeldItemRenderer& heldItemRenderer,
                         FireSystem& fireSystem,
                         ExplosionVolumeSystem& explosionVolumes,
                         BloodSplatterSystem& bloodSplatter,
                         UIManager& uiManager,
                         Network::NetworkManager& networkManager,
                         Physics::PhysicsTestSystem& physicsTest,
                         std::unique_ptr<PlayerEntity>& playerEntity,
                         std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                         std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                         std::vector<std::unique_ptr<PigEntity>>& pigs,
                         std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                         std::vector<std::unique_ptr<SheepEntity>>& sheep,
                         bool& useNewEntityManager);

    void setWindow(Window* window);

    void renderFrame(bool isBreakingBlock, float blockBreakProgress, const glm::ivec3& breakingBlockPos);

private:
    void renderMenuWorld();
    void collectEntities(std::vector<class Entity*>& entities);
    void renderHeldItems(const std::vector<Network::RemotePlayerEntity*>& remotePlayerEntities);

    Window* window = nullptr;
    Renderer& renderer;
    ChunkManager& chunkManager;
    MenuWorldSystem& menuWorldSystem;
    Camera& camera;
    EntityManager& entityManager;
    WorldGenerator& worldGenerator;
    HeldItemRenderer& heldItemRenderer;
    FireSystem& fireSystem;
    ExplosionVolumeSystem& explosionVolumes;
    BloodSplatterSystem& bloodSplatter;
    UIManager& uiManager;
    Network::NetworkManager& networkManager;
    Physics::PhysicsTestSystem& physicsTest;
    std::unique_ptr<PlayerEntity>& playerEntity;
    std::vector<std::unique_ptr<ZombieEntity>>& zombies;
    std::vector<std::unique_ptr<SkeletonEntity>>& skeletons;
    std::vector<std::unique_ptr<PigEntity>>& pigs;
    std::vector<std::unique_ptr<ChickenEntity>>& chickens;
    std::vector<std::unique_ptr<SheepEntity>>& sheep;
    bool& useNewEntityManager;
};
