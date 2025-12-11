#pragma once

#include "../Util/Types.h"
#include "../Util/Config.h"
#include "Chunk.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include <mutex>

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
    
    void unloadAll() { chunks.clear(); }
    void unloadDistantChunks(const glm::vec3& cameraPos);
    void requestChunkGeneration(const ChunkPos& pos);
    
    std::vector<ChunkPos> getChunksToGenerate(const glm::vec3& cameraPos, int range, int maxChunks);
    std::vector<std::shared_ptr<Chunk>> getChunksToMesh(const glm::vec3& cameraPos, int maxChunks);

    static ChunkPos worldToChunk(const glm::vec3& worldPos);
    static glm::vec3 chunkToWorld(const ChunkPos& chunkPos);

    int getDesiredLOD(const ChunkPos& chunkPos, const glm::vec3& cameraPos) const;

    struct RayCastResult {
        bool hit = false;
        ChunkPos chunkPos;
        glm::ivec3 blockPos; // Local block pos within chunk
        glm::ivec3 normal;   // Face normal
        float distance = 0.0f;
    };

    RayCastResult rayCast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    
    void setBlockAt(int x, int y, int z, Block block);
    
    // Fluid simulation
    void updateFluids();
    void scheduleFluidUpdate(int x, int y, int z);

    // Helper to get neighbors for meshing
    std::vector<std::shared_ptr<Chunk>> getNeighbors(const ChunkPos& pos);

    void setWorldName(const std::string& name) { currentWorldName = name; }
    void clear() { 
        chunks.clear(); 
        preloadedChunks.clear();
    }
    
    // Modified chunk cache for single-file loading
    void preloadChunkData(const ChunkPos& pos, const std::vector<Block>& blocks);
    bool hasPreloadedData(const ChunkPos& pos) const;
    std::vector<Block> getPreloadedData(const ChunkPos& pos);

private:
    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>> chunks;
    std::unordered_map<ChunkPos, std::vector<Block>> preloadedChunks;
    std::vector<glm::ivec3> fluidQueue;
    std::mutex fluidMutex;
    std::string currentWorldName;
    
    bool isChunkInRange(const ChunkPos& chunkPos, const ChunkPos& centerChunk, int range) const;
};
