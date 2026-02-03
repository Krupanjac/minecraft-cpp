#pragma once

#include "../World/Chunk.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../Mesh/MeshBuilder.h"
#include "../Render/Renderer.h"
#include "../Core/ThreadPool.h"
#include "../Render/Camera.h"

#include <array>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class MenuWorldSystem {
public:
    using LoadingProgressCallback = std::function<void(float)>;

    MenuWorldSystem(ChunkManager& chunkManager,
                    WorldGenerator& worldGenerator,
                    MeshBuilder& meshBuilder,
                    Renderer& renderer,
                    ThreadPool& threadPool,
                    std::mutex& meshMutex,
                    std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes);

    void initialize(const LoadingProgressCallback& onLoadingProgress);
    void update(float deltaTime);

    Camera& getCamera();
    const Camera& getCamera() const;
    bool isInitialized() const;
    void invalidate();

private:
    void updateCamera(float deltaTime);

    ChunkManager& chunkManager;
    WorldGenerator& worldGenerator;
    MeshBuilder& meshBuilder;
    Renderer& renderer;
    ThreadPool& threadPool;
    std::mutex& meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes;

    bool menuWorldInitialized = false;
    Camera menuCamera{glm::vec3(0.0f, 100.0f, 0.0f)};
    float menuCameraYaw = 0.0f;
    float menuCameraPitch = -15.0f;
    float menuCameraTimer = 0.0f;
    int menuSceneIndex = 0;
    static constexpr int NUM_MENU_SCENES = 4;
    glm::vec3 menuScenePositions[NUM_MENU_SCENES] = {
        glm::vec3(0.0f, 100.0f, 0.0f),
        glm::vec3(200.0f, 85.0f, 200.0f),
        glm::vec3(-150.0f, 110.0f, 100.0f),
        glm::vec3(100.0f, 90.0f, -200.0f)
    };

    long menuWorldSeed = 0;
    bool menuSeedInitialized = false;
    std::unordered_map<ChunkPos, std::array<Block, CHUNK_VOLUME>> menuChunkCache;
    bool menuCacheValid = false;
};
