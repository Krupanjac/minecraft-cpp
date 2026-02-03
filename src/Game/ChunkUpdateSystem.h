#pragma once

#include "../Mesh/MeshBuilder.h"

#include <mutex>
#include <vector>

class ChunkManager;
class WorldGenerator;
class Renderer;
class ThreadPool;

class ChunkUpdateSystem {
public:
    ChunkUpdateSystem(ChunkManager& chunkManager,
                      WorldGenerator& worldGenerator,
                      MeshBuilder& meshBuilder,
                      Renderer& renderer,
                      ThreadPool& threadPool,
                      std::mutex& meshMutex,
                      std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes);

    void updateWorldChunks(const glm::vec3& cameraPos,
                           const glm::vec3& cameraFront,
                           const glm::mat4& viewMatrix,
                           int renderDistance,
                           int maxGeneratePerFrame,
                           int maxMeshesPerFrame);

private:
    void uploadPendingMeshes();

    ChunkManager& chunkManager;
    WorldGenerator& worldGenerator;
    MeshBuilder& meshBuilder;
    Renderer& renderer;
    ThreadPool& threadPool;
    std::mutex& meshMutex;
    std::vector<std::pair<ChunkPos, MeshData>>& pendingMeshes;
};
