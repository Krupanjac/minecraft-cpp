#include "WorldGenerator.h"
#include "../Util/Config.h"
#include "ChunkManager.h"
#include <cmath>
#include <random>
#include <algorithm>

WorldGenerator::WorldGenerator(unsigned int seed) : seed(seed) {
    setSeed(seed);
}

void WorldGenerator::setSeed(unsigned int s) {
    seed = s;
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> disOffset(-100000.0f, 100000.0f);
    std::uniform_real_distribution<float> disBias(-0.15f, 0.15f);
    std::uniform_real_distribution<float> disScale(0.8f, 1.2f);

    // Independent offsets for each feature to break correlation
    offsetContinentX = disOffset(gen);
    offsetContinentZ = disOffset(gen);
    
    offsetTempX = disOffset(gen);
    offsetTempZ = disOffset(gen);
    
    offsetHumidX = disOffset(gen);
    offsetHumidZ = disOffset(gen);
    
    offsetErosionX = disOffset(gen);
    offsetErosionZ = disOffset(gen);
    
    offsetPVX = disOffset(gen);
    offsetPVZ = disOffset(gen);

    // Global biases to vary world "feel" (e.g. Ice Age vs Desert World)
    globalTempBias = disBias(gen);
    globalHumidBias = disBias(gen);
    mountainScaleBias = disScale(gen);
    
    std::uniform_real_distribution<float> disCaveDensity(-0.05f, 0.05f);
    std::uniform_real_distribution<float> disCaveWater(-1.0f, 1.0f);
    std::uniform_real_distribution<float> disFreq(0.8f, 1.2f); // +/- 20% scale variation
    
    globalCaveDensityBias = disCaveDensity(gen);
    globalCaveWaterBias = disCaveWater(gen);
    globalFrequencyBias = disFreq(gen);
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
    // Use specific offset for temperature
    // Scale reduced to 0.0005f for much larger biomes (approx 6x larger)
    float tempNoise = noise2D(x * 0.0005f + offsetTempX, z * 0.0005f + offsetTempZ);
    float t = (tempNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(t + globalTempBias, 0.0f, 1.0f);
}

float WorldGenerator::getHumidity(float x, float z) const {
    // Use specific offset for humidity
    // Scale reduced to 0.0005f for much larger biomes
    float humidNoise = noise2D(x * 0.0005f + offsetHumidX, z * 0.0005f + offsetHumidZ);
    float h = (humidNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(h + globalHumidBias, 0.0f, 1.0f);
}

BiomeType WorldGenerator::getBiome(float x, float z) const {
    // Advanced Biome Selection based on Continentalness, Erosion, Temperature, and Humidity
    
    // 1. Re-calculate Terrain Parameters (Must match getHeight logic)
    float contX = x * NOISE_SCALE * 0.04f * globalFrequencyBias + offsetContinentX;
    float contZ = z * NOISE_SCALE * 0.04f * globalFrequencyBias + offsetContinentZ;
    float warpedContX = contX; float warpedContZ = contZ;
    domainWarp(warpedContX, warpedContZ); 
    float continentalness = fbm(warpedContX, warpedContZ, 2);
    
    float eroX = x * NOISE_SCALE * 0.1f * globalFrequencyBias + offsetErosionX;
    float eroZ = z * NOISE_SCALE * 0.1f * globalFrequencyBias + offsetErosionZ;
    float erosion = fbm(eroX, eroZ, 2);
    
    float temp = getTemperature(x, z);
    float humid = getHumidity(x, z);
    
    // 2. Biome Determination Logic
    
    // OCEAN / COAST
    // Match the spline points: < -0.15 is Ocean/Shore
    if (continentalness < -0.15f) return BiomeType::OCEAN;
    
    // MOUNTAINS (High Erosion)
    // Match the spline points: > 0.5 is Mountain Base
    if (erosion > 0.5f) {
        if (temp < 0.4f) return BiomeType::SNOWY_TUNDRA; // Snowy Peaks
        return BiomeType::MOUNTAINS; // Stone Mountains
    }
    
    // HILLS / HIGHLANDS (Mid Erosion)
    // Match the spline points: > 0.2 is Highlands
    if (erosion > 0.2f) {
        if (temp < 0.25f) return BiomeType::SNOWY_TUNDRA;
        if (humid < 0.3f) return BiomeType::DESERT; // Desert Hills
        return BiomeType::FOREST; // Forested Hills
    }
    
    // FLAT LANDS (Low Erosion)
    // Temperature/Humidity driven
    if (temp < 0.2f) return BiomeType::SNOWY_TUNDRA;
    
    if (temp > 0.7f) {
        if (humid < 0.4f) return BiomeType::DESERT;
        return BiomeType::PLAINS; // Savanna-like
    }
    
    if (humid > 0.6f) return BiomeType::FOREST;
    
    return BiomeType::PLAINS;
}

bool WorldGenerator::isCave(float x, float y, float z) const {
    // Don't generate caves too close to surface or too deep
    if (y > SEA_LEVEL + 10 || y < 5) {
        return false;
    }
    
    // 1. Cheese Caves (Large Rooms)
    // Use lower frequency noise for large open areas
    float cheese = noise3D(x * 0.015f, y * 0.015f, z * 0.015f);
    float cheeseThreshold = -0.6f + globalCaveDensityBias; // Only very low values become air
    
    // 2. Spaghetti Caves (Tunnels)
    // Use ridged noise (abs value close to 0)
    // We need two noise values to create 3D worms (intersection of two "sheets")
    // Add large offsets to ensure independence from other noise maps
    float worm1 = noise3D(x * 0.02f + 123.4f, y * 0.02f + 521.2f, z * 0.02f + 921.1f);
    float worm2 = noise3D(x * 0.02f + 921.4f, y * 0.02f + 123.2f, z * 0.02f + 521.1f);
    
    // Vary tunnel width based on depth
    // Deeper caves are slightly wider
    float depthFactor = std::clamp((SEA_LEVEL - y) / 60.0f, 0.0f, 1.0f);
    float tunnelWidth = 0.06f + depthFactor * 0.04f; 
    
    bool isTunnel = (std::abs(worm1) < tunnelWidth && std::abs(worm2) < tunnelWidth);
    bool isRoom = (cheese < cheeseThreshold);
    
    return isTunnel || isRoom;
}

int WorldGenerator::getSurfaceHeight(int x, int z) const {
    // Trust the height map directly as it now incorporates continentalness and roughness
    float baseHeight = getHeight(static_cast<float>(x), static_cast<float>(z));
    return static_cast<int>(baseHeight);
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
                
                // Bedrock Layer at Y = -64
                if (worldY <= -64) {
                    blockType = BlockType::BEDROCK;
                    isInCave = false; // No caves in bedrock
                } else if (!isInCave) {
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
                    // Cave generation
                    // If below sea level, check if this world has flooded caves
                    if (worldY < SEA_LEVEL && globalCaveWaterBias > 0.0f) {
                        blockType = BlockType::WATER;
                    }
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
    // 1. Continentalness (Macro-scale geography)
    // Determines Oceans vs Land.
    // We use Domain Warping here to create more natural, less grid-aligned coastlines.
    // SCALE: 0.0004 (Massive features, ~2500 blocks) - Reduced to fix "too many oceans"
    float contX = x * NOISE_SCALE * 0.04f * globalFrequencyBias + offsetContinentX;
    float contZ = z * NOISE_SCALE * 0.04f * globalFrequencyBias + offsetContinentZ;
    
    // Apply domain warp to coordinates
    // This makes the noise "swirl" slightly
    float warpedContX = contX;
    float warpedContZ = contZ;
    domainWarp(warpedContX, warpedContZ); 
    
    // Use FBM for continentalness to avoid "egg" shapes
    float continentalness = fbm(warpedContX, warpedContZ, 2);
    
    // 2. Erosion (Mid-scale)
    // Determines if area is flat (Plains) or mountainous.
    // Independent from continentalness.
    // SCALE: 0.001 (~1000 blocks) - Reduced to make mountain ranges larger
    float eroX = x * NOISE_SCALE * 0.1f * globalFrequencyBias + offsetErosionX;
    float eroZ = z * NOISE_SCALE * 0.1f * globalFrequencyBias + offsetErosionZ;
    float erosion = fbm(eroX, eroZ, 2);
    
    // 3. Peaks & Valleys (Micro-scale / Detail)
    // Adds local roughness.
    // SCALE: 0.005 (~200 blocks) - Reduced significantly for gradual slopes
    float pvX = x * NOISE_SCALE * 0.5f + offsetPVX; 
    float pvZ = z * NOISE_SCALE * 0.5f + offsetPVZ;
    
    // Use FBM (Fractal Brownian Motion) for detail
    // Mix smooth noise and ridge noise based on erosion
    // If erosion is low (mountains), we want sharp ridges.
    float smoothPV = noise2D(pvX, pvZ);
    smoothPV += 0.25f * noise2D(pvX * 2.0f, pvZ * 2.0f); // Reduced high freq influence
    smoothPV /= 1.25f;
    
    float sharpPV = ridgeNoise(pvX, pvZ);
    sharpPV += 0.25f * ridgeNoise(pvX * 2.0f, pvZ * 2.0f); // Reduced high freq influence
    sharpPV /= 1.25f;
    
    // Blend: If erosion > 0.0 (Flat), use smooth. If erosion < 0.0 (Mountain), blend to sharp.
    float ridgeFactor = std::clamp((0.2f - erosion) * 2.0f, 0.0f, 1.0f); // 0 if erosion > 0.2, 1 if erosion < -0.3
    float pv = lerp(smoothPV, sharpPV, ridgeFactor);
    
    // 4. Combine using Spline logic
    return getSplineHeight(continentalness, erosion, pv);
}

float WorldGenerator::noise3D(float x, float y, float z) const {
    // Improved Perlin Noise (Gradient Noise)
    // Note: We removed the global seedOffsetX/Z here because we now apply specific offsets
    // in the caller functions (getHeight, getTemperature, etc.) to ensure independence.
    // This function now returns pure noise for the given coordinates.
    
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
    // Mix seed into the hash more thoroughly to ensure different seeds produce different patterns
    auto hash = [&](int i, int j, int k) {
        // Use a more robust mixing function (MurmurHash3-like or similar simple mixer)
        unsigned int h = seed;
        h ^= i * 374761393;
        h ^= j * 668265263;
        h ^= k * 1274126177;
        h ^= h >> 13;
        h *= 0x5bd1e995;
        h ^= h >> 15;
        return h & 0xFFFFFF;
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

float WorldGenerator::fbm(float x, float z, int octaves) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;  // Used for normalizing result to 0.0 - 1.0
    
    for(int i=0; i<octaves; i++) {
        total += noise2D(x * frequency, z * frequency) * amplitude;
        
        maxValue += amplitude;
        
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return total / maxValue;
}

float WorldGenerator::lerp(float a, float b, float t) const {
    return a + t * (b - a);
}

float WorldGenerator::fade(float t) const {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float WorldGenerator::ridgeNoise(float x, float z) const {
    // Ridged multifractal noise
    // 1.0 - abs(noise) creates sharp peaks
    float n = noise2D(x, z);
    return 1.0f - std::abs(n);
}

float WorldGenerator::domainWarp(float& x, float& z) const {
    float qx = noise2D(x + 5.2f, z + 1.3f);
    float qz = noise2D(x + 1.3f, z + 5.2f);
    
    float rx = noise2D(x + 4.0f * qx + 1.7f, z + 4.0f * qz + 9.2f);
    float rz = noise2D(x + 4.0f * qx + 8.3f, z + 4.0f * qz + 2.8f);
    
    x += 4.0f * rx;
    z += 4.0f * rz;
    
    return noise2D(x, z);
}

float WorldGenerator::getSplineHeight(float continentalness, float erosion, float pv) const {
    // Minecraft-like Spline Logic with Interpolation
    
    // 1. Calculate Base Height from Continentalness (Smooth transition from Ocean to Land)
    float baseHeight = 0.0f;
    
    // Control points: {Continentalness, TargetHeight}
    // Must be sorted by Continentalness
    struct Point { float c; float h; };
    Point points[] = {
        {-1.0f, 10.0f},   // Deep Ocean
        {-0.5f, 20.0f},   // Ocean
        {-0.15f, 30.0f},  // Shallow Ocean / Shore Start
        {0.15f, 38.0f},   // Beach/Coastal Plains (Slightly higher start)
        {0.4f, 50.0f},    // Inland
        {1.0f, 70.0f}     // Deep Inland
    };
    
    // Interpolate Base Height
    if (continentalness <= points[0].c) baseHeight = points[0].h;
    else if (continentalness >= points[5].c) baseHeight = points[5].h;
    else {
        for (int i = 0; i < 5; ++i) {
            if (continentalness >= points[i].c && continentalness < points[i+1].c) {
                float t = (continentalness - points[i].c) / (points[i+1].c - points[i].c);
                // Smoothstep for nicer curves
                t = t * t * (3.0f - 2.0f * t); 
                baseHeight = lerp(points[i].h, points[i+1].h, t);
                break;
            }
        }
    }

    // 2. Calculate Terrain Offset from Erosion (Mountains vs Plains)
    // Only apply terrain offset if we are on land (continentalness > 0)
    // Or blend it in as we get closer to land to avoid underwater mountains sticking out weirdly
    
    float terrainOffset = 0.0f;
    // Start blending land features earlier (-0.15) to match the new shore line
    float landFactor = std::clamp((continentalness + 0.15f) * 4.0f, 0.0f, 1.0f); 
    
    if (landFactor > 0.0f) {
        // Erosion Spline with Interpolation to prevent cliffs at biome borders
        // Control points: {Erosion, BaseOffset, Roughness}
        // Adjusted for "Flat Valleys" and "Pointy Peaks"
        struct ErosionPoint { float e; float offset; float roughness; };
        ErosionPoint ePoints[] = {
            {-1.0f, 0.0f, 1.0f},    // Flat Plains (Very smooth)
            {-0.5f, 2.0f, 2.0f},    // Gentle Slopes
            {-0.2f, 5.0f, 4.0f},    // Low Hills / Valley Edges
            {0.2f, 15.0f, 6.0f},    // Highlands / Plateaus
            {0.5f, 40.0f, 10.0f},   // Mountain Base
            {0.8f, 90.0f, 15.0f},   // High Mountains
            {1.0f, 160.0f, 20.0f}   // Sharp Peaks
        };

        float baseOffset = 0.0f;
        float roughness = 0.0f;

        // Interpolate
        if (erosion <= ePoints[0].e) {
            baseOffset = ePoints[0].offset;
            roughness = ePoints[0].roughness;
        } else if (erosion >= ePoints[6].e) {
            baseOffset = ePoints[6].offset;
            roughness = ePoints[6].roughness;
        } else {
            for (int i = 0; i < 6; ++i) {
                if (erosion >= ePoints[i].e && erosion < ePoints[i+1].e) {
                    float t = (erosion - ePoints[i].e) / (ePoints[i+1].e - ePoints[i].e);
                    // Use smoothstep for height to make it gradual
                    float heightT = t * t * (3.0f - 2.0f * t);
                    baseOffset = lerp(ePoints[i].offset, ePoints[i+1].offset, heightT);
                    // Linear for roughness is fine
                    roughness = lerp(ePoints[i].roughness, ePoints[i+1].roughness, t);
                    break;
                }
            }
        }
        
        // Apply mountain scale bias to the offset part (height)
        if (baseOffset > 25.0f) {
             baseOffset = 25.0f + (baseOffset - 25.0f) * mountainScaleBias;
        }

        terrainOffset = baseOffset + pv * roughness;
    }
    
    return baseHeight + terrainOffset * landFactor;
}
