#include "MobSpawnManager.h"
#include "ZombieEntity.h"
#include "SkeletonEntity.h"
#include "PigEntity.h"
#include "ChickenEntity.h"
#include "SheepEntity.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../World/Block.h"
#include "../Util/Config.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cmath>

MobSpawnManager::MobSpawnManager(ChunkManager& chunkMgr, WorldGenerator& worldGen)
    : chunkManager(chunkMgr)
    , worldGenerator(worldGen)
    , rng(std::random_device{}())
{
    hostileSpawnTimer = config.hostileSpawnInterval;
    passiveSpawnTimer = config.passiveSpawnInterval;
}

bool MobSpawnManager::isNightTime(float timeOfDay) {
    // Night is roughly from 0.75 (sunset) to 0.25 (sunrise)
    // timeOfDay: 0.0 = midnight, 0.5 = noon
    return timeOfDay < 0.25f || timeOfDay > 0.75f;
}

int MobSpawnManager::getLightLevel(const glm::vec3& pos, float timeOfDay) const {
    // Simplified light level calculation
    // In a full implementation, this would use actual block light + sky light
    
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));
    
    // Check if underground (has solid block above)
    bool underground = false;
    for (int checkY = y + 1; checkY < y + 32 && checkY < 256; ++checkY) {
        BlockType block = getBlockAt(x, checkY, z);
        if (block != BlockType::AIR && 
            block != BlockType::WATER &&
            block != BlockType::LEAVES) {
            underground = true;
            break;
        }
    }
    
    if (underground) {
        // Underground - always dark (could add torch light in future)
        return 0;
    }
    
    // Surface lighting based on time of day
    if (isNightTime(timeOfDay)) {
        // Night: low light (4) but full moon could be brighter
        return 4;
    } else {
        // Day: full sunlight
        return 15;
    }
}

void MobSpawnManager::update(float deltaTime, const glm::vec3& playerPos, float timeOfDay,
                             std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                             std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                             std::vector<std::unique_ptr<PigEntity>>& pigs,
                             std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                             std::vector<std::unique_ptr<SheepEntity>>& sheep) {
    
    // Despawn distant mobs first
    despawnFarMobs(zombies, playerPos, config.hostileDespawnRadius);
    despawnFarMobs(skeletons, playerPos, config.hostileDespawnRadius);
    despawnFarMobs(pigs, playerPos, config.passiveDespawnRadius);
    despawnFarMobs(chickens, playerPos, config.passiveDespawnRadius);
    despawnFarMobs(sheep, playerPos, config.passiveDespawnRadius);
    
    // Hostile mob spawning
    hostileSpawnTimer -= deltaTime;
    if (hostileSpawnTimer <= 0.0f) {
        hostileSpawnTimer = config.hostileSpawnInterval;
        
        int totalHostile = static_cast<int>(zombies.size() + skeletons.size());
        if (totalHostile < config.maxHostileMobs) {
            for (int i = 0; i < config.maxSpawnAttemptsPerTick; ++i) {
                if (trySpawnHostileMob(playerPos, timeOfDay, zombies, skeletons)) {
                    break; // Successful spawn
                }
            }
        }
    }
    
    // Passive mob spawning - spawn multiple animals per tick for herds
    passiveSpawnTimer -= deltaTime;
    if (passiveSpawnTimer <= 0.0f) {
        passiveSpawnTimer = config.passiveSpawnInterval;
        
        int totalPassive = static_cast<int>(pigs.size() + chickens.size() + sheep.size());
        if (totalPassive < config.maxPassiveMobs) {
            int spawnsThisTick = 0;
            int maxSpawnsPerTick = 3;  // Spawn up to 3 animals per tick
            for (int i = 0; i < config.maxSpawnAttemptsPerTick && spawnsThisTick < maxSpawnsPerTick; ++i) {
                if (trySpawnPassiveMob(playerPos, timeOfDay, pigs, chickens, sheep)) {
                    spawnsThisTick++;
                }
            }
        }
    }
}

bool MobSpawnManager::trySpawnHostileMob(const glm::vec3& playerPos, float timeOfDay,
                                         std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                                         std::vector<std::unique_ptr<SkeletonEntity>>& skeletons) {
    
    glm::vec3 spawnPos = findSpawnPosition(playerPos, 
                                           config.minSpawnDistFromPlayer, 
                                           config.hostileSpawnRadius);
    
    if (!isValidHostileSpawn(spawnPos, timeOfDay)) {
        return false;
    }
    
    // Randomly choose between zombie and skeleton
    std::uniform_int_distribution<int> mobDist(0, 2);
    int mobType = mobDist(rng);
    
    if (mobType <= 1) {
        // 66% chance zombie
        zombies.push_back(std::make_unique<ZombieEntity>(spawnPos));
        LOG_INFO("Spawned zombie at (" + std::to_string(spawnPos.x) + ", " + 
                 std::to_string(spawnPos.y) + ", " + std::to_string(spawnPos.z) + ")");
    } else {
        // 33% chance skeleton
        skeletons.push_back(std::make_unique<SkeletonEntity>(spawnPos));
        LOG_INFO("Spawned skeleton at (" + std::to_string(spawnPos.x) + ", " + 
                 std::to_string(spawnPos.y) + ", " + std::to_string(spawnPos.z) + ")");
    }
    
    return true;
}

bool MobSpawnManager::trySpawnPassiveMob(const glm::vec3& playerPos, float timeOfDay,
                                         std::vector<std::unique_ptr<PigEntity>>& pigs,
                                         std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                                         std::vector<std::unique_ptr<SheepEntity>>& sheep) {
    
    glm::vec3 spawnPos = findSpawnPosition(playerPos,
                                           config.minSpawnDistFromPlayer,
                                           config.passiveSpawnRadius);
    
    if (!isValidPassiveSpawn(spawnPos, timeOfDay)) {
        return false;
    }
    
    // Randomly choose mob type
    std::uniform_int_distribution<int> mobDist(0, 2);
    int mobType = mobDist(rng);
    
    if (mobType == 0) {
        pigs.push_back(std::make_unique<PigEntity>(spawnPos));
        LOG_INFO("Spawned pig at (" + std::to_string(spawnPos.x) + ", " + 
                 std::to_string(spawnPos.y) + ", " + std::to_string(spawnPos.z) + ")");
    } else if (mobType == 1) {
        chickens.push_back(std::make_unique<ChickenEntity>(spawnPos));
        LOG_INFO("Spawned chicken at (" + std::to_string(spawnPos.x) + ", " + 
                 std::to_string(spawnPos.y) + ", " + std::to_string(spawnPos.z) + ")");
    } else {
        sheep.push_back(std::make_unique<SheepEntity>(spawnPos));
        LOG_INFO("Spawned sheep at (" + std::to_string(spawnPos.x) + ", " + 
                 std::to_string(spawnPos.y) + ", " + std::to_string(spawnPos.z) + ")");
    }
    
    return true;
}

glm::vec3 MobSpawnManager::findSpawnPosition(const glm::vec3& playerPos, float minDist, float maxDist) {
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> radiusDist(minDist, maxDist);
    
    float angle = angleDist(rng);
    float radius = radiusDist(rng);
    
    float spawnX = playerPos.x + std::cos(angle) * radius;
    float spawnZ = playerPos.z + std::sin(angle) * radius;
    
    // Find surface height
    int surfaceY = worldGenerator.getSurfaceHeight(
        static_cast<int>(std::floor(spawnX)),
        static_cast<int>(std::floor(spawnZ)));
    
    // Ensure above sea level (use global SEA_LEVEL from Config.h)
    if (surfaceY < SEA_LEVEL) {
        surfaceY = SEA_LEVEL + 1;
    }
    
    // Return position slightly above the block to ensure we don't spawn inside it
    // surfaceY is the Y coordinate of the solid block, so we spawn at Y+1
    return glm::vec3(spawnX, static_cast<float>(surfaceY) + 1.05f, spawnZ);
}

bool MobSpawnManager::isValidHostileSpawn(const glm::vec3& pos, float timeOfDay) const {
    // Check light level
    int light = getLightLevel(pos, timeOfDay);
    if (light > config.hostileMinLightLevel) {
        return false;  // Too bright
    }
    
    // Check for solid ground
    if (!hasSolidGround(pos)) {
        return false;
    }
    
    // Check headroom (2 blocks for zombie/skeleton)
    if (!hasHeadroom(pos, 2.0f)) {
        return false;
    }
    
    // Don't spawn in water
    BlockType blockBelow = getBlockAt(
        static_cast<int>(std::floor(pos.x)),
        static_cast<int>(std::floor(pos.y)) - 1,
        static_cast<int>(std::floor(pos.z)));
    
    if (blockBelow == BlockType::WATER) {
        return false;
    }
    
    return true;
}

bool MobSpawnManager::isValidPassiveSpawn(const glm::vec3& pos, float timeOfDay) const {
    // Check light level - animals prefer darker areas or twilight
    // They don't spawn in broad daylight (like Minecraft)
    int light = getLightLevel(pos, timeOfDay);
    
    // Animals spawn in shaded/dark areas OR during dawn/dusk
    bool isDawnOrDusk = (timeOfDay > 0.2f && timeOfDay < 0.3f) ||  // Dawn
                        (timeOfDay > 0.7f && timeOfDay < 0.8f);     // Dusk
    
    // Allow spawning if: in dark area (cave), OR during dawn/dusk, OR underground
    bool isUnderground = light == 0;  // Underground is always light level 0
    
    if (!isDawnOrDusk && !isUnderground && light > config.passiveMaxLightLevel) {
        return false;  // Too bright in broad daylight
    }
    
    // Check for solid ground
    if (!hasSolidGround(pos)) {
        return false;
    }
    
    // Check headroom (1.5 blocks for passive mobs)
    if (!hasHeadroom(pos, 1.5f)) {
        return false;
    }
    
    // Check block below is grass or dirt (animals spawn on grass)
    BlockType blockBelowPassive = getBlockAt(
        static_cast<int>(std::floor(pos.x)),
        static_cast<int>(std::floor(pos.y)) - 1,
        static_cast<int>(std::floor(pos.z)));
    
    if (blockBelowPassive != BlockType::GRASS && 
        blockBelowPassive != BlockType::DIRT &&
        blockBelowPassive != BlockType::SNOW) {
        return false;
    }
    
    return true;
}

bool MobSpawnManager::hasSolidGround(const glm::vec3& pos) const {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y)) - 1;
    int z = static_cast<int>(std::floor(pos.z));
    
    BlockType block = getBlockAt(x, y, z);
    
    // Check if block is solid (not air, water, etc.)
    return block != BlockType::AIR &&
           block != BlockType::WATER;
}

bool MobSpawnManager::hasHeadroom(const glm::vec3& pos, float height) const {
    int x = static_cast<int>(std::floor(pos.x));
    int baseY = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));
    
    int blocksNeeded = static_cast<int>(std::ceil(height));
    
    for (int dy = 0; dy < blocksNeeded; ++dy) {
        BlockType block = getBlockAt(x, baseY + dy, z);
        if (block != BlockType::AIR &&
            block != BlockType::WATER) {
            return false;
        }
    }
    
    return true;
}

BlockType MobSpawnManager::getBlockAt(int x, int y, int z) const {
    // Use ChunkManager to get block
    Block block = chunkManager.getBlockAt(x, y, z);
    return block.getType();
}

template<typename T>
void MobSpawnManager::despawnFarMobs(std::vector<std::unique_ptr<T>>& mobs,
                                     const glm::vec3& playerPos, float maxDist) {
    float maxDistSq = maxDist * maxDist;
    
    mobs.erase(
        std::remove_if(mobs.begin(), mobs.end(),
            [&playerPos, maxDistSq](const std::unique_ptr<T>& mob) {
                glm::vec3 diff = mob->getPosition() - playerPos;
                float distSq = diff.x * diff.x + diff.z * diff.z; // XZ distance only
                return distSq > maxDistSq;
            }),
        mobs.end());
}

// Explicit template instantiations
template void MobSpawnManager::despawnFarMobs<ZombieEntity>(
    std::vector<std::unique_ptr<ZombieEntity>>&, const glm::vec3&, float);
template void MobSpawnManager::despawnFarMobs<SkeletonEntity>(
    std::vector<std::unique_ptr<SkeletonEntity>>&, const glm::vec3&, float);
template void MobSpawnManager::despawnFarMobs<PigEntity>(
    std::vector<std::unique_ptr<PigEntity>>&, const glm::vec3&, float);
template void MobSpawnManager::despawnFarMobs<ChickenEntity>(
    std::vector<std::unique_ptr<ChickenEntity>>&, const glm::vec3&, float);
template void MobSpawnManager::despawnFarMobs<SheepEntity>(
    std::vector<std::unique_ptr<SheepEntity>>&, const glm::vec3&, float);
