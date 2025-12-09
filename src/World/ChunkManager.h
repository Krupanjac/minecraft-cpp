#pragma once

#include "../Util/Types.h"
#include "../Util/Config.h"
#include "Chunk.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager() = default;

    void update(const glm::vec3& cameraPos);
    
    std::shared_ptr<Chunk> getChunk(const ChunkPos& pos);
    std::shared_ptr<Chunk> getChunkAt(const glm::vec3& worldPos);
    Block getBlockAt(int x, int y, int z);
    
    const std::unordered_map<ChunkPos, std::shared_ptr<Chunk>>& getChunks() const {
        return chunks;
    }
    
    void unloadDistantChunks(const glm::vec3& cameraPos);
    void requestChunkGeneration(const ChunkPos& pos);
    
    std::vector<ChunkPos> getChunksToGenerate(const glm::vec3& cameraPos, int range, int maxChunks);
    std::vector<std::shared_ptr<Chunk>> getChunksToMesh(int maxChunks);

    static ChunkPos worldToChunk(const glm::vec3& worldPos);
    static glm::vec3 chunkToWorld(const ChunkPos& chunkPos);

    struct RayCastResult {
        bool hit = false;
        ChunkPos chunkPos;
        glm::ivec3 blockPos; // Local block pos within chunk
        glm::ivec3 normal;   // Face normal
        float distance = 0.0f;
    };

    RayCastResult rayCast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    
    void setBlockAt(int x, int y, int z, Block block);
    
    // Helper to get neighbors for meshing
    std::vector<std::shared_ptr<Chunk>> getNeighbors(const ChunkPos& pos);

private:
    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>> chunks;
    
    bool isChunkInRange(const ChunkPos& chunkPos, const ChunkPos& centerChunk, int range) const;
};
