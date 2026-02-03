#include "EntityManager.h"
#include "ZombieEntity.h"
#include "SkeletonEntity.h"
#include "PigEntity.h"
#include "ChickenEntity.h"
#include "SheepEntity.h"
#include "../Model/Model.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../World/Block.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

EntityManager::EntityManager() 
    : rng(std::random_device{}())
{
    hostileSpawnTimer = config.spawnCheckInterval;
    passiveSpawnTimer = config.spawnCheckInterval;
}

EntityManager::~EntityManager() {
    clear();
}

void EntityManager::initialize(ChunkManager& chunkMgr, WorldGenerator& worldGen) {
    chunkManager = &chunkMgr;
    worldGenerator = &worldGen;
    LOG_INFO("EntityManager initialized");
}

void EntityManager::preloadModels() {
    if (modelsPreloaded) return;
    
    LOG_INFO("EntityManager: Preloading mob models...");
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Load all models upfront - this happens during loading screen
    // so the stutter is expected and shown as loading progress
    try {
        modelCache.zombie = std::make_shared<ModelSystem::Model>("assets/models/Zombie/Zombie_Quaternius.gltf");
        modelCache.loadedCount++;
        LOG_INFO("  Loaded Zombie model");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to preload Zombie model: " + std::string(e.what()));
    }
    
    try {
        modelCache.skeleton = std::make_shared<ModelSystem::Model>("assets/models/Skeleton/Skeleton.gltf");
        modelCache.loadedCount++;
        LOG_INFO("  Loaded Skeleton model");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to preload Skeleton model: " + std::string(e.what()));
    }
    
    try {
        modelCache.pig = std::make_shared<ModelSystem::Model>("assets/models/Pig/Pig.gltf");
        modelCache.loadedCount++;
        LOG_INFO("  Loaded Pig model");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to preload Pig model: " + std::string(e.what()));
    }
    
    try {
        modelCache.chicken = std::make_shared<ModelSystem::Model>("assets/models/Chicken/Chicken.gltf");
        modelCache.loadedCount++;
        LOG_INFO("  Loaded Chicken model");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to preload Chicken model: " + std::string(e.what()));
    }
    
    try {
        modelCache.sheep = std::make_shared<ModelSystem::Model>("assets/models/Sheep/Sheep.gltf");
        modelCache.loadedCount++;
        LOG_INFO("  Loaded Sheep model");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to preload Sheep model: " + std::string(e.what()));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    modelsPreloaded = true;
    LOG_INFO("EntityManager: Model preloading complete (" + std::to_string(modelCache.loadedCount.load()) + 
             "/" + std::to_string(ModelCache::TOTAL_MODELS) + " models) in " + std::to_string(duration) + "ms");
}

float EntityManager::getModelPreloadProgress() const {
    return static_cast<float>(modelCache.loadedCount.load()) / static_cast<float>(ModelCache::TOTAL_MODELS);
}

std::shared_ptr<ModelSystem::Model> EntityManager::getModelForType(MobType type) {
    // Clone the cached model so each entity has independent animation state
    std::shared_ptr<ModelSystem::Model> cached = nullptr;
    switch (type) {
        case MobType::ZOMBIE: cached = modelCache.zombie; break;
        case MobType::SKELETON: cached = modelCache.skeleton; break;
        case MobType::PIG: cached = modelCache.pig; break;
        case MobType::CHICKEN: cached = modelCache.chicken; break;
        case MobType::SHEEP: cached = modelCache.sheep; break;
        default: return nullptr;
    }
    
    if (cached) {
        return cached->clone();
    }
    return nullptr;
}

EntityId EntityManager::generateEntityId() {
    return nextEntityId++;
}

void EntityManager::queueSpawn(MobType type, const glm::vec3& position, EntityId id) {
    std::lock_guard<std::mutex> lock(spawnQueueMutex);
    spawnQueue.push({type, position, id, false});
}

void EntityManager::spawnImmediate(MobType type, const glm::vec3& position, EntityId id) {
    SpawnRequest req{type, position, id, true};
    
    // Get or generate ID
    EntityId entityId = (req.assignedId != INVALID_ENTITY_ID) ? req.assignedId : generateEntityId();
    
    // Get cached model
    auto model = getModelForType(type);
    if (!model) {
        LOG_ERROR("EntityManager: Model not loaded for type " + std::to_string(static_cast<int>(type)) + 
                  " - ensure preloadModels() was called");
        return;
    }
    
    LOG_INFO("EntityManager: Spawning mob type " + std::to_string(static_cast<int>(type)) + 
             " at (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + 
             ") ID=" + std::to_string(entityId));
    
    float yaw = 0.0f;  // Track yaw for spawn event
    
    switch (type) {
        case MobType::ZOMBIE: {
            auto zombie = std::make_unique<ZombieEntity>(position, model, entityId);
            yaw = zombie->getRotation().y;
            entities.idMap[entityId] = zombie.get();
            entities.zombies.push_back(std::move(zombie));
            break;
        }
        case MobType::SKELETON: {
            auto skeleton = std::make_unique<SkeletonEntity>(position, model, entityId);
            yaw = skeleton->getRotation().y;
            entities.idMap[entityId] = skeleton.get();
            entities.skeletons.push_back(std::move(skeleton));
            break;
        }
        case MobType::PIG: {
            auto pig = std::make_unique<PigEntity>(position, model, entityId);
            yaw = pig->getRotation().y;
            entities.idMap[entityId] = pig.get();
            entities.pigs.push_back(std::move(pig));
            break;
        }
        case MobType::CHICKEN: {
            auto chicken = std::make_unique<ChickenEntity>(position, model, entityId);
            yaw = chicken->getRotation().y;
            entities.idMap[entityId] = chicken.get();
            entities.chickens.push_back(std::move(chicken));
            break;
        }
        case MobType::SHEEP: {
            auto sheep = std::make_unique<SheepEntity>(position, model, entityId);
            yaw = sheep->getRotation().y;
            entities.idMap[entityId] = sheep.get();
            entities.sheep.push_back(std::move(sheep));
            break;
        }
        default:
            LOG_WARNING("Unknown mob type: " + std::to_string(static_cast<int>(type)));
            return;  // Don't record spawn event for unknown type
    }
    
    // Record spawn event for network broadcast (if server)
    if (isServer) {
        pendingSpawnEvents.push_back({entityId, type, position, yaw});
    }
}

void EntityManager::spawnFromNetwork(MobType type, const glm::vec3& position, EntityId id, float yaw) {
    // Check if entity already exists
    if (entities.idMap.find(id) != entities.idMap.end()) {
        // Already spawned, just update position
        Entity* entity = entities.idMap[id];
        entity->setPosition(position);
        entity->setRotation(glm::vec3(0.0f, yaw, 0.0f));
        return;
    }
    
    // Get cached model
    auto model = getModelForType(type);
    if (!model) {
        LOG_ERROR("EntityManager::spawnFromNetwork: Model not loaded for type " + std::to_string(static_cast<int>(type)));
        return;
    }
    
    LOG_INFO("EntityManager: Network spawn mob type " + std::to_string(static_cast<int>(type)) + 
             " ID=" + std::to_string(id) + " at (" + 
             std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
    
    switch (type) {
        case MobType::ZOMBIE: {
            auto zombie = std::make_unique<ZombieEntity>(position, model, id);
            zombie->setRotation(glm::vec3(0.0f, yaw, 0.0f));
            entities.idMap[id] = zombie.get();
            entities.zombies.push_back(std::move(zombie));
            break;
        }
        case MobType::SKELETON: {
            auto skeleton = std::make_unique<SkeletonEntity>(position, model, id);
            skeleton->setRotation(glm::vec3(0.0f, yaw, 0.0f));
            entities.idMap[id] = skeleton.get();
            entities.skeletons.push_back(std::move(skeleton));
            break;
        }
        case MobType::PIG: {
            auto pig = std::make_unique<PigEntity>(position, model, id);
            pig->setRotation(glm::vec3(0.0f, yaw, 0.0f));
            entities.idMap[id] = pig.get();
            entities.pigs.push_back(std::move(pig));
            break;
        }
        case MobType::CHICKEN: {
            auto chicken = std::make_unique<ChickenEntity>(position, model, id);
            chicken->setRotation(glm::vec3(0.0f, yaw, 0.0f));
            entities.idMap[id] = chicken.get();
            entities.chickens.push_back(std::move(chicken));
            break;
        }
        case MobType::SHEEP: {
            auto sheep = std::make_unique<SheepEntity>(position, model, id);
            sheep->setRotation(glm::vec3(0.0f, yaw, 0.0f));
            entities.idMap[id] = sheep.get();
            entities.sheep.push_back(std::move(sheep));
            break;
        }
        default:
            LOG_WARNING("spawnFromNetwork: Unknown mob type: " + std::to_string(static_cast<int>(type)));
            break;
    }
}

void EntityManager::despawnById(EntityId id) {
    auto it = entities.idMap.find(id);
    if (it == entities.idMap.end()) return;
    
    entities.idMap.erase(it);
    
    // Record despawn event for network broadcast (if server)
    if (isServer) {
        pendingDespawnEvents.push_back(id);
    }
    
    // Find and remove from type-specific container
    // Check zombies
    for (auto zit = entities.zombies.begin(); zit != entities.zombies.end(); ++zit) {
        if ((*zit)->getEntityId() == id) {
            entities.zombies.erase(zit);
            return;
        }
    }
    // Check skeletons
    for (auto sit = entities.skeletons.begin(); sit != entities.skeletons.end(); ++sit) {
        if ((*sit)->getEntityId() == id) {
            entities.skeletons.erase(sit);
            return;
        }
    }
    // Check pigs
    for (auto pit = entities.pigs.begin(); pit != entities.pigs.end(); ++pit) {
        if ((*pit)->getEntityId() == id) {
            entities.pigs.erase(pit);
            return;
        }
    }
    // Check chickens
    for (auto cit = entities.chickens.begin(); cit != entities.chickens.end(); ++cit) {
        if ((*cit)->getEntityId() == id) {
            entities.chickens.erase(cit);
            return;
        }
    }
    // Check sheep
    for (auto shit = entities.sheep.begin(); shit != entities.sheep.end(); ++shit) {
        if ((*shit)->getEntityId() == id) {
            entities.sheep.erase(shit);
            return;
        }
    }
}

void EntityManager::processSpawnQueue(int maxSpawns) {
    std::lock_guard<std::mutex> lock(spawnQueueMutex);
    
    int spawned = 0;
    while (!spawnQueue.empty() && spawned < maxSpawns) {
        SpawnRequest req = spawnQueue.front();
        spawnQueue.pop();
        
        // Check if we're at capacity
        int total = getTotalCount();
        if (total >= config.maxTotalMobs) {
            continue; // Skip this spawn, don't put back in queue
        }
        
        // Check type-specific limits
        bool canSpawn = true;
        if (req.type == MobType::ZOMBIE || req.type == MobType::SKELETON) {
            if (getHostileCount() >= config.maxHostileMobs) canSpawn = false;
        } else {
            if (getPassiveCount() >= config.maxPassiveMobs) canSpawn = false;
        }
        
        if (!canSpawn) continue;
        
        // Actually spawn
        spawnImmediate(req.type, req.position, req.assignedId);
        spawned++;
    }
}

void EntityManager::processDespawns(const glm::vec3& playerPos) {
    float despawnRadiusSq = config.despawnRadius * config.despawnRadius;
    int despawned = 0;
    
    auto shouldDespawn = [&](const glm::vec3& entityPos) -> bool {
        if (despawned >= config.maxDespawnsPerFrame) return false;
        glm::vec3 diff = entityPos - playerPos;
        float distSq = diff.x * diff.x + diff.z * diff.z; // XZ distance only
        return distSq > despawnRadiusSq;
    };
    
    // Despawn zombies
    auto& zombies = entities.zombies;
    for (auto it = zombies.begin(); it != zombies.end();) {
        // Use shouldBeRemoved() to allow death animation to play before removal
        if ((*it)->shouldBeRemoved() || shouldDespawn((*it)->getPosition())) {
            EntityId id = (*it)->getEntityId();
            if (isServer) pendingDespawnEvents.push_back(id);
            entities.idMap.erase(id);
            it = zombies.erase(it);
            despawned++;
        } else {
            ++it;
        }
    }
    
    // Despawn skeletons
    auto& skeletons = entities.skeletons;
    for (auto it = skeletons.begin(); it != skeletons.end();) {
        // Use shouldBeRemoved() to allow death animation to play before removal
        if ((*it)->shouldBeRemoved() || shouldDespawn((*it)->getPosition())) {
            EntityId id = (*it)->getEntityId();
            if (isServer) pendingDespawnEvents.push_back(id);
            entities.idMap.erase(id);
            it = skeletons.erase(it);
            despawned++;
        } else {
            ++it;
        }
    }
    
    // Despawn pigs
    auto& pigs = entities.pigs;
    for (auto it = pigs.begin(); it != pigs.end();) {
        // Use shouldBeRemoved() to allow death animation to play before removal
        if ((*it)->shouldBeRemoved() || shouldDespawn((*it)->getPosition())) {
            EntityId id = (*it)->getEntityId();
            if (isServer) pendingDespawnEvents.push_back(id);
            entities.idMap.erase(id);
            it = pigs.erase(it);
            despawned++;
        } else {
            ++it;
        }
    }
    
    // Despawn chickens
    auto& chickens = entities.chickens;
    for (auto it = chickens.begin(); it != chickens.end();) {
        // Use shouldBeRemoved() to allow death animation to play before removal
        if ((*it)->shouldBeRemoved() || shouldDespawn((*it)->getPosition())) {
            EntityId id = (*it)->getEntityId();
            if (isServer) pendingDespawnEvents.push_back(id);
            entities.idMap.erase(id);
            it = chickens.erase(it);
            despawned++;
        } else {
            ++it;
        }
    }
    
    // Despawn sheep
    auto& sheep = entities.sheep;
    for (auto it = sheep.begin(); it != sheep.end();) {
        // Use shouldBeRemoved() to allow death animation to play before removal
        if ((*it)->shouldBeRemoved() || shouldDespawn((*it)->getPosition())) {
            EntityId id = (*it)->getEntityId();
            if (isServer) pendingDespawnEvents.push_back(id);
            entities.idMap.erase(id);
            it = sheep.erase(it);
            despawned++;
        } else {
            ++it;
        }
    }
}

EntityManager::ActivationLevel EntityManager::getActivationLevel(
    const glm::vec3& entityPos, const glm::vec3& playerPos) const {
    
    glm::vec3 diff = entityPos - playerPos;
    float distSq = diff.x * diff.x + diff.z * diff.z;
    
    float activationSq = config.activationRadius * config.activationRadius;
    float sleepSq = config.sleepRadius * config.sleepRadius;
    
    if (distSq <= activationSq) return ActivationLevel::FULL;
    if (distSq <= sleepSq) return ActivationLevel::REDUCED;
    return ActivationLevel::SLEEP;
}

std::vector<EntityManager::AttackEvent> EntityManager::update(
    float deltaTime, const glm::vec3& playerPos, float timeOfDay) {
    
    std::vector<AttackEvent> attacks;
    
    if (!chunkManager) {
        LOG_WARNING("EntityManager::update - chunkManager is null!");
        return attacks;
    }
    
    if (!worldGenerator) {
        LOG_WARNING("EntityManager::update - worldGenerator is null!");
        return attacks;
    }
    
    // Process spawn queue (limited per frame to avoid stutters)
    processSpawnQueue(config.maxSpawnsPerFrame);
    
    // Despawn far entities
    processDespawns(playerPos);
    
    // Only do spawn logic if not in client mode (server/offline handles spawning)
    if (!isClient) {
        // Update spawn timers
        hostileSpawnTimer -= deltaTime;
        passiveSpawnTimer -= deltaTime;
        
        // Try to spawn hostile mobs
        if (hostileSpawnTimer <= 0.0f) {
            hostileSpawnTimer = config.spawnCheckInterval;
            
            if (getHostileCount() < config.maxHostileMobs) {
                bool spawned = false;
                for (int i = 0; i < 3; ++i) { // Multiple attempts
                    glm::vec3 spawnPos = findSpawnPosition(playerPos, config.minSpawnDistance, config.spawnRadius);
                    if (isValidHostileSpawn(spawnPos, timeOfDay)) {
                        // 66% zombie, 33% skeleton
                        std::uniform_int_distribution<int> dist(0, 2);
                        MobType type = (dist(rng) <= 1) ? MobType::ZOMBIE : MobType::SKELETON;
                        queueSpawn(type, spawnPos);
                        spawned = true;
                        break;
                    }
                }
            }
        }
        
        // Try to spawn passive mobs
        if (passiveSpawnTimer <= 0.0f) {
            passiveSpawnTimer = config.spawnCheckInterval;
            
            if (getPassiveCount() < config.maxPassiveMobs) {
                for (int i = 0; i < 3; ++i) {
                    glm::vec3 spawnPos = findSpawnPosition(playerPos, config.minSpawnDistance, config.spawnRadius * 1.5f);
                    if (isValidPassiveSpawn(spawnPos, timeOfDay)) {
                        std::uniform_int_distribution<int> dist(0, 2);
                        int mobType = dist(rng);
                        MobType type = (mobType == 0) ? MobType::PIG : 
                                       (mobType == 1) ? MobType::CHICKEN : MobType::SHEEP;
                        queueSpawn(type, spawnPos);
                        break;
                    }
                }
            }
        }
    }
    
    // Update AI based on activation level
    updateEntityAI(deltaTime, playerPos, attacks);
    
    return attacks;
}

void EntityManager::updateEntityAI(float deltaTime, const glm::vec3& playerPos, 
                                   std::vector<AttackEvent>& attacks) {
    static int frameCounter = 0;
    frameCounter++;
    
    // Update zombies (always update for death timer, updateAI handles dead state internally)
    for (auto& zombie : entities.zombies) {
        if (!zombie) continue;
        
        // Always update dead entities for death animation timer
        if (zombie->isDead()) {
            zombie->updateAI(deltaTime, *chunkManager, playerPos);
            continue;
        }
        
        ActivationLevel level = getActivationLevel(zombie->getPosition(), playerPos);
        
        bool shouldUpdate = false;
        switch (level) {
            case ActivationLevel::FULL:
                shouldUpdate = true;
                break;
            case ActivationLevel::REDUCED:
                shouldUpdate = (frameCounter % 3 == 0); // Every 3rd frame
                break;
            case ActivationLevel::SLEEP:
                shouldUpdate = (frameCounter % 10 == 0); // Every 10th frame
                break;
        }
        
        if (shouldUpdate) {
            float adjustedDelta = deltaTime;
            if (level == ActivationLevel::REDUCED) adjustedDelta *= 3.0f;
            else if (level == ActivationLevel::SLEEP) adjustedDelta *= 10.0f;
            
            bool attacked = zombie->updateAI(adjustedDelta, *chunkManager, playerPos);
            if (attacked) {
                attacks.push_back({zombie->getEntityId(), zombie->consumeAttackImpulse()});
            }
        }
    }
    
    // Update skeletons (always update for death timer, updateAI handles dead state internally)
    for (auto& skeleton : entities.skeletons) {
        if (!skeleton) continue;
        
        // Always update dead entities for death animation timer
        if (skeleton->isDead()) {
            skeleton->updateAI(deltaTime, *chunkManager, playerPos);
            continue;
        }
        
        ActivationLevel level = getActivationLevel(skeleton->getPosition(), playerPos);
        
        bool shouldUpdate = (level == ActivationLevel::FULL) ||
                           (level == ActivationLevel::REDUCED && frameCounter % 3 == 0) ||
                           (level == ActivationLevel::SLEEP && frameCounter % 10 == 0);
        
        if (shouldUpdate) {
            float adjustedDelta = deltaTime;
            if (level == ActivationLevel::REDUCED) adjustedDelta *= 3.0f;
            else if (level == ActivationLevel::SLEEP) adjustedDelta *= 10.0f;
            
            bool attacked = skeleton->updateAI(adjustedDelta, *chunkManager, playerPos);
            if (attacked) {
                attacks.push_back({skeleton->getEntityId(), skeleton->consumeAttackImpulse()});
            }
        }
    }
    
    // Update passive mobs (always update for death timer, updateAI handles dead state internally)
    for (auto& pig : entities.pigs) {
        if (!pig) continue;
        // Always update dead entities for death animation timer
        if (pig->isDead()) {
            pig->updateAI(deltaTime, *chunkManager);
            continue;
        }
        ActivationLevel level = getActivationLevel(pig->getPosition(), playerPos);
        if (level != ActivationLevel::SLEEP || frameCounter % 10 == 0) {
            pig->updateAI(deltaTime, *chunkManager);
        }
    }
    
    for (auto& chicken : entities.chickens) {
        if (!chicken) continue;
        // Always update dead entities for death animation timer
        if (chicken->isDead()) {
            chicken->updateAI(deltaTime, *chunkManager);
            continue;
        }
        ActivationLevel level = getActivationLevel(chicken->getPosition(), playerPos);
        if (level != ActivationLevel::SLEEP || frameCounter % 10 == 0) {
            chicken->updateAI(deltaTime, *chunkManager);
        }
    }
    
    for (auto& sheep : entities.sheep) {
        if (!sheep) continue;
        // Always update dead entities for death animation timer
        if (sheep->isDead()) {
            sheep->updateAI(deltaTime, *chunkManager);
            continue;
        }
        ActivationLevel level = getActivationLevel(sheep->getPosition(), playerPos);
        if (level != ActivationLevel::SLEEP || frameCounter % 10 == 0) {
            sheep->updateAI(deltaTime, *chunkManager);
        }
    }
}

std::vector<Entity*> EntityManager::getAllEntities() const {
    std::vector<Entity*> result;
    result.reserve(getTotalCount());
    
    // Include all entities (even dead ones) so death animations can play
    for (const auto& z : entities.zombies) {
        if (z) result.push_back(z.get());
    }
    for (const auto& s : entities.skeletons) {
        if (s) result.push_back(s.get());
    }
    for (const auto& p : entities.pigs) {
        if (p) result.push_back(p.get());
    }
    for (const auto& c : entities.chickens) {
        if (c) result.push_back(c.get());
    }
    for (const auto& s : entities.sheep) {
        if (s) result.push_back(s.get());
    }
    
    return result;
}

std::vector<Entity*> EntityManager::getEntitiesInRadius(const glm::vec3& center, float radius) const {
    std::vector<Entity*> result;
    float radiusSq = radius * radius;
    
    auto inRange = [&](const glm::vec3& pos) -> bool {
        glm::vec3 diff = pos - center;
        return (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) <= radiusSq;
    };
    
    // Include all entities (even dead ones) so death animations can play
    for (const auto& z : entities.zombies) {
        if (z && inRange(z->getPosition())) result.push_back(z.get());
    }
    for (const auto& s : entities.skeletons) {
        if (s && inRange(s->getPosition())) result.push_back(s.get());
    }
    for (const auto& p : entities.pigs) {
        if (p && inRange(p->getPosition())) result.push_back(p.get());
    }
    for (const auto& c : entities.chickens) {
        if (c && inRange(c->getPosition())) result.push_back(c.get());
    }
    for (const auto& s : entities.sheep) {
        if (s && inRange(s->getPosition())) result.push_back(s.get());
    }
    
    return result;
}

Entity* EntityManager::getEntityById(EntityId id) const {
    auto it = entities.idMap.find(id);
    return (it != entities.idMap.end()) ? it->second : nullptr;
}

int EntityManager::getHostileCount() const {
    return static_cast<int>(entities.zombies.size() + entities.skeletons.size());
}

int EntityManager::getPassiveCount() const {
    return static_cast<int>(entities.pigs.size() + entities.chickens.size() + entities.sheep.size());
}

int EntityManager::getTotalCount() const {
    return getHostileCount() + getPassiveCount();
}

void EntityManager::clear() {
    entities.zombies.clear();
    entities.skeletons.clear();
    entities.pigs.clear();
    entities.chickens.clear();
    entities.sheep.clear();
    entities.idMap.clear();
    
    std::lock_guard<std::mutex> lock(spawnQueueMutex);
    while (!spawnQueue.empty()) spawnQueue.pop();
}

void EntityManager::damageEntity(EntityId id, float amount) {
    Entity* entity = getEntityById(id);
    if (!entity) return;
    
    // Check type and apply damage
    for (auto& s : entities.skeletons) {
        if (s && s->getEntityId() == id) {
            s->takeDamage(amount);
            return;
        }
    }
    // Add damage handling for other entity types as needed
}

void EntityManager::killEntity(EntityId id) {
    damageEntity(id, 1000.0f); // Massive damage to kill
}

void EntityManager::setNetworkMode(bool server, bool client) {
    isServer = server;
    isClient = client;
    LOG_INFO("EntityManager network mode: server=" + std::to_string(server) + 
             ", client=" + std::to_string(client));
}

std::vector<EntityManager::SpawnEvent> EntityManager::consumeSpawnEvents() {
    std::vector<SpawnEvent> events = std::move(pendingSpawnEvents);
    pendingSpawnEvents.clear();
    return events;
}

std::vector<EntityId> EntityManager::consumeDespawnEvents() {
    std::vector<EntityId> events = std::move(pendingDespawnEvents);
    pendingDespawnEvents.clear();
    return events;
}

std::vector<NetworkEntityState> EntityManager::getEntityStatesForSync() const {
    std::vector<NetworkEntityState> states;
    states.reserve(getTotalCount());
    
    for (const auto& z : entities.zombies) {
        if (!z) continue;
        NetworkEntityState state;
        state.id = z->getEntityId();
        state.type = MobType::ZOMBIE;
        state.position = z->getPosition();
        state.velocity = z->getVelocity();
        state.yaw = z->getRotation().y;
        states.push_back(state);
    }
    
    for (const auto& s : entities.skeletons) {
        if (!s || s->isDead()) continue;
        NetworkEntityState state;
        state.id = s->getEntityId();
        state.type = MobType::SKELETON;
        state.position = s->getPosition();
        state.velocity = s->getVelocity();
        state.yaw = s->getRotation().y;
        state.health = s->getHealth();
        state.isDead = s->isDead();
        states.push_back(state);
    }
    
    for (const auto& p : entities.pigs) {
        if (!p || p->isDead()) continue;
        NetworkEntityState state;
        state.id = p->getEntityId();
        state.type = MobType::PIG;
        state.position = p->getPosition();
        state.velocity = p->getVelocity();
        state.yaw = p->getRotation().y;
        states.push_back(state);
    }
    
    for (const auto& c : entities.chickens) {
        if (!c || c->isDead()) continue;
        NetworkEntityState state;
        state.id = c->getEntityId();
        state.type = MobType::CHICKEN;
        state.position = c->getPosition();
        state.velocity = c->getVelocity();
        state.yaw = c->getRotation().y;
        states.push_back(state);
    }
    
    for (const auto& s : entities.sheep) {
        if (!s || s->isDead()) continue;
        NetworkEntityState state;
        state.id = s->getEntityId();
        state.type = MobType::SHEEP;
        state.position = s->getPosition();
        state.velocity = s->getVelocity();
        state.yaw = s->getRotation().y;
        states.push_back(state);
    }
    
    return states;
}

void EntityManager::applyEntityStates(const std::vector<NetworkEntityState>& states) {
    // Track which IDs we received to detect removals
    std::unordered_set<EntityId> receivedIds;
    
    for (const auto& state : states) {
        receivedIds.insert(state.id);
        
        Entity* existing = getEntityById(state.id);
        
        if (existing) {
            // Update existing entity
            existing->setPosition(state.position);
            existing->setVelocity(state.velocity);
            existing->setRotation(glm::vec3(0.0f, state.yaw, 0.0f));
        } else {
            // Spawn new entity
            spawnImmediate(state.type, state.position, state.id);
        }
    }
    
    // Remove entities not in the server state
    // (This is a simple approach - could be optimized)
    auto removeIfNotReceived = [&](auto& container) {
        container.erase(
            std::remove_if(container.begin(), container.end(),
                [&](const auto& entity) {
                    if (!entity) return true;
                    bool shouldRemove = receivedIds.find(entity->getEntityId()) == receivedIds.end();
                    if (shouldRemove) {
                        entities.idMap.erase(entity->getEntityId());
                    }
                    return shouldRemove;
                }),
            container.end());
    };
    
    removeIfNotReceived(entities.zombies);
    removeIfNotReceived(entities.skeletons);
    removeIfNotReceived(entities.pigs);
    removeIfNotReceived(entities.chickens);
    removeIfNotReceived(entities.sheep);
}

void EntityManager::serverUpdateSpawning(float deltaTime, 
                                         const std::vector<glm::vec3>& playerPositions,
                                         float timeOfDay) {
    if (playerPositions.empty()) return;
    
    // Use first player as reference for now (could be improved to use nearest)
    const glm::vec3& refPos = playerPositions[0];
    
    // Same spawning logic as single-player but with network sync consideration
    hostileSpawnTimer -= deltaTime;
    passiveSpawnTimer -= deltaTime;
    
    if (hostileSpawnTimer <= 0.0f) {
        hostileSpawnTimer = config.spawnCheckInterval;
        
        if (getHostileCount() < config.maxHostileMobs) {
            for (int i = 0; i < 3; ++i) {
                glm::vec3 spawnPos = findSpawnPosition(refPos, config.minSpawnDistance, config.spawnRadius);
                if (isValidHostileSpawn(spawnPos, timeOfDay)) {
                    std::uniform_int_distribution<int> dist(0, 2);
                    MobType type = (dist(rng) <= 1) ? MobType::ZOMBIE : MobType::SKELETON;
                    spawnImmediate(type, spawnPos); // Immediate for server
                    break;
                }
            }
        }
    }
    
    if (passiveSpawnTimer <= 0.0f) {
        passiveSpawnTimer = config.spawnCheckInterval;
        
        if (getPassiveCount() < config.maxPassiveMobs) {
            for (int i = 0; i < 3; ++i) {
                glm::vec3 spawnPos = findSpawnPosition(refPos, config.minSpawnDistance, config.spawnRadius * 1.5f);
                if (isValidPassiveSpawn(spawnPos, timeOfDay)) {
                    std::uniform_int_distribution<int> dist(0, 2);
                    int mobType = dist(rng);
                    MobType type = (mobType == 0) ? MobType::PIG : 
                                   (mobType == 1) ? MobType::CHICKEN : MobType::SHEEP;
                    spawnImmediate(type, spawnPos);
                    break;
                }
            }
        }
    }
}

// Spawning helper implementations
bool EntityManager::isNightTime(float timeOfDay) {
    float angle = timeOfDay * 6.28318530718f;
    float sunY = std::sin(angle);
    return sunY < -0.1f;
}

glm::vec3 EntityManager::findSpawnPosition(const glm::vec3& playerPos, float minDist, float maxDist) {
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> radiusDist(minDist, maxDist);
    
    float angle = angleDist(rng);
    float radius = radiusDist(rng);
    
    float spawnX = playerPos.x + std::cos(angle) * radius;
    float spawnZ = playerPos.z + std::sin(angle) * radius;
    
    int surfaceY = worldGenerator->getSurfaceHeight(
        static_cast<int>(std::floor(spawnX)),
        static_cast<int>(std::floor(spawnZ)));
    
    if (surfaceY < SEA_LEVEL) {
        surfaceY = SEA_LEVEL + 1;
    }
    
    // Spawn above the surface block to avoid embedding mobs in terrain
    return glm::vec3(spawnX, static_cast<float>(surfaceY) + 0.1f, spawnZ);
}

int EntityManager::getLightLevel(const glm::vec3& pos, float timeOfDay) const {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));
    
    bool underground = false;
    for (int checkY = y + 1; checkY < y + 32 && checkY < 256; ++checkY) {
        Block block = chunkManager->getBlockAt(x, checkY, z);
        if (block.isSolid() && block.getType() != BlockType::LEAVES) {
            underground = true;
            break;
        }
    }
    
    if (underground) return 0;
    
    bool isNight = timeOfDay < 0.25f || timeOfDay > 0.75f;
    return isNight ? 4 : 15;
}

bool EntityManager::hasSolidGround(const glm::vec3& pos) const {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y)) - 1;
    int z = static_cast<int>(std::floor(pos.z));
    
    Block block = chunkManager->getBlockAt(x, y, z);
    return block.isSolid();
}

bool EntityManager::hasHeadroom(const glm::vec3& pos, float height) const {
    int x = static_cast<int>(std::floor(pos.x));
    int baseY = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));
    
    int blocksNeeded = static_cast<int>(std::ceil(height));
    
    for (int dy = 0; dy < blocksNeeded; ++dy) {
        Block block = chunkManager->getBlockAt(x, baseY + dy, z);
        if (block.isSolid()) return false;
    }
    
    return true;
}

bool EntityManager::hasSideClearance(const glm::vec3& pos, int radius, int heightBlocks) const {
    int x = static_cast<int>(std::floor(pos.x));
    int baseY = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));

    for (int dy = 0; dy < heightBlocks; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue; // skip center column
                if (std::abs(dx) + std::abs(dz) > 1) continue; // cardinal neighbors only
                Block block = chunkManager->getBlockAt(x + dx, baseY + dy, z + dz);
                if (block.isSolid()) return false;
            }
        }
    }
    return true;
}

bool EntityManager::isValidHostileSpawn(const glm::vec3& pos, float timeOfDay) const {
    if (!isNightTime(timeOfDay)) return false;
    int light = getLightLevel(pos, timeOfDay);
    if (light > 7) return false;
    
    if (!hasSolidGround(pos)) return false;
    if (!hasHeadroom(pos, 2.0f)) return false;
    if (!hasSideClearance(pos, 1, 2)) return false;
    
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y)) - 1;
    int z = static_cast<int>(std::floor(pos.z));
    Block blockBelow = chunkManager->getBlockAt(x, y, z);
    
    if (blockBelow.isWater()) return false;
    
    return true;
}

bool EntityManager::isValidPassiveSpawn(const glm::vec3& pos, float timeOfDay) const {
    // Passive mobs spawn during daytime on grass/dirt/snow
    // They can also spawn underground (caves) at any time
    
    if (!hasSolidGround(pos)) return false;
    if (!hasHeadroom(pos, 1.5f)) return false;
    if (!hasSideClearance(pos, 1, 2)) return false;
    
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y)) - 1;
    int z = static_cast<int>(std::floor(pos.z));
    Block blockBelow = chunkManager->getBlockAt(x, y, z);
    
    BlockType type = blockBelow.getType();
    if (type != BlockType::GRASS && type != BlockType::DIRT && type != BlockType::SNOW) {
        return false;
    }
    
    return true;
}
