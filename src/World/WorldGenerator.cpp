#include "WorldGenerator.h"
#include "../Util/Config.h"
#include "ChunkManager.h"
#include <cmath>
#include <random>

WorldGenerator::WorldGenerator() : seed(12345) {
}

void WorldGenerator::generate(std::shared_ptr<Chunk> chunk) {
    const ChunkPos& chunkPos = chunk->getPosition();
    glm::vec3 worldPos = ChunkManager::chunkToWorld(chunkPos);
    
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            float worldX = worldPos.x + x;
            float worldZ = worldPos.z + z;
            
            int height = static_cast<int>(getHeight(worldX, worldZ));
            
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                int worldY = static_cast<int>(worldPos.y) + y;
                
                BlockType blockType = BlockType::AIR;
                
                if (worldY < height - 4) {
                    blockType = BlockType::STONE;
                } else if (worldY < height - 1) {
                    blockType = BlockType::DIRT;
                } else if (worldY < height) {
                    blockType = BlockType::GRASS;
                } else if (worldY < SEA_LEVEL) {
                    blockType = BlockType::WATER;
                }
                
                chunk->setBlock(x, y, z, Block(blockType));
            }
        }
    }
    
    chunk->setState(ChunkState::MESH_BUILD);
}

float WorldGenerator::getNoise(float x, float y, float z) const {
    return noise3D(x * NOISE_SCALE, y * NOISE_SCALE, z * NOISE_SCALE);
}

float WorldGenerator::getHeight(float x, float z) const {
    float noise = noise2D(x * NOISE_SCALE, z * NOISE_SCALE);
    noise = (noise + 1.0f) * 0.5f;  // Map from [-1,1] to [0,1]
    
    float baseHeight = SEA_LEVEL + noise * 32.0f;
    
    // Add some variation
    float detailNoise = noise2D(x * NOISE_SCALE * 4.0f, z * NOISE_SCALE * 4.0f);
    baseHeight += detailNoise * 4.0f;
    
    return baseHeight;
}

float WorldGenerator::noise3D(float x, float y, float z) const {
    // Simple pseudo-random 3D noise
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    int zi = static_cast<int>(std::floor(z)) & 255;
    
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float zf = z - std::floor(z);
    
    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);
    
    // Hash coordinates
    int aaa = ((xi + seed) * 374761393 + (yi + seed) * 668265263 + (zi + seed) * 1274126177) & 0xFFFFFF;
    int aba = ((xi + seed) * 374761393 + ((yi + 1) + seed) * 668265263 + (zi + seed) * 1274126177) & 0xFFFFFF;
    int aab = ((xi + seed) * 374761393 + (yi + seed) * 668265263 + ((zi + 1) + seed) * 1274126177) & 0xFFFFFF;
    int abb = ((xi + seed) * 374761393 + ((yi + 1) + seed) * 668265263 + ((zi + 1) + seed) * 1274126177) & 0xFFFFFF;
    int baa = (((xi + 1) + seed) * 374761393 + (yi + seed) * 668265263 + (zi + seed) * 1274126177) & 0xFFFFFF;
    int bba = (((xi + 1) + seed) * 374761393 + ((yi + 1) + seed) * 668265263 + (zi + seed) * 1274126177) & 0xFFFFFF;
    int bab = (((xi + 1) + seed) * 374761393 + (yi + seed) * 668265263 + ((zi + 1) + seed) * 1274126177) & 0xFFFFFF;
    int bbb = (((xi + 1) + seed) * 374761393 + ((yi + 1) + seed) * 668265263 + ((zi + 1) + seed) * 1274126177) & 0xFFFFFF;
    
    // Convert to [-1, 1]
    float val_aaa = (aaa / 8388607.5f) - 1.0f;
    float val_aba = (aba / 8388607.5f) - 1.0f;
    float val_aab = (aab / 8388607.5f) - 1.0f;
    float val_abb = (abb / 8388607.5f) - 1.0f;
    float val_baa = (baa / 8388607.5f) - 1.0f;
    float val_bba = (bba / 8388607.5f) - 1.0f;
    float val_bab = (bab / 8388607.5f) - 1.0f;
    float val_bbb = (bbb / 8388607.5f) - 1.0f;
    
    // Trilinear interpolation
    float x1 = lerp(val_aaa, val_baa, u);
    float x2 = lerp(val_aba, val_bba, u);
    float x3 = lerp(val_aab, val_bab, u);
    float x4 = lerp(val_abb, val_bbb, u);
    
    float y1 = lerp(x1, x2, v);
    float y2 = lerp(x3, x4, v);
    
    return lerp(y1, y2, w);
}

float WorldGenerator::noise2D(float x, float z) const {
    return noise3D(x, 0, z);
}

float WorldGenerator::lerp(float a, float b, float t) const {
    return a + t * (b - a);
}

float WorldGenerator::fade(float t) const {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
