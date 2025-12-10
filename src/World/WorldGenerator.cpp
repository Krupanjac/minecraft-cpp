#include "WorldGenerator.h"
#include "../Util/Config.h"
#include "ChunkManager.h"
#include <cmath>
#include <random>
#include <algorithm>

WorldGenerator::WorldGenerator(unsigned int seed) : seed(seed) {
}

BiomeInfo WorldGenerator::getBiomeInfo(BiomeType biome) const {
    BiomeInfo info;
    info.type = biome;
    
    switch (biome) {
        case BiomeType::OCEAN:
            info.temperature = 0.5f;
            info.humidity = 1.0f;
            info.heightVariation = 0.3f;
            info.surfaceBlock = BlockType::SAND;
            info.subsurfaceBlock = BlockType::SAND;
            info.surfaceDepth = 3;
            break;
            
        case BiomeType::PLAINS:
            info.temperature = 0.6f;
            info.humidity = 0.5f;
            info.heightVariation = 0.5f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            break;
            
        case BiomeType::DESERT:
            info.temperature = 0.9f;
            info.humidity = 0.1f;
            info.heightVariation = 0.4f;
            info.surfaceBlock = BlockType::SAND;
            info.subsurfaceBlock = BlockType::SANDSTONE;
            info.surfaceDepth = 5;
            break;
            
        case BiomeType::FOREST:
            info.temperature = 0.5f;
            info.humidity = 0.8f;
            info.heightVariation = 0.6f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            break;
            
        case BiomeType::MOUNTAINS:
            info.temperature = 0.3f;
            info.humidity = 0.4f;
            info.heightVariation = 1.5f;
            info.surfaceBlock = BlockType::STONE;
            info.subsurfaceBlock = BlockType::STONE;
            info.surfaceDepth = 1;
            break;
            
        case BiomeType::SNOWY_TUNDRA:
            info.temperature = 0.0f;
            info.humidity = 0.3f;
            info.heightVariation = 0.4f;
            info.surfaceBlock = BlockType::SNOW;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 3;
            break;
    }
    
    return info;
}

float WorldGenerator::getTemperature(float x, float z) const {
    float tempNoise = noise2D(x * 0.003f + 1000.0f, z * 0.003f + 1000.0f);
    return (tempNoise + 1.0f) * 0.5f; // Map to [0, 1]
}

float WorldGenerator::getHumidity(float x, float z) const {
    float humidNoise = noise2D(x * 0.003f + 2000.0f, z * 0.003f + 2000.0f);
    return (humidNoise + 1.0f) * 0.5f; // Map to [0, 1]
}

BiomeType WorldGenerator::getBiome(float x, float z) const {
    float temp = getTemperature(x, z);
    float humid = getHumidity(x, z);
    
    // Simple biome selection based on temperature and humidity
    // Cold biomes
    if (temp < 0.25f) {
        return BiomeType::SNOWY_TUNDRA;
    }
    
    // Hot biomes
    if (temp > 0.75f) {
        if (humid < 0.3f) {
            return BiomeType::DESERT;
        }
    }
    
    // Medium temperature biomes
    if (humid > 0.6f) {
        return BiomeType::FOREST;
    }
    
    if (humid < 0.3f && temp > 0.5f) {
        return BiomeType::DESERT;
    }
    
    // Default to plains
    return BiomeType::PLAINS;
}

bool WorldGenerator::isCave(float x, float y, float z) const {
    // Don't generate caves too close to surface or too deep
    if (y > SEA_LEVEL + 10 || y < 5) {
        return false;
    }
    
    // Use 3D noise for cave generation
    float caveNoise1 = noise3D(x * 0.02f, y * 0.02f, z * 0.02f);
    float caveNoise2 = noise3D(x * 0.02f + 100.0f, y * 0.02f + 100.0f, z * 0.02f + 100.0f);
    
    // Caves exist where both noise values are in a certain range
    // This creates worm-like cave systems
    float threshold = 0.15f;
    return (std::abs(caveNoise1) < threshold && std::abs(caveNoise2) < threshold);
}

void WorldGenerator::generate(std::shared_ptr<Chunk> chunk) {
    const ChunkPos& chunkPos = chunk->getPosition();
    glm::vec3 worldPos = ChunkManager::chunkToWorld(chunkPos);
    
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            float worldX = worldPos.x + x;
            float worldZ = worldPos.z + z;
            
            // Get biome for this column
            BiomeType biome = getBiome(worldX, worldZ);
            BiomeInfo biomeInfo = getBiomeInfo(biome);
            
            // Calculate height based on biome
            float baseHeight = getHeight(worldX, worldZ);
            
            // Apply biome-specific height variation
            float biomeHeightMod = (biomeInfo.heightVariation - 0.5f) * 20.0f;
            int height = static_cast<int>(baseHeight + biomeHeightMod);
            
            // Special case for ocean biome - keep it low
            if (biome == BiomeType::OCEAN) {
                height = std::min(height, SEA_LEVEL - 5);
            }
            
            // Special case for mountains - make them tall
            if (biome == BiomeType::MOUNTAINS) {
                float mountainNoise = noise2D(worldX * 0.005f, worldZ * 0.005f);
                height += static_cast<int>(mountainNoise * 30.0f);
            }
            
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                int worldY = static_cast<int>(worldPos.y) + y;
                
                BlockType blockType = BlockType::AIR;
                
                // Check if this position should be a cave
                bool isInCave = isCave(worldX, static_cast<float>(worldY), worldZ);
                
                if (!isInCave) {
                    if (worldY < height - biomeInfo.surfaceDepth) {
                        blockType = BlockType::STONE;
                    } else if (worldY < height - 1) {
                        blockType = biomeInfo.subsurfaceBlock;
                    } else if (worldY < height) {
                        blockType = biomeInfo.surfaceBlock;
                        
                        // Special case: replace snow with ice if underwater
                        if (blockType == BlockType::SNOW && worldY < SEA_LEVEL) {
                            blockType = BlockType::ICE;
                        }
                    } else if (worldY < SEA_LEVEL) {
                        // Water or ice for frozen biomes
                        if (biome == BiomeType::SNOWY_TUNDRA && worldY == SEA_LEVEL - 1) {
                            blockType = BlockType::ICE;
                        } else {
                            blockType = BlockType::WATER;
                        }
                    }
                } else {
                    // Cave - but still fill with water if below sea level
                    if (worldY < SEA_LEVEL) {
                        blockType = BlockType::WATER;
                    }
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
