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

int WorldGenerator::getSurfaceHeight(int x, int z) const {
    BiomeType biome = getBiome(static_cast<float>(x), static_cast<float>(z));
    BiomeInfo biomeInfo = getBiomeInfo(biome);
    
    float baseHeight = getHeight(static_cast<float>(x), static_cast<float>(z));
    float biomeHeightMod = (biomeInfo.heightVariation - 0.5f) * 20.0f;
    int height = static_cast<int>(baseHeight + biomeHeightMod);
    
    if (biome == BiomeType::OCEAN) {
        height = std::min(height, SEA_LEVEL - 5);
    }
    if (biome == BiomeType::MOUNTAINS) {
        float mountainNoise = noise2D(x * 0.005f, z * 0.005f);
        height += static_cast<int>(mountainNoise * 30.0f);
    }
    return height;
}

bool WorldGenerator::hasTree(int x, int z, BiomeType biome) const {
    // 1. Check if this position is a candidate based on probability
    unsigned int seedX = static_cast<unsigned int>(x);
    unsigned int seedZ = static_cast<unsigned int>(z);
    
    unsigned int h = seed + seedX * 374761393 + seedZ * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    float r = (h & 0xFFFF) / 65536.0f;
    
    float treeProb = 0.0f;
    if (biome == BiomeType::FOREST) treeProb = 0.02f; 
    else if (biome == BiomeType::PLAINS) treeProb = 0.001f;
    else if (biome == BiomeType::MOUNTAINS) treeProb = 0.005f;
    // No trees in DESERT, OCEAN, SNOWY_TUNDRA (unless we add spruce later)
    
    if (r >= treeProb) return false;

    // 2. Spatial check: Suppress this tree if a "better" candidate is nearby
    // This creates a Poisson-disk-like distribution
    int radius = 3; // Minimum distance between trees
    
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            if (dx == 0 && dz == 0) continue;
            
            int nx = x + dx;
            int nz = z + dz;
            
            unsigned int nSeedX = static_cast<unsigned int>(nx);
            unsigned int nSeedZ = static_cast<unsigned int>(nz);
            unsigned int nh = seed + nSeedX * 374761393 + nSeedZ * 668265263;
            nh = (nh ^ (nh >> 13)) * 1274126177;
            float nr = (nh & 0xFFFF) / 65536.0f;
            
            // If neighbor is also a candidate
            if (nr < treeProb) {
                // If neighbor has a lower random value (or equal with coordinate tie-breaker), they win
                if (nr < r || (nr == r && (nx < x || (nx == x && nz < z)))) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

int WorldGenerator::getTreeHeight(int x, int z) const {
    unsigned int h = seed + x * 123 + z * 456;
    h = (h ^ (h >> 13)) * 1274126177;
    // Height variation: 4 to 8
    return 4 + (h % 5); 
}

void WorldGenerator::generate(std::shared_ptr<Chunk> chunk) {
    const ChunkPos& chunkPos = chunk->getPosition();
    glm::vec3 worldPos = ChunkManager::chunkToWorld(chunkPos);
    
    // 1. Terrain Pass
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = static_cast<int>(worldPos.x) + x;
            int worldZ = static_cast<int>(worldPos.z) + z;
            
            BiomeType biome = getBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
            BiomeInfo biomeInfo = getBiomeInfo(biome);
            
            int height = getSurfaceHeight(worldX, worldZ);
            
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                int worldY = static_cast<int>(worldPos.y) + y;
                BlockType blockType = BlockType::AIR;
                
                bool isInCave = isCave(static_cast<float>(worldX), static_cast<float>(worldY), static_cast<float>(worldZ));
                
                if (!isInCave) {
                    if (worldY < height - biomeInfo.surfaceDepth) blockType = BlockType::STONE;
                    else if (worldY < height - 1) blockType = biomeInfo.subsurfaceBlock;
                    else if (worldY < height) {
                        blockType = biomeInfo.surfaceBlock;
                        if (blockType == BlockType::SNOW && worldY < SEA_LEVEL) blockType = BlockType::ICE;
                    } else if (worldY < SEA_LEVEL) {
                        if (biome == BiomeType::SNOWY_TUNDRA && worldY == SEA_LEVEL - 1) blockType = BlockType::ICE;
                        else blockType = BlockType::WATER;
                    }
                } else {
                    if (worldY < SEA_LEVEL) blockType = BlockType::WATER;
                }
                
                chunk->setBlock(x, y, z, Block(blockType));
            }
            
            // 2. Vegetation Pass (Plants)
            // Only place plants if this column is the surface
            int chunkBaseY = static_cast<int>(worldPos.y);
            if (height >= chunkBaseY && height < chunkBaseY + CHUNK_HEIGHT) {
                // Ensure we are not underwater
                if (height < SEA_LEVEL) continue;

                int localY = height - chunkBaseY;
                // Check if block below is valid for plants (Grass)
                Block below = chunk->getBlock(x, localY - 1, z);
                if (below.getType() == BlockType::GRASS) {
                    // Random plant placement
                    unsigned int h = seed + worldX * 374761393 + worldZ * 668265263;
                    h = (h ^ (h >> 13)) * 1274126177;
                    float r = (h & 0xFFFF) / 65536.0f;
                    
                    float plantProb = 0.0f;
                    if (biome == BiomeType::PLAINS) plantProb = 0.2f;
                    else if (biome == BiomeType::FOREST) plantProb = 0.1f;
                    else if (biome == BiomeType::MOUNTAINS) plantProb = 0.05f;
                    
                    if (r < plantProb) {
                        BlockType plant = BlockType::TALL_GRASS;
                        if (((h >> 16) & 0xFF) < 25) plant = BlockType::ROSE; // ~10% chance
                        chunk->setBlock(x, localY, z, Block(plant));
                    }
                }
            }
        }
    }
    
    // 3. Tree Pass (Neighborhood Search)
    // Check a padded area around the chunk to allow trees from neighbors to spill over
    int pad = 2; // Tree radius
    for (int nx = -pad; nx < CHUNK_SIZE + pad; ++nx) {
        for (int nz = -pad; nz < CHUNK_SIZE + pad; ++nz) {
            int worldX = static_cast<int>(worldPos.x) + nx;
            int worldZ = static_cast<int>(worldPos.z) + nz;
            
            BiomeType biome = getBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
            
            if (hasTree(worldX, worldZ, biome)) {
                int treeBaseY = getSurfaceHeight(worldX, worldZ);
                
                // VALIDITY CHECKS:
                // 1. Water check: Trees can't grow in water
                if (treeBaseY < SEA_LEVEL) continue;
                
                // 2. Cave check: Trees can't float over cave entrances
                // Check the block immediately below the tree (treeBaseY - 1)
                if (isCave(static_cast<float>(worldX), static_cast<float>(treeBaseY - 1), static_cast<float>(worldZ))) continue;

                int treeH = getTreeHeight(worldX, worldZ);
                
                // Check if tree affects this chunk vertically
                int chunkBaseY = static_cast<int>(worldPos.y);
                int treeTopY = treeBaseY + treeH + 1; // +1 for leaves
                
                if (treeTopY < chunkBaseY || treeBaseY > chunkBaseY + CHUNK_HEIGHT) continue;
                
                // Draw Trunk
                if (nx >= 0 && nx < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                    for (int i = 0; i < treeH; ++i) {
                        int wy = treeBaseY + i;
                        if (wy >= chunkBaseY && wy < chunkBaseY + CHUNK_HEIGHT) {
                            chunk->setBlock(nx, wy - chunkBaseY, nz, Block(BlockType::LOG));
                        }
                    }
                }
                
                // Draw Leaves
                // Standard Minecraft Oak shape with variation
                // Use hash for leaf variation
                unsigned int h = seed + worldX * 34123 + worldZ * 23123;
                h = (h ^ (h >> 13)) * 1274126177;
                bool extraLeaves = (h % 2) == 0; // 50% chance for slightly fuller leaves

                for (int ly = treeBaseY + treeH - 3; ly <= treeBaseY + treeH; ++ly) {
                    if (ly < chunkBaseY || ly >= chunkBaseY + CHUNK_HEIGHT) continue;
                    
                    int dy = ly - (treeBaseY + treeH); // 0 at top, -1, -2, -3
                    int radius = (dy >= -1) ? 1 : 2;   // Top 2 layers radius 1, bottom 2 radius 2
                    
                    // Iterate leaf box relative to tree
                    for (int lx = worldX - radius; lx <= worldX + radius; ++lx) {
                        for (int lz = worldZ - radius; lz <= worldZ + radius; ++lz) {
                            // Check if this leaf block is inside OUR chunk
                            int localX = lx - static_cast<int>(worldPos.x);
                            int localZ = lz - static_cast<int>(worldPos.z);
                            
                            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE) {
                                // Corner check
                                bool isCorner = std::abs(lx - worldX) == radius && std::abs(lz - worldZ) == radius;
                                
                                if (isCorner) {
                                    // Top layers (radius 1): always skip corners (cross shape)
                                    if (radius == 1) continue;
                                    
                                    // Bottom layers (radius 2): skip corners unless "extraLeaves" is true and it's the 3rd layer down
                                    if (radius == 2) {
                                        // Standard oak: skip corners on radius 2 layers usually
                                        // Let's make it random per tree or per layer
                                        if (!extraLeaves || (h % 3 != 0)) continue; 
                                    }
                                }

                                // Don't overwrite trunk
                                if (lx == worldX && lz == worldZ) continue;
                                
                                // Don't overwrite solid blocks (terrain)
                                Block existing = chunk->getBlock(localX, ly - chunkBaseY, localZ);
                                if (existing.getType() == BlockType::AIR || existing.isCrossModel()) {
                                    chunk->setBlock(localX, ly - chunkBaseY, localZ, Block(BlockType::LEAVES));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    chunk->setState(ChunkState::MESH_BUILD);
}



float WorldGenerator::getNoise(float x, float y, float z) const {
    return noise3D(x * NOISE_SCALE, y * NOISE_SCALE, z * NOISE_SCALE);
}

float WorldGenerator::getHeight(float x, float z) const {
    // Fractal Brownian Motion (FBM)
    // Adding multiple layers of noise (octaves) with increasing frequency and decreasing amplitude
    int octaves = 8;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float persistence = 0.5f; // How much amplitude decreases per octave
    float lacunarity = 2.0f;  // How much frequency increases per octave
    
    float totalNoise = 0.0f;
    float maxAmplitude = 0.0f;
    
    // Use different offsets for each octave to avoid artifacts
    float offsets[] = {
        0.0f, 123.45f, 678.90f, 321.54f,
        987.65f, 456.78f, 135.79f, 246.80f
    };

    for (int i = 0; i < octaves; ++i) {
        float n = noise2D((x + offsets[i]) * NOISE_SCALE * frequency, (z + offsets[i]) * NOISE_SCALE * frequency);
        totalNoise += n * amplitude;
        maxAmplitude += amplitude;
        
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    // Normalize to [-1, 1]
    // Gradient noise typically has a range smaller than [-1, 1] (approx [-0.7, 0.7])
    // We multiply by 1.5 to stretch the contrast and utilize the full range
    totalNoise = (totalNoise / maxAmplitude) * 1.5f;
    totalNoise = std::clamp(totalNoise, -1.0f, 1.0f);
    
    // Map to [0, 1]
    float normalizedNoise = (totalNoise + 1.0f) * 0.5f;
    
    // Exponential scaling: 2 ^ (noise * power)
    // Increased power to 8.0 for taller mountains
    float power = 8.0f;
    float heightFactor = std::pow(2.0f, normalizedNoise * power);
    
    // Base height calculation
    // At noise=0.5 (average), heightFactor is 2^4 = 16.
    // Base 20 + 16 = 36 (Just above SEA_LEVEL 32)
    // Max height: 20 + 256 = 276 (Very high peaks)
    // Min height: 20 + 1 = 21 (Deep ocean)
    
    return 20.0f + heightFactor;
}

float WorldGenerator::noise3D(float x, float y, float z) const {
    // Improved Perlin Noise (Gradient Noise)
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    int zi = static_cast<int>(std::floor(z)) & 255;
    
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float zf = z - std::floor(z);
    
    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);
    
    // Hash coordinates to get gradients
    // We use the same large primes for hashing to avoid a lookup table
    auto hash = [&](int i, int j, int k) {
        return ((i + seed) * 374761393 + (j + seed) * 668265263 + (k + seed) * 1274126177) & 0xFFFFFF;
    };

    int aaa = hash(xi, yi, zi);
    int aba = hash(xi, yi + 1, zi);
    int aab = hash(xi, yi, zi + 1);
    int abb = hash(xi, yi + 1, zi + 1);
    int baa = hash(xi + 1, yi, zi);
    int bba = hash(xi + 1, yi + 1, zi);
    int bab = hash(xi + 1, yi, zi + 1);
    int bbb = hash(xi + 1, yi + 1, zi + 1);
    
    // Calculate dot products
    float val_aaa = grad(aaa, xf, yf, zf);
    float val_aba = grad(aba, xf, yf - 1, zf);
    float val_aab = grad(aab, xf, yf, zf - 1);
    float val_abb = grad(abb, xf, yf - 1, zf - 1);
    float val_baa = grad(baa, xf - 1, yf, zf);
    float val_bba = grad(bba, xf - 1, yf - 1, zf);
    float val_bab = grad(bab, xf - 1, yf, zf - 1);
    float val_bbb = grad(bbb, xf - 1, yf - 1, zf - 1);
    
    // Trilinear interpolation
    float x1 = lerp(val_aaa, val_baa, u);
    float x2 = lerp(val_aba, val_bba, u);
    float x3 = lerp(val_aab, val_bab, u);
    float x4 = lerp(val_abb, val_bbb, u);
    
    float y1 = lerp(x1, x2, v);
    float y2 = lerp(x3, x4, v);
    
    return lerp(y1, y2, w);
}

float WorldGenerator::grad(int hash, float x, float y, float z) const {
    // Convert low 4 bits of hash code into 12 gradient directions
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
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
