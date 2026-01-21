#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <random>
#include <unordered_set>

enum class BlockType : unsigned char;

class ChunkManager;
class WorldGenerator;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;

// Minecraft-like mob spawning system
// - Hostile mobs spawn at night or in dark areas
// - Passive mobs spawn on grass during day
// - Spawning respects light levels and block types
// - Mobs despawn when too far from player

struct SpawnConfig {
    // Hostile mob settings
    int maxHostileMobs = 12;
    float hostileSpawnRadius = 24.0f;
    float hostileDespawnRadius = 64.0f;
    float hostileSpawnInterval = 2.0f;
    int hostileMinLightLevel = 7;  // Spawn only if light <= this
    
    // Passive mob settings  
    int maxPassiveMobs = 16;
    float passiveSpawnRadius = 32.0f;
    float passiveDespawnRadius = 80.0f;
    float passiveSpawnInterval = 4.0f;
    int passiveMinLightLevel = 9;  // Spawn only if light >= this
    
    // General settings
    float minSpawnDistFromPlayer = 16.0f;
    int maxSpawnAttemptsPerTick = 4;
};

class MobSpawnManager {
public:
    MobSpawnManager(ChunkManager& chunkMgr, WorldGenerator& worldGen);
    ~MobSpawnManager() = default;
    
    // Update spawning logic - call every frame
    // timeOfDay: 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
    void update(float deltaTime, const glm::vec3& playerPos, float timeOfDay,
                std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                std::vector<std::unique_ptr<SkeletonEntity>>& skeletons,
                std::vector<std::unique_ptr<PigEntity>>& pigs,
                std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                std::vector<std::unique_ptr<SheepEntity>>& sheep);
    
    // Configuration
    void setConfig(const SpawnConfig& cfg) { config = cfg; }
    const SpawnConfig& getConfig() const { return config; }
    
    // Check if it's nighttime (hostile mobs can spawn on surface)
    static bool isNightTime(float timeOfDay);
    
    // Get approximate light level at position (simplified)
    int getLightLevel(const glm::vec3& pos, float timeOfDay) const;
    
private:
    ChunkManager& chunkManager;
    WorldGenerator& worldGenerator;
    SpawnConfig config;
    
    std::mt19937 rng;
    float hostileSpawnTimer = 0.0f;
    float passiveSpawnTimer = 0.0f;
    
    // Spawn attempt helpers
    bool trySpawnHostileMob(const glm::vec3& playerPos, float timeOfDay,
                           std::vector<std::unique_ptr<ZombieEntity>>& zombies,
                           std::vector<std::unique_ptr<SkeletonEntity>>& skeletons);
                           
    bool trySpawnPassiveMob(const glm::vec3& playerPos, float timeOfDay,
                           std::vector<std::unique_ptr<PigEntity>>& pigs,
                           std::vector<std::unique_ptr<ChickenEntity>>& chickens,
                           std::vector<std::unique_ptr<SheepEntity>>& sheep);
    
    // Find a valid spawn position
    glm::vec3 findSpawnPosition(const glm::vec3& playerPos, float minDist, float maxDist);
    
    // Check if position is valid for spawning
    bool isValidHostileSpawn(const glm::vec3& pos, float timeOfDay) const;
    bool isValidPassiveSpawn(const glm::vec3& pos, float timeOfDay) const;
    
    // Despawn mobs that are too far
    template<typename T>
    void despawnFarMobs(std::vector<std::unique_ptr<T>>& mobs, 
                        const glm::vec3& playerPos, float maxDist);
    
    // Check if there's solid ground at position
    bool hasSolidGround(const glm::vec3& pos) const;
    
    // Get block type at position
    BlockType getBlockAt(int x, int y, int z) const;
    
    // Check if position has enough headroom for mob
    bool hasHeadroom(const glm::vec3& pos, float height) const;
};
