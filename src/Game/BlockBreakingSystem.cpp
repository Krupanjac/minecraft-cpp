#define NOMINMAX
#include "BlockBreakingSystem.h"

#include "../Audio/AudioManager.h"
#include "../Network/NetworkManager.h"
#include "../Physics/PhysicsTest.h"
#include "../Render/Camera.h"
#include "../Render/HeldItemRenderer.h"
#include "../UI/UIManager.h"
#include "../World/ChunkManager.h"
#include "../World/Item.h"

#include <cmath>

BlockBreakingSystem::BlockBreakingSystem(UIManager& uiManagerRef,
                                         Camera& cameraRef,
                                         ChunkManager& chunkManagerRef,
                                         HeldItemRenderer& heldItemRendererRef,
                                         Network::NetworkManager& networkManagerRef,
                                         Physics::PhysicsTestSystem& physicsTestRef,
                                         bool& isBreakingBlockRef,
                                         float& blockBreakProgressRef,
                                         glm::ivec3& breakingBlockPosRef,
                                         BlockType& breakingBlockTypeRef)
    : uiManager(uiManagerRef),
      camera(cameraRef),
      chunkManager(chunkManagerRef),
      heldItemRenderer(heldItemRendererRef),
      networkManager(networkManagerRef),
      physicsTest(physicsTestRef),
      isBreakingBlock(isBreakingBlockRef),
      blockBreakProgress(blockBreakProgressRef),
      breakingBlockPos(breakingBlockPosRef),
      breakingBlockType(breakingBlockTypeRef) {
}

void BlockBreakingSystem::update(float deltaTime, ItemType currentHeldItem) {
    if (isBreakingBlock && !uiManager.isCreativeMode && uiManager.isWorldLoaded()) {
        glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
        auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);

        if (result.hit) {
            glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
            int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
            int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
            int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;

            if (breakingBlockPos != glm::ivec3(x, y, z)) {
                breakingBlockPos = glm::ivec3(x, y, z);
                blockBreakProgress = 0.0f;
                breakingBlockType = chunkManager.getBlockAt(x, y, z).getType();
            }

            float baseBreakTime = getBlockBreakTime(breakingBlockType);
            float toolMultiplier = ItemRegistry::instance().getMiningMultiplier(currentHeldItem, breakingBlockType);
            float effectiveBreakTime = baseBreakTime / toolMultiplier;

            blockBreakProgress += deltaTime / effectiveBreakTime;

            heldItemRenderer.setMining(true);

            digSoundTimer += deltaTime;
            if (digSoundTimer >= 0.25f) {
                Audio::SoundType digHitSound = Audio::getDigSoundForBlock(static_cast<uint8_t>(breakingBlockType));
                Audio::AudioManager::instance().playSoundAt(digHitSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 0.3f);
                digSoundTimer = 0.0f;
            }

            if (blockBreakProgress >= 1.0f) {
                Audio::SoundType digSound = Audio::getDigSoundForBlock(static_cast<uint8_t>(breakingBlockType));
                Audio::AudioManager::instance().playSoundAt(digSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));

#if ENABLE_PHYSICS_TEST
                physicsTest.spawnDroppedItem(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), breakingBlockType);
#endif

                chunkManager.setBlockAt(x, y, z, Block(BlockType::AIR));

                if (networkManager.isOnline()) {
                    networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(BlockType::AIR));
                }

                blockBreakProgress = 0.0f;
            }
        } else {
            blockBreakProgress = 0.0f;
            heldItemRenderer.setMining(false);
        }
    } else {
        heldItemRenderer.setMining(false);
    }
}

float BlockBreakingSystem::getBlockBreakTime(BlockType type) const {
    switch (type) {
        case BlockType::AIR:
        case BlockType::TALL_GRASS:
        case BlockType::ROSE:
            return 0.0f;
        case BlockType::LEAVES:
            return 0.35f;
        case BlockType::DIRT:
        case BlockType::GRASS:
        case BlockType::SAND:
        case BlockType::GRAVEL:
        case BlockType::SNOW:
            return 0.75f;
        case BlockType::WOOD:
        case BlockType::LOG:
            return 3.0f;
        case BlockType::STONE:
        case BlockType::SANDSTONE:
            return 7.5f;
        case BlockType::ICE:
            return 0.7f;
        case BlockType::WATER:
        case BlockType::BEDROCK:
            return 100000.0f;
        default:
            return 1.5f;
    }
}
