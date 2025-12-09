#include "ChunkManager.h"
#include "../Core/Settings.h"
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
    int renderDist = Settings::instance().renderDistance;
    
    std::vector<ChunkPos> toRemove;
    for (const auto& [pos, chunk] : chunks) {
        if (!isChunkInRange(pos, centerChunk, renderDist + 2)) {
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

std::vector<ChunkPos> ChunkManager::getChunksToGenerate(const glm::vec3& cameraPos, int range, int maxChunks) {
    std::vector<ChunkPos> result;
    ChunkPos centerChunk = worldToChunk(cameraPos);
    
    // Generate in a spiral pattern around the player
    for (int dist = 0; dist <= range && result.size() < static_cast<size_t>(maxChunks); ++dist) {
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

ChunkManager::RayCastResult ChunkManager::rayCast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) {
    RayCastResult result;
    result.hit = false;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    int stepX = (direction.x > 0) ? 1 : -1;
    int stepY = (direction.y > 0) ? 1 : -1;
    int stepZ = (direction.z > 0) ? 1 : -1;

    float tDeltaX = (direction.x != 0) ? std::abs(1.0f / direction.x) : std::numeric_limits<float>::infinity();
    float tDeltaY = (direction.y != 0) ? std::abs(1.0f / direction.y) : std::numeric_limits<float>::infinity();
    float tDeltaZ = (direction.z != 0) ? std::abs(1.0f / direction.z) : std::numeric_limits<float>::infinity();

    float tMaxX = (direction.x != 0) ? ((direction.x > 0) ? (std::floor(origin.x) + 1 - origin.x) * tDeltaX : (origin.x - std::floor(origin.x)) * tDeltaX) : std::numeric_limits<float>::infinity();
    float tMaxY = (direction.y != 0) ? ((direction.y > 0) ? (std::floor(origin.y) + 1 - origin.y) * tDeltaY : (origin.y - std::floor(origin.y)) * tDeltaY) : std::numeric_limits<float>::infinity();
    float tMaxZ = (direction.z != 0) ? ((direction.z > 0) ? (std::floor(origin.z) + 1 - origin.z) * tDeltaZ : (origin.z - std::floor(origin.z)) * tDeltaZ) : std::numeric_limits<float>::infinity();

    float t = 0.0f;
    glm::ivec3 normal(0);

    while (t <= maxDistance) {
        Block block = getBlockAt(x, y, z);
        if (block.getType() != BlockType::AIR && block.getType() != BlockType::WATER) {
            result.hit = true;
            result.distance = t;
            result.normal = normal;
            
            glm::vec3 worldPos(x + 0.5f, y + 0.5f, z + 0.5f);
            result.chunkPos = worldToChunk(worldPos);
            
            glm::vec3 chunkOrigin = chunkToWorld(result.chunkPos);
            result.blockPos = glm::ivec3(x, y, z) - glm::ivec3(chunkOrigin);
            
            return result;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                normal = glm::ivec3(-stepX, 0, 0);
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                normal = glm::ivec3(0, 0, -stepZ);
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                normal = glm::ivec3(0, -stepY, 0);
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                normal = glm::ivec3(0, 0, -stepZ);
            }
        }
    }

    return result;
}

Block ChunkManager::getBlockAt(int x, int y, int z) {
    glm::vec3 worldPos(x + 0.5f, y + 0.5f, z + 0.5f);
    ChunkPos chunkPos = worldToChunk(worldPos);
    auto chunk = getChunk(chunkPos);
    if (chunk) {
        glm::vec3 chunkOrigin = chunkToWorld(chunkPos);
        int lx = x - static_cast<int>(chunkOrigin.x);
        int ly = y - static_cast<int>(chunkOrigin.y);
        int lz = z - static_cast<int>(chunkOrigin.z);
        
        if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_HEIGHT && lz >= 0 && lz < CHUNK_SIZE) {
            return chunk->getBlock(lx, ly, lz);
        }
    }
    return Block(BlockType::AIR);
}

void ChunkManager::setBlockAt(int x, int y, int z, Block block) {
    glm::vec3 worldPos(x + 0.5f, y + 0.5f, z + 0.5f);
    ChunkPos chunkPos = worldToChunk(worldPos);
    auto chunk = getChunk(chunkPos);
    if (chunk) {
        glm::vec3 chunkOrigin = chunkToWorld(chunkPos);
        int lx = x - static_cast<int>(chunkOrigin.x);
        int ly = y - static_cast<int>(chunkOrigin.y);
        int lz = z - static_cast<int>(chunkOrigin.z);
        
        if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_HEIGHT && lz >= 0 && lz < CHUNK_SIZE) {
            chunk->setBlock(lx, ly, lz, block);
            chunk->setDirty(true);
            chunk->setState(ChunkState::MESH_BUILD);
            
            // Update neighbors if on boundary
            if (lx == 0) {
                auto neighbor = getChunk(chunkPos + ChunkPos(-1, 0, 0));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            } else if (lx == CHUNK_SIZE - 1) {
                auto neighbor = getChunk(chunkPos + ChunkPos(1, 0, 0));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            }
            
            if (ly == 0) {
                auto neighbor = getChunk(chunkPos + ChunkPos(0, -1, 0));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            } else if (ly == CHUNK_HEIGHT - 1) {
                auto neighbor = getChunk(chunkPos + ChunkPos(0, 1, 0));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            }
            
            if (lz == 0) {
                auto neighbor = getChunk(chunkPos + ChunkPos(0, 0, -1));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            } else if (lz == CHUNK_SIZE - 1) {
                auto neighbor = getChunk(chunkPos + ChunkPos(0, 0, 1));
                if (neighbor) neighbor->setState(ChunkState::MESH_BUILD);
            }
        }
    }
}

std::vector<std::shared_ptr<Chunk>> ChunkManager::getNeighbors(const ChunkPos& pos) {
    std::vector<std::shared_ptr<Chunk>> neighbors(6);
    neighbors[0] = getChunk(pos + ChunkPos(1, 0, 0));  // X+
    neighbors[1] = getChunk(pos + ChunkPos(-1, 0, 0)); // X-
    neighbors[2] = getChunk(pos + ChunkPos(0, 1, 0));  // Y+
    neighbors[3] = getChunk(pos + ChunkPos(0, -1, 0)); // Y-
    neighbors[4] = getChunk(pos + ChunkPos(0, 0, 1));  // Z+
    neighbors[5] = getChunk(pos + ChunkPos(0, 0, -1)); // Z-
    return neighbors;
}
