#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <random>
#include <string>

// Forward declarations
namespace ModelSystem { class Model; }
class ChunkManager;
class WorldGenerator;
class Entity;
class ZombieEntity;
class SkeletonEntity;
class PigEntity;
class ChickenEntity;
class SheepEntity;

// Mob types for spawning system
enum class MobType : uint8_t {
    ZOMBIE = 0,
    SKELETON = 1,
    PIG = 2,
    CHICKEN = 3,
    SHEEP = 4,
    COUNT
};

// Unique ID for entities across network
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY_ID = 0;

// Spawn request queued for async processing
struct SpawnRequest {
    MobType type;
    glm::vec3 position;
    EntityId assignedId = INVALID_ENTITY_ID;  // 0 = auto-assign
    bool immediate = false;  // Skip queue, spawn on main thread
};

// Entity state for network sync
struct NetworkEntityState {
    EntityId id = INVALID_ENTITY_ID;
    MobType type = MobType::ZOMBIE;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float health = 20.0f;
    bool isDead = false;
    uint8_t aiState = 0;  // Compressed AI state
    uint32_t targetEntityId = INVALID_ENTITY_ID;  // Who they're chasing
};

// Configuration for entity management
struct EntityManagerConfig {
    // Spawning limits
    int maxHostileMobs = 30;
    int maxPassiveMobs = 50;
    int maxTotalMobs = 70;
    
    // Distances
    float spawnRadius = 48.0f;
    float despawnRadius = 128.0f;
    float minSpawnDistance = 24.0f;
    float activationRadius = 64.0f;  // Mobs within this run full AI
    float sleepRadius = 96.0f;       // Beyond activation, reduced tick rate
    
    // Spawn timing (to distribute load)
    float spawnCheckInterval = 0.5f;  // Check spawns every X seconds
    int maxSpawnsPerFrame = 2;        // Max entities to actually create per frame
    int maxDespawnsPerFrame = 5;      // Max entities to remove per frame
    
    // Model preloading
    bool preloadModels = true;        // Preload all mob models at startup
    int modelLoadBudgetMs = 2;        // Max ms to spend loading models per frame
};

/**
 * EntityManager - Centralized entity management system
 * 
 * Features:
 * - Async model preloading to prevent spawn stutters
 * - Cached model sharing across entities of the same type
 * - Deferred spawning queue for load distribution
 * - Distance-based activation levels (full AI / reduced tick / sleep)
 * - Proper cleanup when entities go out of range
 * - Network-ready: supports server-authoritative mob handling
 * - Entity ID system for multiplayer sync
 */
class EntityManager {
public:
    EntityManager();
    ~EntityManager();
    
    // Initialize with world references
    void initialize(ChunkManager& chunkMgr, WorldGenerator& worldGen);
    
    // Configuration
    void setConfig(const EntityManagerConfig& cfg) { config = cfg; }
    const EntityManagerConfig& getConfig() const { return config; }
    
    // Model preloading (call during loading screen to avoid runtime stutters)
    void preloadModels();
    bool isModelPreloadComplete() const { return modelsPreloaded; }
    float getModelPreloadProgress() const;
    
    // Per-frame update (call from main thread)
    // Returns list of entities that attacked the player this frame
    struct AttackEvent {
        EntityId attackerId;
        glm::vec3 knockback;
    };
    std::vector<AttackEvent> update(float deltaTime, const glm::vec3& playerPos, float timeOfDay);
    
    // Spawning interface
    void queueSpawn(MobType type, const glm::vec3& position, EntityId id = INVALID_ENTITY_ID);
    void spawnImmediate(MobType type, const glm::vec3& position, EntityId id = INVALID_ENTITY_ID);
    
    // Entity access (read-only for rendering)
    std::vector<Entity*> getAllEntities() const;
    std::vector<Entity*> getEntitiesInRadius(const glm::vec3& center, float radius) const;
    Entity* getEntityById(EntityId id) const;
    
    // Entity counts
    int getHostileCount() const;
    int getPassiveCount() const;
    int getTotalCount() const;
    
    // Clear all entities
    void clear();
    
    // Damage/kill entity
    void damageEntity(EntityId id, float amount);
    void killEntity(EntityId id);
    
    // Network synchronization
    void setNetworkMode(bool isServer, bool isClient);
    
    // Server: Get entity states to broadcast
    std::vector<NetworkEntityState> getEntityStatesForSync() const;
    
    // Client: Apply entity states from server
    void applyEntityStates(const std::vector<NetworkEntityState>& states);
    
    // Server: Handle mob spawning logic
    void serverUpdateSpawning(float deltaTime, const std::vector<glm::vec3>& playerPositions, float timeOfDay);
    
private:
    // Internal entity storage with type-specific containers for cache efficiency
    struct EntityStorage {
        std::vector<std::unique_ptr<ZombieEntity>> zombies;
        std::vector<std::unique_ptr<SkeletonEntity>> skeletons;
        std::vector<std::unique_ptr<PigEntity>> pigs;
        std::vector<std::unique_ptr<ChickenEntity>> chickens;
        std::vector<std::unique_ptr<SheepEntity>> sheep;
        
        // ID lookup map
        std::unordered_map<EntityId, Entity*> idMap;
    } entities;
    
    // Model cache - shared across all entities of same type
    struct ModelCache {
        std::shared_ptr<ModelSystem::Model> zombie;
        std::shared_ptr<ModelSystem::Model> skeleton;
        std::shared_ptr<ModelSystem::Model> pig;
        std::shared_ptr<ModelSystem::Model> chicken;
        std::shared_ptr<ModelSystem::Model> sheep;
        
        std::atomic<int> loadedCount{0};
        static constexpr int TOTAL_MODELS = 5;
    } modelCache;
    
    // Spawn queue for deferred creation
    std::queue<SpawnRequest> spawnQueue;
    std::mutex spawnQueueMutex;
    
    // References
    ChunkManager* chunkManager = nullptr;
    WorldGenerator* worldGenerator = nullptr;
    
    // State
    EntityManagerConfig config;
    EntityId nextEntityId = 1;
    std::atomic<bool> modelsPreloaded{false};
    std::mt19937 rng;
    
    // Spawn timing
    float hostileSpawnTimer = 0.0f;
    float passiveSpawnTimer = 0.0f;
    
    // Network mode
    bool isServer = false;
    bool isClient = false;
    
    // Activation levels for performance
    enum class ActivationLevel {
        FULL,     // Full AI update every frame
        REDUCED,  // AI update every few frames
        SLEEP     // Minimal updates
    };
    
    // Helper methods
    EntityId generateEntityId();
    std::shared_ptr<ModelSystem::Model> getModelForType(MobType type);
    void processSpawnQueue(int maxSpawns);
    void processDespawns(const glm::vec3& playerPos);
    ActivationLevel getActivationLevel(const glm::vec3& entityPos, const glm::vec3& playerPos) const;
    
    // Spawning helpers
    static bool isNightTime(float timeOfDay);
    bool isValidHostileSpawn(const glm::vec3& pos, float timeOfDay) const;
    bool isValidPassiveSpawn(const glm::vec3& pos, float timeOfDay) const;
    glm::vec3 findSpawnPosition(const glm::vec3& playerPos, float minDist, float maxDist);
    int getLightLevel(const glm::vec3& pos, float timeOfDay) const;
    bool hasSolidGround(const glm::vec3& pos) const;
    bool hasHeadroom(const glm::vec3& pos, float height) const;
    
    // AI update helpers
    void updateEntityAI(float deltaTime, const glm::vec3& playerPos, std::vector<AttackEvent>& attacks);
};
