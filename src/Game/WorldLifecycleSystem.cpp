#include "WorldLifecycleSystem.h"

#include "../Core/Logger.h"
#include "../Core/ThreadPool.h"
#include "../Entity/ChickenEntity.h"
#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"
#include "../Entity/MobSpawnManager.h"
#include "../Entity/PigEntity.h"
#include "../Entity/PlayerEntity.h"
#include "../Entity/SheepEntity.h"
#include "../Entity/SkeletonEntity.h"
#include "../Entity/ZombieEntity.h"
#include "../Network/NetworkManager.h"
#include "../Physics/PhysicsTest.h"
#include "../Render/Renderer.h"
#include "../Render/Camera.h"
#include "../UI/UIManager.h"
#include "../World/Block.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../World/WorldSerializer.h"
#include "../World/FireSystem.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Game/PlayerHealthSystem.h"
#include "../Util/Config.h"

WorldLifecycleSystem::WorldLifecycleSystem(Renderer& rendererRef,
                                           ChunkManager& chunkManagerRef,
                                           WorldGenerator& worldGeneratorRef,
                                           MeshBuilder& meshBuilderRef,
                                           ThreadPool& threadPoolRef,
                                           std::mutex& meshMutexRef,
                                           std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshesRef,
                                           UIManager& uiManagerRef,
                                           EntityManager& entityManagerRef,
                                           Network::NetworkManager& networkManagerRef,
                                           PlayerHealthSystem& playerHealthSystemRef,
                                           ExplosionVolumeSystem& explosionVolumesRef,
                                           FireSystem& fireSystemRef,
                                           Camera& cameraRef,
                                           std::unique_ptr<PlayerEntity>& playerEntityRef,
                                           std::unique_ptr<MobSpawnManager>& mobSpawnManagerRef,
                                           std::vector<std::unique_ptr<ZombieEntity>>& zombiesRef,
                                           std::vector<std::unique_ptr<SkeletonEntity>>& skeletonsRef,
                                           std::vector<std::unique_ptr<PigEntity>>& pigsRef,
                                           std::vector<std::unique_ptr<ChickenEntity>>& chickensRef,
                                           std::vector<std::unique_ptr<SheepEntity>>& sheepRef,
                                           bool& useNewEntityManagerRef,
                                           Physics::PhysicsTestSystem& physicsTestRef,
                                           std::string& currentWorldNameRef,
                                           long& currentSeedRef)
        : renderer(rendererRef),
      chunkManager(chunkManagerRef),
      worldGenerator(worldGeneratorRef),
      meshBuilder(meshBuilderRef),
      threadPool(threadPoolRef),
      meshMutex(meshMutexRef),
      pendingMeshes(pendingMeshesRef),
      uiManager(uiManagerRef),
      entityManager(entityManagerRef),
      networkManager(networkManagerRef),
      playerHealthSystem(playerHealthSystemRef),
      explosionVolumes(explosionVolumesRef),
      fireSystem(fireSystemRef),
      camera(cameraRef),
      playerEntity(playerEntityRef),
      mobSpawnManager(mobSpawnManagerRef),
      zombies(zombiesRef),
      skeletons(skeletonsRef),
      pigs(pigsRef),
      chickens(chickensRef),
      sheep(sheepRef),
      useNewEntityManager(useNewEntityManagerRef),
      physicsTest(physicsTestRef),
      currentWorldName(currentWorldNameRef),
      currentSeed(currentSeedRef) {
}

void WorldLifecycleSystem::setLoadingCallback(const LoadingProgressCallback& callback) {
    loadingCallback = callback;
}

void WorldLifecycleSystem::setShouldCloseCallback(const std::function<bool()>& callback) {
    shouldCloseCallback = callback;
}

void WorldLifecycleSystem::renderLoadingProgress(float progress) {
    if (loadingCallback) {
        loadingCallback(progress);
    }
}

void WorldLifecycleSystem::createWorld(const std::string& name, long seed) {
    LOG_INFO("Creating new world: " + name + " with seed: " + std::to_string(seed));

    currentSeed = seed;
    currentWorldName = name.empty() ? "World_" + std::to_string(seed) : name;

    threadPool.wait();
    {
        std::lock_guard<std::mutex> lock(meshMutex);
        pendingMeshes.clear();
    }

    worldGenerator.setSeed(static_cast<unsigned int>(seed));

    chunkManager.unloadAll();
    chunkManager.clear();
    renderer.clear();

    int spawnX = 0;
    int spawnZ = 0;
    int searchRadius = 0;
    bool foundLand = false;

    if (worldGenerator.getSurfaceHeight(0, 0) >= SEA_LEVEL) {
        foundLand = true;
    }

    while (!foundLand && searchRadius < 10000) {
        searchRadius += 64;

        if (worldGenerator.getSurfaceHeight(searchRadius, 0) >= SEA_LEVEL) { spawnX = searchRadius; spawnZ = 0; foundLand = true; break; }
        if (worldGenerator.getSurfaceHeight(-searchRadius, 0) >= SEA_LEVEL) { spawnX = -searchRadius; spawnZ = 0; foundLand = true; break; }
        if (worldGenerator.getSurfaceHeight(0, searchRadius) >= SEA_LEVEL) { spawnX = 0; spawnZ = searchRadius; foundLand = true; break; }
        if (worldGenerator.getSurfaceHeight(0, -searchRadius) >= SEA_LEVEL) { spawnX = 0; spawnZ = -searchRadius; foundLand = true; break; }
    }

    if (foundLand && (spawnX != 0 || spawnZ != 0)) {
        LOG_INFO("Spawn moved to (" + std::to_string(spawnX) + ", " + std::to_string(spawnZ) + ") to avoid ocean.");
    }

    int terrainHeight = worldGenerator.getSurfaceHeight(spawnX, spawnZ);
    float initialSpawnY = static_cast<float>(terrainHeight) + 30.0f;

    camera.setPosition(glm::vec3(static_cast<float>(spawnX), initialSpawnY, static_cast<float>(spawnZ)));
    camera.setYaw(-90.0f);
    camera.setPitch(0.0f);
    camera.velocity = glm::vec3(0.0f);

    int initialRadius = 4;

    bool initialGenDone = false;
    while (!initialGenDone && !(shouldCloseCallback && shouldCloseCallback())) {
        auto chunksToGen = chunkManager.getChunksToGenerate(camera.getPosition(), initialRadius, 10000);

        if (chunksToGen.empty()) {
            initialGenDone = true;
        } else {
            int generated = 0;
            int totalChunks = static_cast<int>(chunksToGen.size());

            for (const auto& pos : chunksToGen) {
                chunkManager.requestChunkGeneration(pos);
                auto chunk = chunkManager.getChunk(pos);
                if (chunk) {
                    if (chunkManager.hasPreloadedData(pos)) {
                        auto blocks = chunkManager.getPreloadedData(pos);
                        std::copy(blocks.begin(), blocks.end(), chunk->getBlocks().begin());
                        chunk->setModified(true);
                    } else {
                        worldGenerator.generate(chunk);
                    }

                    chunk->setState(ChunkState::MESH_BUILD);

                    auto neighbors = chunkManager.getNeighbors(pos);
                    for (auto& n : neighbors) {
                        if (n && n->getState() != ChunkState::UNLOADED) {
                            n->setState(ChunkState::MESH_BUILD);
                        }
                    }
                }
                generated++;

                if (generated % 5 == 0) {
                    float progress = static_cast<float>(generated) / static_cast<float>(totalChunks) * 0.5f;
                    renderLoadingProgress(progress);
                }
            }
        }
    }

    LOG_INFO("Building initial meshes...");
    bool initialLoadDone = false;
    int meshedCount = 0;
    int totalInitialChunks = (initialRadius * 2 + 1) * (initialRadius * 2 + 1) * 5;

    while (!initialLoadDone && !(shouldCloseCallback && shouldCloseCallback())) {
        auto chunksToMesh = chunkManager.getChunksToMesh(camera.getPosition(), 100);

        if (chunksToMesh.empty()) {
            initialLoadDone = true;
        } else {
            for (auto& chunk : chunksToMesh) {
                auto neighbors = chunkManager.getNeighbors(chunk->getPosition());

                MeshData meshData = meshBuilder.buildChunkMesh(chunk,
                    neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], chunk->getCurrentLOD());

                renderer.uploadChunkMesh(chunk->getPosition(),
                    meshData.vertices, meshData.indices,
                    meshData.waterVertices, meshData.waterIndices);

                chunk->setState(ChunkState::GPU_UPLOADED);
                meshedCount++;
            }

            float progress = 0.5f + (static_cast<float>(meshedCount) / static_cast<float>(totalInitialChunks)) * 0.5f;
            if (progress > 1.0f) progress = 1.0f;

            renderLoadingProgress(progress);
        }
    }

    glm::vec3 spawnPos = camera.getPosition();
    spawnPos.z -= 5.0f;
    spawnPos.y = chunkManager.getHeightAt(static_cast<int>(spawnPos.x), static_cast<int>(spawnPos.z)) + 2.0f;

    playerEntity = std::make_unique<PlayerEntity>(spawnPos);

    int scanStartY = static_cast<int>(camera.getPosition().y);
    int currentSpawnX = static_cast<int>(camera.getPosition().x);
    int currentSpawnZ = static_cast<int>(camera.getPosition().z);

    bool foundGround = false;
    for (int y = scanStartY; y > 0; --y) {
        Block block = chunkManager.getBlockAt(currentSpawnX, y, currentSpawnZ);
        if (block.getType() != BlockType::AIR) {
            camera.setPosition(glm::vec3(static_cast<float>(currentSpawnX), static_cast<float>(y) + 2.5f, static_cast<float>(currentSpawnZ)));
            camera.velocity = glm::vec3(0.0f);
            LOG_INFO("Spawn position refined to Y=" + std::to_string(y + 2.5f));
            foundGround = true;
            break;
        }
    }

    if (!foundGround) {
        LOG_INFO("Could not find ground via raycast, using default height.");
    }

    if (useNewEntityManager) {
        entityManager.initialize(chunkManager, worldGenerator);

        LOG_INFO("Preloading mob models...");
        renderLoadingProgress(0.95f);
        entityManager.preloadModels();

        entityManager.setNetworkMode(networkManager.isHost(), networkManager.isClient());
    }

    mobSpawnManager = std::make_unique<MobSpawnManager>(chunkManager, worldGenerator);

#if ENABLE_PHYSICS_TEST
    initializePhysicsTest();
#endif
}

bool WorldLifecycleSystem::loadWorld(const std::string& name) {
    LOG_INFO("Loading world: " + name);

    threadPool.wait();
    {
        std::lock_guard<std::mutex> lock(meshMutex);
        pendingMeshes.clear();
    }

    chunkManager.unloadAll();
    chunkManager.clear();
    renderer.clear();
    entityManager.clear();

    glm::vec3 playerPos;
    long seed;

    if (WorldSerializer::loadWorld(name, chunkManager, playerPos, seed)) {
        camera.setPosition(playerPos);
        currentWorldName = name;
        currentSeed = seed;
        worldGenerator.setSeed(static_cast<unsigned int>(seed));

        playerEntity = std::make_unique<PlayerEntity>(playerPos);

        if (useNewEntityManager) {
            entityManager.initialize(chunkManager, worldGenerator);
            if (!entityManager.isModelPreloadComplete()) {
                entityManager.preloadModels();
            }
        }

        mobSpawnManager = std::make_unique<MobSpawnManager>(chunkManager, worldGenerator);

#if ENABLE_PHYSICS_TEST
        initializePhysicsTest();
#endif

        LOG_INFO("World loaded successfully");
        return true;
    }

    LOG_ERROR("Failed to load world");
    return false;
}

#if ENABLE_PHYSICS_TEST
std::vector<Entity*> WorldLifecycleSystem::collectEntities() {
    std::vector<Entity*> entities;
    if (useNewEntityManager) {
        auto managed = entityManager.getAllEntities();
        entities.insert(entities.end(), managed.begin(), managed.end());
    } else {
        for (auto& z : zombies) { if (z) entities.push_back(z.get()); }
        for (auto& s : skeletons) { if (s) entities.push_back(s.get()); }
        for (auto& p : pigs) { if (p) entities.push_back(p.get()); }
        for (auto& c : chickens) { if (c) entities.push_back(c.get()); }
        for (auto& s : sheep) { if (s) entities.push_back(s.get()); }
    }
    auto remotePlayers = networkManager.getRemotePlayerEntities();
    for (auto* rp : remotePlayers) { if (rp) entities.push_back(rp); }
    return entities;
}

void WorldLifecycleSystem::initializePhysicsTest() {
    auto entityProvider = [this]() {
        return collectEntities();
    };

    auto playerDamage = [this](float amount, const glm::vec3& knockback) {
        playerHealthSystem.takeDamage(amount, knockback);
    };

    auto fireStart = [this](const glm::ivec3& pos) {
        Block block = chunkManager.getBlockAt(pos.x, pos.y, pos.z);
        if (block.isFlammable()) {
            fireSystem.igniteBlock(pos, 4.0f, true, true);

            BlockType t = block.getType();
            if (t == BlockType::TALL_GRASS || t == BlockType::ROSE) {
                glm::ivec3 belowPos = pos + glm::ivec3(0, -1, 0);
                Block below = chunkManager.getBlockAt(belowPos.x, belowPos.y, belowPos.z);
                if (below.getType() == BlockType::GRASS) {
                    fireSystem.igniteBlock(belowPos, 4.0f, true, true);
                }
            }
        } else {
            glm::ivec3 topPos = pos + glm::ivec3(0, 1, 0);
            Block above = chunkManager.getBlockAt(topPos.x, topPos.y, topPos.z);
            if (above.isSolid()) {
                topPos = pos;
            }
            fireSystem.igniteBlock(topPos, 3.0f, false, false);
        }
    };

    physicsTest.initialize(&chunkManager, &this->camera, &this->explosionVolumes, entityProvider, playerDamage, fireStart);
    LOG_INFO("Physics test system ready - Press P to toggle, X/C for explosions");
}
#endif
