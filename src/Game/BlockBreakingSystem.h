#pragma once

#include "../World/Block.h"
#include "../World/Item.h"
#include <glm/glm.hpp>

class UIManager;
class Camera;
class ChunkManager;
class HeldItemRenderer;

namespace Network {
class NetworkManager;
}

namespace Physics {
class PhysicsTestSystem;
}

class BlockBreakingSystem {
public:
    BlockBreakingSystem(UIManager& uiManager,
                        Camera& camera,
                        ChunkManager& chunkManager,
                        HeldItemRenderer& heldItemRenderer,
                        Network::NetworkManager& networkManager,
                        Physics::PhysicsTestSystem& physicsTest,
                        bool& isBreakingBlock,
                        float& blockBreakProgress,
                        glm::ivec3& breakingBlockPos,
                        BlockType& breakingBlockType);

    void update(float deltaTime, ItemType currentHeldItem);

private:
    float getBlockBreakTime(BlockType type) const;

    UIManager& uiManager;
    Camera& camera;
    ChunkManager& chunkManager;
    HeldItemRenderer& heldItemRenderer;
    Network::NetworkManager& networkManager;
    Physics::PhysicsTestSystem& physicsTest;
    bool& isBreakingBlock;
    float& blockBreakProgress;
    glm::ivec3& breakingBlockPos;
    BlockType& breakingBlockType;
    float digSoundTimer = 0.0f;
};
