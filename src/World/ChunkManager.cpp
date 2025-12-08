#include "ChunkManager.h"
#include <cmath>
#include <algorithm>

ChunkManager::ChunkManager() {
}

void ChunkManager::update(const glm::vec3& cameraPos) {
    // Unload distant chunks
    unloadDistantChunks(cameraPos);
}

std::shared_ptr<Chunk> ChunkManager::getChunk(const ChunkPos& pos) {
    auto it = chunks.find(pos);
    if (it != chunks.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Chunk> ChunkManager::getChunkAt(const glm::vec3& worldPos) {
    return getChunk(worldToChunk(worldPos));
}

void ChunkManager::unloadDistantChunks(const glm::vec3& cameraPos) {
    ChunkPos centerChunk = worldToChunk(cameraPos);
    
    std::vector<ChunkPos> toRemove;
    for (const auto& [pos, chunk] : chunks) {
        if (!isChunkInRange(pos, centerChunk, RENDER_DISTANCE + 2)) {
            toRemove.push_back(pos);
        }
    }
    
    for (const auto& pos : toRemove) {
        chunks.erase(pos);
    }
}

void ChunkManager::requestChunkGeneration(const ChunkPos& pos) {
    if (chunks.find(pos) == chunks.end()) {
        auto chunk = std::make_shared<Chunk>(pos);
        chunks[pos] = chunk;
    }
}

std::vector<ChunkPos> ChunkManager::getChunksToGenerate(const glm::vec3& cameraPos, int maxChunks) {
    std::vector<ChunkPos> result;
    ChunkPos centerChunk = worldToChunk(cameraPos);
    
    // Generate in a spiral pattern around the player
    for (int dist = 0; dist <= RENDER_DISTANCE && result.size() < static_cast<size_t>(maxChunks); ++dist) {
        for (int x = -dist; x <= dist && result.size() < static_cast<size_t>(maxChunks); ++x) {
            for (int z = -dist; z <= dist && result.size() < static_cast<size_t>(maxChunks); ++z) {
                for (int y = -2; y <= 2 && result.size() < static_cast<size_t>(maxChunks); ++y) {
                    if (std::abs(x) != dist && std::abs(z) != dist) continue;
                    
                    ChunkPos pos = centerChunk + ChunkPos(x, y, z);
                    auto chunk = getChunk(pos);
                    
                    if (!chunk || chunk->getState() == ChunkState::UNLOADED) {
                        result.push_back(pos);
                    }
                }
            }
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<Chunk>> ChunkManager::getChunksToMesh(int maxChunks) {
    std::vector<std::shared_ptr<Chunk>> result;
    
    for (const auto& [pos, chunk] : chunks) {
        if (chunk->getState() == ChunkState::MESH_BUILD && result.size() < static_cast<size_t>(maxChunks)) {
            result.push_back(chunk);
        }
    }
    
    return result;
}

ChunkPos ChunkManager::worldToChunk(const glm::vec3& worldPos) {
    return ChunkPos(
        static_cast<int>(std::floor(worldPos.x / CHUNK_SIZE)),
        static_cast<int>(std::floor(worldPos.y / CHUNK_HEIGHT)),
        static_cast<int>(std::floor(worldPos.z / CHUNK_SIZE))
    );
}

glm::vec3 ChunkManager::chunkToWorld(const ChunkPos& chunkPos) {
    return glm::vec3(
        chunkPos.x * CHUNK_SIZE,
        chunkPos.y * CHUNK_HEIGHT,
        chunkPos.z * CHUNK_SIZE
    );
}

bool ChunkManager::isChunkInRange(const ChunkPos& chunkPos, const ChunkPos& centerChunk, int range) const {
    int dx = chunkPos.x - centerChunk.x;
    int dz = chunkPos.z - centerChunk.z;
    return (dx * dx + dz * dz) <= (range * range);
}
