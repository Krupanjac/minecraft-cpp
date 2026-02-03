#include "MenuWorldSystem.h"
#include "../Core/Logger.h"

#include <chrono>

MenuWorldSystem::MenuWorldSystem(ChunkManager& chunkManagerRef,
                                 WorldGenerator& worldGeneratorRef,
                                 MeshBuilder& meshBuilderRef,
                                 Renderer& rendererRef,
                                 ThreadPool& threadPoolRef,
                                 std::mutex& meshMutexRef,
                                 std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshesRef)
    : chunkManager(chunkManagerRef),
      worldGenerator(worldGeneratorRef),
      meshBuilder(meshBuilderRef),
      renderer(rendererRef),
      threadPool(threadPoolRef),
      meshMutex(meshMutexRef),
      pendingMeshes(pendingMeshesRef) {
}

void MenuWorldSystem::initialize(const LoadingProgressCallback& onLoadingProgress) {
    LOG_INFO("Initializing menu background world...");

    threadPool.wait();
    {
        std::lock_guard<std::mutex> lock(meshMutex);
        pendingMeshes.clear();
    }

    chunkManager.unloadAll();
    chunkManager.clear();
    renderer.clear();

    if (!menuSeedInitialized) {
        menuWorldSeed = static_cast<long>(std::chrono::system_clock::now().time_since_epoch().count());
        menuSeedInitialized = true;
        LOG_INFO("Menu world seed initialized: " + std::to_string(menuWorldSeed));
    } else {
        LOG_INFO("Reusing cached menu world seed: " + std::to_string(menuWorldSeed));
    }
    worldGenerator.setSeed(static_cast<unsigned int>(menuWorldSeed));

    int centerX = 0;
    int centerZ = 0;
    int terrainHeight = worldGenerator.getSurfaceHeight(centerX, centerZ);
    float baseY = static_cast<float>(terrainHeight) + 25.0f;

    menuScenePositions[0] = glm::vec3(centerX, baseY, centerZ);
    menuScenePositions[1] = glm::vec3(centerX + 40.0f, baseY + 8.0f, centerZ + 35.0f);
    menuScenePositions[2] = glm::vec3(centerX - 35.0f, baseY + 5.0f, centerZ + 25.0f);
    menuScenePositions[3] = glm::vec3(centerX + 25.0f, baseY + 3.0f, centerZ - 40.0f);

    menuCamera.setPosition(menuScenePositions[0]);
    menuCamera.setYaw(menuCameraYaw);
    menuCamera.setPitch(-15.0f);
    menuCameraPitch = -15.0f;
    menuCamera.setFov(70.0f);

    int menuRadius = 6;
    int totalChunks = (menuRadius * 2 + 1) * (menuRadius * 2 + 1) * 8;
    int generatedChunks = 0;

    bool usingCache = menuCacheValid && !menuChunkCache.empty();
    if (usingCache) {
        LOG_INFO("Restoring menu world from cache (" + std::to_string(menuChunkCache.size()) + " chunks)...");
    } else {
        LOG_INFO("Generating menu world chunks with loading screen...");
    }

    for (int x = -menuRadius; x <= menuRadius; x++) {
        for (int z = -menuRadius; z <= menuRadius; z++) {
            for (int y = 0; y < 8; y++) {
                ChunkPos pos = ChunkManager::worldToChunk(glm::vec3(centerX, 0, centerZ) + glm::vec3(x * CHUNK_SIZE, y * CHUNK_SIZE - 64, z * CHUNK_SIZE));
                chunkManager.requestChunkGeneration(pos);
                auto chunk = chunkManager.getChunk(pos);

                if (chunk && chunk->getState() == ChunkState::UNLOADED) {
                    auto cacheIt = menuChunkCache.find(pos);
                    if (usingCache && cacheIt != menuChunkCache.end()) {
                        chunk->setBlocks(cacheIt->second);
                    } else {
                        worldGenerator.generate(chunk);
                        menuChunkCache[pos] = chunk->getBlocks();
                    }
                    chunk->setState(ChunkState::MESH_BUILD);
                }
                generatedChunks++;

                if (generatedChunks % 20 == 0 && onLoadingProgress) {
                    float progress = static_cast<float>(generatedChunks) / static_cast<float>(totalChunks) * 0.6f;
                    onLoadingProgress(progress);
                }
            }
        }
    }

    menuCacheValid = true;

    LOG_INFO("Building menu world meshes...");
    auto chunksToMesh = chunkManager.getChunksToMesh(menuCamera.getPosition(), 1000);
    int totalMeshes = static_cast<int>(chunksToMesh.size());
    int meshedCount = 0;

    for (auto& chunk : chunksToMesh) {
        auto neighbors = chunkManager.getNeighbors(chunk->getPosition());
        MeshData meshData = meshBuilder.buildChunkMesh(chunk,
            neighbors[0], neighbors[1], neighbors[2], neighbors[3], neighbors[4], neighbors[5], chunk->getCurrentLOD());
        renderer.uploadChunkMesh(chunk->getPosition(),
            meshData.vertices, meshData.indices,
            meshData.waterVertices, meshData.waterIndices);
        chunk->setState(ChunkState::GPU_UPLOADED);
        meshedCount++;

        if (meshedCount % 10 == 0 && onLoadingProgress) {
            float progress = 0.6f + (static_cast<float>(meshedCount) / static_cast<float>(totalMeshes)) * 0.4f;
            onLoadingProgress(progress);
        }
    }

    if (onLoadingProgress) {
        onLoadingProgress(1.0f);
    }

    menuWorldInitialized = true;
    LOG_INFO("Menu background world initialized with " + std::to_string(generatedChunks) + " chunks" +
             (usingCache ? " (from cache)" : " (newly generated)"));
}

void MenuWorldSystem::update(float deltaTime) {
    if (!menuWorldInitialized) {
        return;
    }
    updateCamera(deltaTime);
}

Camera& MenuWorldSystem::getCamera() {
    return menuCamera;
}

const Camera& MenuWorldSystem::getCamera() const {
    return menuCamera;
}

bool MenuWorldSystem::isInitialized() const {
    return menuWorldInitialized;
}

void MenuWorldSystem::invalidate() {
    menuWorldInitialized = false;
}

void MenuWorldSystem::updateCamera(float deltaTime) {
    menuCameraYaw += deltaTime * 3.0f;
    if (menuCameraYaw > 360.0f) menuCameraYaw -= 360.0f;

    menuCamera.setYaw(menuCameraYaw);
    menuCamera.setPitch(menuCameraPitch);

    menuCameraTimer += deltaTime;
    if (menuCameraTimer >= 20.0f) {
        menuCameraTimer = 0.0f;
        menuSceneIndex = (menuSceneIndex + 1) % NUM_MENU_SCENES;
    }

    glm::vec3 targetPos = menuScenePositions[menuSceneIndex];
    glm::vec3 currentPos = menuCamera.getPosition();
    glm::vec3 newPos = currentPos + (targetPos - currentPos) * deltaTime * 0.3f;
    menuCamera.setPosition(newPos);
}
