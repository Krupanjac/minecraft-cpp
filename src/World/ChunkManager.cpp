#include "ChunkManager.h"
#include "../Core/Settings.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <set>

ChunkManager::ChunkManager() {
}

void ChunkManager::update(const glm::vec3& cameraPos, const glm::vec3& /*viewDir*/, const glm::mat4& /*viewMatrix*/) {
    // Unload distant chunks
    unloadDistantChunks(cameraPos);
    updateFluids();
}

int ChunkManager::getHeightAt(int x, int z) {
    // Scan from top down
    // Assumes reasonable bounds based on generation
    for (int y = 256; y >= -64; --y) {
        if (getBlockAt(x, y, z).getType() != BlockType::AIR) return y;
    }
    return 0;
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
                // Generate vertical column from bedrock (-4) to height limit (12)
                for (int y = -4; y <= 12 && result.size() < static_cast<size_t>(maxChunks); ++y) {
                    if (std::abs(x) != dist && std::abs(z) != dist) continue;
                    
                    ChunkPos pos(centerChunk.x + x, y, centerChunk.z + z);
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

std::vector<std::shared_ptr<Chunk>> ChunkManager::getChunksToMesh(const glm::vec3& cameraPos, int maxChunks) {
    std::vector<std::shared_ptr<Chunk>> candidates;
    ChunkPos centerChunk = worldToChunk(cameraPos);
    
    // 1. Collect all chunks that need meshing
    for (const auto& [pos, chunk] : chunks) {
        int desiredLOD = getDesiredLOD(pos, cameraPos);
        
        // Force update if LOD is wrong, even if state is "stable" (READY/GPU_UPLOADED)
        bool lodChanged = (chunk->getCurrentLOD() != desiredLOD);

        if (lodChanged) {
             // If LOD changed, we MUST rebuild, regardless of current state (unless already generating)
             if (chunk->getState() != ChunkState::GENERATING && chunk->getState() != ChunkState::UNLOADED) {
                 chunk->setCurrentLOD(desiredLOD);
                 chunk->setState(ChunkState::MESH_BUILD);
             }
        }

        if (chunk->getState() == ChunkState::MESH_BUILD) {
            candidates.push_back(chunk);
        }
    }

    // 2. Sort by distance to player (closest first)
    std::sort(candidates.begin(), candidates.end(), [centerChunk](const std::shared_ptr<Chunk>& a, const std::shared_ptr<Chunk>& b) {
        ChunkPos posA = a->getPosition();
        ChunkPos posB = b->getPosition();
        
        int distA = (posA.x - centerChunk.x) * (posA.x - centerChunk.x) + 
                    (posA.z - centerChunk.z) * (posA.z - centerChunk.z);
        int distB = (posB.x - centerChunk.x) * (posB.x - centerChunk.x) + 
                    (posB.z - centerChunk.z) * (posB.z - centerChunk.z);
                    
        return distA < distB;
    });
    
    // 3. Return top maxChunks
    if (candidates.size() > static_cast<size_t>(maxChunks)) {
        candidates.resize(maxChunks);
    }
    
    return candidates;
}

int ChunkManager::getDesiredLOD(const ChunkPos& chunkPos, const glm::vec3& cameraPos) const {
    ChunkPos centerChunk = worldToChunk(cameraPos);
    int dx = std::abs(chunkPos.x - centerChunk.x);
    int dz = std::abs(chunkPos.z - centerChunk.z);
    int dist = std::max(dx, dz);
    
    if (dist < 16) return 0;
    if (dist < 32) return 1;
    return 2;
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
            Block currentBlock = chunk->getBlock(lx, ly, lz);
            if (currentBlock == block) return; // No change

            chunk->setBlock(lx, ly, lz, block);
            chunk->setDirty(true);
            chunk->setState(ChunkState::MESH_BUILD);

            // Fluid updates
            if (block.getType() == BlockType::WATER) {
                scheduleFluidUpdate(x, y, z);
            }
            
            // Wake up neighbors
            int dx[] = {1, -1, 0, 0, 0, 0};
            int dy[] = {0, 0, 1, -1, 0, 0};
            int dz[] = {0, 0, 0, 0, 1, -1};
            
            for(int i=0; i<6; ++i) {
                Block n = getBlockAt(x+dx[i], y+dy[i], z+dz[i]);
                if (n.getType() == BlockType::WATER) {
                    scheduleFluidUpdate(x+dx[i], y+dy[i], z+dz[i]);
                }
            }
            
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

void ChunkManager::preloadChunkData(const ChunkPos& pos, const std::vector<Block>& blocks) {
    preloadedChunks[pos] = blocks;
}

bool ChunkManager::hasPreloadedData(const ChunkPos& pos) const {
    return preloadedChunks.find(pos) != preloadedChunks.end();
}

std::vector<Block> ChunkManager::consumePreloadedData(const ChunkPos& pos) {
    auto it = preloadedChunks.find(pos);
    if (it != preloadedChunks.end()) {
        std::vector<Block> blocks = std::move(it->second);
        preloadedChunks.erase(it);
        return blocks;
    }
    return {};
}

std::vector<Block> ChunkManager::getPreloadedData(const ChunkPos& pos) {
    return consumePreloadedData(pos);
}

void ChunkManager::scheduleFluidUpdate(int x, int y, int z) {
    std::lock_guard<std::mutex> lock(fluidMutex);
    glm::ivec3 pos(x, y, z);
    if (pendingFluidUpdates.find(pos) == pendingFluidUpdates.end()) {
        fluidQueue.push_back(pos);
        pendingFluidUpdates.insert(pos);
    }
}

void ChunkManager::updateFluids() {
    std::vector<glm::ivec3> currentQueue;
    {
        std::lock_guard<std::mutex> lock(fluidMutex);
        if (fluidQueue.empty()) return;

        // Limit updates per frame to prevent freezing
        // If queue is huge, we process a chunk of it
        size_t maxUpdates = 1000;
        size_t count = std::min(fluidQueue.size(), maxUpdates);
        
        currentQueue.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            glm::ivec3 pos = fluidQueue.front();
            fluidQueue.pop_front();
            currentQueue.push_back(pos);
            pendingFluidUpdates.erase(pos);
        }
    }
    
    // Deduplicate updates for this frame
    // Use a custom comparator for glm::ivec3 since it doesn't have operator<
    struct Vec3Less {
        bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        }
    };
    
    std::set<glm::ivec3, Vec3Less> processed;
    
    for (const auto& pos : currentQueue) {
        if (processed.count(pos)) continue;
        processed.insert(pos);

        int x = pos.x;
        int y = pos.y;
        int z = pos.z;
        
        Block block = getBlockAt(x, y, z);
        if (block.getType() != BlockType::WATER) continue;
        
        u8 level = block.getData();
        
        // Logic:
        // 0. Infinite Water Source Logic
        // If block is Air or Flowing Water, and has 2+ Source Water neighbors, become Source Water
        if (block.getType() == BlockType::AIR || (block.getType() == BlockType::WATER && level > 0)) {
            int sourceNeighbors = 0;
            int dx[] = {1, -1, 0, 0};
            int dz[] = {0, 0, 1, -1};
            
            for (int j = 0; j < 4; ++j) {
                Block neighbor = getBlockAt(x + dx[j], y, z + dz[j]);
                if (neighbor.getType() == BlockType::WATER && neighbor.getData() == 0) {
                    sourceNeighbors++;
                }
            }
            
            if (sourceNeighbors >= 2) {
                // Become source block
                // Only if there is a solid block underneath (or water)
                Block down = getBlockAt(x, y - 1, z);
                if (down.isSolid() || down.getType() == BlockType::WATER) {
                    setBlockAt(x, y, z, Block(BlockType::WATER, 0));
                    continue; // Done with this block
                }
            }
        }

        if (block.getType() != BlockType::WATER) continue;
        
        // 1. Try to flow down
        Block down = getBlockAt(x, y - 1, z);
        if (down.getType() == BlockType::AIR || (down.getType() == BlockType::WATER && down.getData() != 0)) {
            // Flow down (reset level to 1 for falling water)
            setBlockAt(x, y - 1, z, Block(BlockType::WATER, 1));
        } else if (down.isSolid() || (down.getType() == BlockType::WATER && down.getData() == 0)) {
            // 2. If blocked below, flow sideways
            // Only if current level < 7
            if (level < 7) {
                u8 nextLevel = level + 1;
                if (level == 0) nextLevel = 1; // Source flows to level 1
                
                // Check 4 neighbors
                int dx[] = {1, -1, 0, 0};
                int dz[] = {0, 0, 1, -1};
                
                for (int j = 0; j < 4; ++j) {
                    Block neighbor = getBlockAt(x + dx[j], y, z + dz[j]);
                    if (neighbor.getType() == BlockType::AIR || (neighbor.getType() == BlockType::WATER && neighbor.getData() > nextLevel)) {
                        // Flow into air OR into water that is "lower" (higher data value)
                        setBlockAt(x + dx[j], y, z + dz[j], Block(BlockType::WATER, nextLevel));
                    }
                }
            }
        }
    }
}
