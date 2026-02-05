#pragma once

#include "../World/Block.h"
#include <glm/glm.hpp>

#include <memory>
#include <vector>

class Window;
class UIManager;
class Camera;
class ChunkManager;
class HeldItemRenderer;
class EntityManager;
class PlayerEntity;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;
class BloodSplatterSystem;

namespace Physics {
class PhysicsTestSystem;
}

namespace Network {
class NetworkManager;
}

class InputSystem {
public:
    InputSystem(UIManager& uiManager,
                Camera& camera,
                ChunkManager& chunkManager,
                HeldItemRenderer& heldItemRenderer,
                EntityManager& entityManager,
                Network::NetworkManager& networkManager,
                Physics::PhysicsTestSystem& physicsTest,
                BloodSplatterSystem& bloodSplatter,
                std::unique_ptr<PlayerEntity>& playerEntity,
                std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                std::vector<std::unique_ptr<PigEntity>>& pigs,
                std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                std::vector<std::unique_ptr<SheepEntity>>& sheep,
                bool& useNewEntityManager,
                float& attackCooldown,
                bool& isBreakingBlock,
                float& blockBreakProgress,
                glm::ivec3& breakingBlockPos,
                BlockType& breakingBlockType,
                bool& isUnderwater);

    void setWindow(Window* window);

    void handleMouseButton(int button, int action, int mods);
    void processInput(float deltaTime);

private:
    Window* window = nullptr;

    UIManager& uiManager;
    Camera& camera;
    ChunkManager& chunkManager;
    HeldItemRenderer& heldItemRenderer;
    EntityManager& entityManager;
    Network::NetworkManager& networkManager;
    Physics::PhysicsTestSystem& physicsTest;
    BloodSplatterSystem& bloodSplatter;
    std::unique_ptr<PlayerEntity>& playerEntity;
    std::vector<std::unique_ptr<ZombieEntity>>& zombies;
    std::vector<std::unique_ptr<SkeletonEntity>>& skeletons;
    std::vector<std::unique_ptr<PigEntity>>& pigs;
    std::vector<std::unique_ptr<ChickenEntity>>& chickens;
    std::vector<std::unique_ptr<SheepEntity>>& sheep;
    bool& useNewEntityManager;
    float& attackCooldown;
    bool& isBreakingBlock;
    float& blockBreakProgress;
    glm::ivec3& breakingBlockPos;
    BlockType& breakingBlockType;
    bool& isUnderwater;

    double lastX = 0.0;
    double lastY = 0.0;
    bool firstMouse = true;
};
