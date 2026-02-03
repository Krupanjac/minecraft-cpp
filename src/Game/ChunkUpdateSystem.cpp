#include "ChunkUpdateSystem.h"

#include "../Core/ThreadPool.h"
#include "../Render/Renderer.h"
#include "../World/Block.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../Util/Config.h"

ChunkUpdateSystem::ChunkUpdateSystem(ChunkManager& chunkManagerRef,
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

void ChunkUpdateSystem::updateWorldChunks(const glm::vec3& cameraPos,
                                          const glm::vec3& cameraFront,
                                          const glm::mat4& viewMatrix,
                                          int renderDistance,
                                          int maxGeneratePerFrame,
                                          int maxMeshesPerFrame) {
    chunkManager.update(cameraPos, cameraFront, viewMatrix);

    auto chunksToGenerate = chunkManager.getChunksToGenerate(cameraPos, renderDistance, maxGeneratePerFrame);
    for (const auto& pos : chunksToGenerate) {
        chunkManager.requestChunkGeneration(pos);
        auto chunk = chunkManager.getChunk(pos);
        if (chunk && chunk->getState() == ChunkState::UNLOADED) {
            chunk->setState(ChunkState::GENERATING);

            threadPool.enqueue([this, chunk]() {
                if (chunkManager.hasPreloadedData(chunk->getPosition())) {
                    auto blocks = chunkManager.getPreloadedData(chunk->getPosition());
                    std::copy(blocks.begin(), blocks.end(), chunk->getBlocks().begin());
                    chunk->setModified(true);
                } else {
                    worldGenerator.generate(chunk);
                }

                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                        for (int z = 0; z < CHUNK_SIZE; ++z) {
                            if (chunk->getBlock(x, y, z).getType() == BlockType::WATER) {
                                glm::vec3 worldPos = ChunkManager::chunkToWorld(chunk->getPosition());
                                chunkManager.scheduleFluidUpdate(
                                    static_cast<int>(worldPos.x) + x,
                                    static_cast<int>(worldPos.y) + y,
                                    static_cast<int>(worldPos.z) + z
                                );
                            }
                        }
                    }
                }

                chunk->setState(ChunkState::MESH_BUILD);
            });
        }
    }

    auto chunksToMesh = chunkManager.getChunksToMesh(cameraPos, maxMeshesPerFrame);
    for (auto chunk : chunksToMesh) {
        chunk->setState(ChunkState::READY);

        const ChunkPos& pos = chunk->getPosition();
        auto chunkXPos = chunkManager.getChunk(pos + ChunkPos(1, 0, 0));
        auto chunkXNeg = chunkManager.getChunk(pos + ChunkPos(-1, 0, 0));
        auto chunkYPos = chunkManager.getChunk(pos + ChunkPos(0, 1, 0));
        auto chunkYNeg = chunkManager.getChunk(pos + ChunkPos(0, -1, 0));
        auto chunkZPos = chunkManager.getChunk(pos + ChunkPos(0, 0, 1));
        auto chunkZNeg = chunkManager.getChunk(pos + ChunkPos(0, 0, -1));

        int lod = chunk->getCurrentLOD();

        threadPool.enqueue([this, chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg, lod]() {
            auto meshData = meshBuilder.buildChunkMesh(chunk, chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg, lod);

            std::lock_guard<std::mutex> lock(meshMutex);
            pendingMeshes.emplace_back(chunk->getPosition(), std::move(meshData));
        });
    }

    uploadPendingMeshes();
}

void ChunkUpdateSystem::uploadPendingMeshes() {
    std::lock_guard<std::mutex> lock(meshMutex);
    for (auto& [pos, meshData] : pendingMeshes) {
        if (!meshData.isEmpty()) {
            renderer.uploadChunkMesh(pos, meshData.vertices, meshData.indices, meshData.waterVertices, meshData.waterIndices);
            auto chunk = chunkManager.getChunk(pos);
            if (chunk) {
                chunk->setState(ChunkState::GPU_UPLOADED);
            }
        } else {
            auto chunk = chunkManager.getChunk(pos);
            if (chunk) {
                chunk->setState(ChunkState::GPU_UPLOADED);
            }
            renderer.uploadChunkMesh(pos, {}, {}, {}, {});
        }
    }
    pendingMeshes.clear();
}
