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
    float tempNoise = fbm(x * 0.0003f + offsetTempX, z * 0.0003f + offsetTempZ, 4);
    float t = (tempNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(t + globalTempBias, 0.0f, 1.0f);
}

float WorldGenerator::getHumidity(float x, float z) const {
    // Use specific offset for humidity
    // Scale reduced to 0.0005f for much larger biomes
    float humidNoise = fbm(x * 0.0003f + offsetHumidX, z * 0.0003f + offsetHumidZ, 4);
    float h = (humidNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(h + globalHumidBias, 0.0f, 1.0f);
}

BiomeType WorldGenerator::getBiome(float x, float z) const {
    // Advanced Biome Selection based on Continentalness, Erosion, Temperature, and Humidity
    
    // 1. Re-calculate Terrain Parameters (Must match getHeight logic)
    float contX = x * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentX;
    float contZ = z * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentZ;
    float warpedContX = contX; float warpedContZ = contZ;
    domainWarp(warpedContX, warpedContZ); 
    float continentalness = fbm(warpedContX, warpedContZ, 4);
    
    float eroX = x * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionX;
    float eroZ = z * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionZ;
    float erosion = fbm(eroX, eroZ, 3);
    
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
    float cheese = noise3D(x * 0.012f, y * 0.012f, z * 0.012f);
    float cheeseThreshold = -0.55f + globalCaveDensityBias;
    
    // 2. Spaghetti Caves (Tunnels)
    // Use ridged noise (abs value close to 0)
    float worm1 = noise3D(x * 0.018f + 123.4f, y * 0.025f + 521.2f, z * 0.018f + 921.1f);
    float worm2 = noise3D(x * 0.018f + 921.4f, y * 0.025f + 123.2f, z * 0.018f + 521.1f);
    
    // Vary tunnel width based on depth
    float depthFactor = std::clamp((SEA_LEVEL - y) / 60.0f, 0.0f, 1.0f);
    float tunnelWidth = 0.05f + depthFactor * 0.04f; 
    
    bool isTunnel = (std::abs(worm1) < tunnelWidth && std::abs(worm2) < tunnelWidth);
    bool isRoom = (cheese < cheeseThreshold);
    
    return isTunnel || isRoom;
}

int WorldGenerator::getSurfaceHeight(int x, int z) const {
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
    if (biome == BiomeType::FOREST) treeProb = 0.025f; 
    else if (biome == BiomeType::PLAINS) treeProb = 0.001f;
    else if (biome == BiomeType::MOUNTAINS) treeProb = 0.004f;
    // No trees in DESERT, OCEAN, SNOWY_TUNDRA (unless we add spruce later)
    
    if (r >= treeProb) return false;

    // 2. Spatial check: Suppress if a "better" candidate is nearby
    int radius = 3;
    
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
                // If neighbor has a lower random value, they win
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
                    if (worldY < SEA_LEVEL && globalCaveWaterBias > 0.0f) {
                        blockType = BlockType::WATER;
                    }
                }
                
                chunk->setBlock(x, y, z, Block(blockType));
            }
            
            // 2. Vegetation Pass (Plants)
            int chunkBaseY = static_cast<int>(worldPos.y);
            if (height >= chunkBaseY && height < chunkBaseY + CHUNK_HEIGHT) {
                if (height < SEA_LEVEL) continue;

                int localY = height - chunkBaseY;
                Block below = chunk->getBlock(x, localY - 1, z);
                if (below.getType() == BlockType::GRASS) {
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
    int pad = 2;
    for (int nx = -pad; nx < CHUNK_SIZE + pad; ++nx) {
        for (int nz = -pad; nz < CHUNK_SIZE + pad; ++nz) {
            int worldX = static_cast<int>(worldPos.x) + nx;
            int worldZ = static_cast<int>(worldPos.z) + nz;
            
            BiomeType biome = getBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
            
            if (hasTree(worldX, worldZ, biome)) {
                int treeBaseY = getSurfaceHeight(worldX, worldZ);
                
                if (treeBaseY < SEA_LEVEL) continue;
                if (isCave(static_cast<float>(worldX), static_cast<float>(treeBaseY - 1), static_cast<float>(worldZ))) continue;

                int treeH = getTreeHeight(worldX, worldZ);
                
                int chunkBaseY = static_cast<int>(worldPos.y);
                int treeTopY = treeBaseY + treeH + 1;
                
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
                unsigned int h = seed + worldX * 34123 + worldZ * 23123;
                h = (h ^ (h >> 13)) * 1274126177;
                bool extraLeaves = (h % 2) == 0;

                for (int ly = treeBaseY + treeH - 3; ly <= treeBaseY + treeH; ++ly) {
                    if (ly < chunkBaseY || ly >= chunkBaseY + CHUNK_HEIGHT) continue;
                    
                    int dy = ly - (treeBaseY + treeH);
                    int radius = (dy >= -1) ? 1 : 2;
                    
                    for (int lx = worldX - radius; lx <= worldX + radius; ++lx) {
                        for (int lz = worldZ - radius; lz <= worldZ + radius; ++lz) {
                            int localX = lx - static_cast<int>(worldPos.x);
                            int localZ = lz - static_cast<int>(worldPos.z);
                            
                            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE) {
                                bool isCorner = std::abs(lx - worldX) == radius && std::abs(lz - worldZ) == radius;
                                
                                if (isCorner) {
                                    if (radius == 1) continue;
                                    if (radius == 2) {
                                        if (!extraLeaves || (h % 3 != 0)) continue; 
                                    }
                                }

                                if (lx == worldX && lz == worldZ) continue;
                                
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
    // =====================================================
    // IMPROVED TERRAIN GENERATION with MULTI-OCTAVE FBM,
    // DOMAIN WARPING, RIDGE NOISE, and HEIGHT SHAPING
    // =====================================================
    
    // 1. Continentalness (Macro-scale geography)
    // Determines Oceans vs Land using multi-layer domain warping
    // SCALE: Very low frequency for continent-sized features
    float contX = x * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentX;
    float contZ = z * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentZ;
    
    // Multi-layer domain warping for natural, non-grid coastlines
    float warpedContX = contX;
    float warpedContZ = contZ;
    domainWarp(warpedContX, warpedContZ);
    
    // Apply second layer of warping for extra complexity
    float warp2X = warpedContX + 0.3f * noise2D(warpedContX * 0.5f + 100.0f, warpedContZ * 0.5f + 200.0f);
    float warp2Z = warpedContZ + 0.3f * noise2D(warpedContX * 0.5f + 300.0f, warpedContZ * 0.5f + 400.0f);
    
    // Use FBM with more octaves for continentalness
    float continentalness = fbm(warp2X, warp2Z, 5);
    
    // 2. Erosion (Mid-scale) - Determines mountains vs plains
    // More octaves for natural transitions
    float eroX = x * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionX;
    float eroZ = z * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionZ;
    
    // Warp erosion coordinates too for less grid-aligned features
    float warpedEroX = eroX + 0.4f * noise2D(eroX * 0.7f + 500.0f, eroZ * 0.7f + 600.0f);
    float warpedEroZ = eroZ + 0.4f * noise2D(eroX * 0.7f + 700.0f, eroZ * 0.7f + 800.0f);
    
    float erosion = fbm(warpedEroX, warpedEroZ, 4);
    
    // 3. Peaks & Valleys (Micro-scale / Detail)
    // SCALE: Higher frequency for local terrain features
    float pvX = x * NOISE_SCALE * 0.4f + offsetPVX; 
    float pvZ = z * NOISE_SCALE * 0.4f + offsetPVZ;
    
    // Use FBM with many octaves for smooth rolling terrain
    float smoothPV = fbm(pvX, pvZ, 6);
    
    // Use ridgedMultifractal for sharp alpine-style mountain peaks
    // Parameters tuned for realistic mountain generation:
    // - 6 octaves for detailed ridges
    // - lacunarity 2.0 for standard frequency doubling
    // - gain 0.5 for balanced weight decay
    // - offset 1.0 for sharp ridges
    float alpinePV = ridgedMultifractal(pvX, pvZ, 6, 2.0f, 0.5f, 1.0f);
    
    // Also add turbulence for mid-range hills
    float turbPV = turbulence(pvX * 0.8f, pvZ * 0.8f, 4);
    
    // Blend based on erosion value:
    // - Low erosion (plains) = smooth noise
    // - Mid erosion (hills) = turbulent noise  
    // - High erosion (mountains) = ridged multifractal
    float ridgeFactor = std::clamp((erosion + 0.2f) * 1.5f, 0.0f, 1.0f);
    float turbFactor = std::clamp((erosion + 0.5f) * 1.0f, 0.0f, 1.0f) - ridgeFactor * 0.5f;
    turbFactor = std::clamp(turbFactor, 0.0f, 1.0f);
    
    // Three-way blend: smooth -> turbulent -> alpine
    float pv = smoothPV * (1.0f - turbFactor - ridgeFactor) 
             + turbPV * turbFactor 
             + (alpinePV * 2.0f - 1.0f) * ridgeFactor; // Remap alpinePV from [0,1] to [-1,1]
    
    // 4. Height Shaping (Nonlinear Mapping)
    // Apply power function to create flatter lowlands and steeper peaks
    float rawHeight = getSplineHeight(continentalness, erosion, pv);
    
    // Normalize to [0, 1] range first (approximate based on expected output range)
    float minExpected = 10.0f;  // Deep ocean floor
    float maxExpected = 230.0f; // Tallest peaks
    float normalized = (rawHeight - minExpected) / (maxExpected - minExpected);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    
    // Apply height shaping curve
    // exponent > 1 = flatter lowlands, sharper peaks
    // exponent < 1 = higher average elevation
    float exponent = 1.3f;
    float shaped = std::pow(normalized, exponent);
    
    // Map back to world height
    float finalHeight = minExpected + shaped * (maxExpected - minExpected);
    
    // 5. Simple Thermal Erosion Simulation
    // Smooth out extremely steep areas (prevents impossible cliffs)
    // Sample neighboring heights and blend slightly
    float neighborScale = 2.0f; // Sample distance
    float h1 = getSplineHeight(
        fbm((x + neighborScale) * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentX, z * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentZ, 3),
        fbm((x + neighborScale) * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionX, z * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionZ, 2),
        0.0f
    );
    float h2 = getSplineHeight(
        fbm(x * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentX, (z + neighborScale) * NOISE_SCALE * 0.02f * globalFrequencyBias + offsetContinentZ, 3),
        fbm(x * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionX, (z + neighborScale) * NOISE_SCALE * 0.08f * globalFrequencyBias + offsetErosionZ, 2),
        0.0f
    );
    
    // Blend with neighbors very slightly for smoothing
    float erosionBlend = 0.05f;
    finalHeight = finalHeight * (1.0f - 2.0f * erosionBlend) + h1 * erosionBlend + h2 * erosionBlend;
    
    return finalHeight;
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
    
    // Hash function with better mixing
    auto hash = [&](int i, int j, int k) {
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
    float maxValue = 0.0f;
    
    // Typical fBm parameters
    float persistence = 0.5f;   // Amplitude decay per octave
    float lacunarity = 2.0f;    // Frequency increase per octave
    
    for(int i = 0; i < octaves; i++) {
        total += noise2D(x * frequency, z * frequency) * amplitude;
        
        maxValue += amplitude;
        
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    // Normalize to [-1, 1]
    return total / maxValue;
}

float WorldGenerator::lerp(float a, float b, float t) const {
    return a + t * (b - a);
}

float WorldGenerator::fade(float t) const {
    // Quintic interpolation for smoother gradients
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float WorldGenerator::ridgeNoise(float x, float z) const {
    // Ridged multifractal noise
    // 1.0 - abs(noise) creates sharp peaks
    float n = noise2D(x, z);
    float ridge = 1.0f - std::abs(n);
    // Square to sharpen peaks
    return ridge * ridge;
}

float WorldGenerator::billowNoise(float x, float z) const {
    // Billow noise - abs of noise creates puffy, cloud-like formations
    // Good for rolling hills and dunes
    float n = noise2D(x, z);
    return std::abs(n) * 2.0f - 1.0f; // Remap to [-1, 1]
}

float WorldGenerator::turbulence(float x, float z, int octaves) const {
    // Turbulent noise - sum of absolute fBm
    // Creates swirly, chaotic patterns good for terrain variation
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    
    float persistence = 0.5f;
    float lacunarity = 2.0f;
    
    for(int i = 0; i < octaves; i++) {
        total += std::abs(noise2D(x * frequency, z * frequency)) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    return total / maxValue;
}

float WorldGenerator::ridgedMultifractal(float x, float z, int octaves, float lacunarity, float gain, float offset) const {
    // Ridged Multifractal Noise Algorithm
    // Based on Ken Musgrave's improved ridge noise from "Texturing and Modeling: A Procedural Approach"
    // 
    // Key principles:
    // 1. ridge = offset - abs(noise) creates sharp peaks at zero crossings
    // 2. Each octave is weighted by the previous ridge value (weight accumulation)
    //    This creates "layered" ridges where peaks have finer detail
    // 3. The result is squared to sharpen peaks further
    //
    // Parameters:
    // - lacunarity: Frequency multiplier per octave (typically 2.0)
    // - gain: Controls weight decay (typically 0.5)
    // - offset: Controls ridge sharpness (typically 1.0)
    
    float sum = 0.0f;
    float frequency = 1.0f;
    float amplitude = 0.5f;
    float weight = 1.0f;
    float prev = 1.0f;
    
    for (int i = 0; i < octaves; i++) {
        // Get noise value
        float n = noise2D(x * frequency, z * frequency);
        
        // Create ridge (invert absolute value)
        float ridge = offset - std::abs(n);
        
        // Square to sharpen peaks
        ridge = ridge * ridge;
        
        // Weight by previous octave's ridge value
        // This creates the "eroded" look where peaks have more detail
        ridge *= weight;
        
        // Update weight for next octave
        // Clamp to [0, 1] to prevent runaway values
        weight = std::clamp(ridge * gain, 0.0f, 1.0f);
        
        // Accumulate
        sum += ridge * amplitude;
        prev = ridge;
        
        // Prepare for next octave
        frequency *= lacunarity;
        amplitude *= gain;
    }
    
    // Normalize to approximately [0, 1]
    // The theoretical max depends on octaves but ~1.0 is typical
    return std::clamp(sum, 0.0f, 1.0f);
}

float WorldGenerator::domainWarp(float& x, float& z) const {
    // Multi-layer domain warping for natural terrain
    // Layer 1: Low frequency warp
    float qx = noise2D(x * 0.8f + 5.2f, z * 0.8f + 1.3f);
    float qz = noise2D(x * 0.8f + 1.3f, z * 0.8f + 5.2f);
    
    // Layer 2: Higher frequency warp
    float rx = noise2D(x + 4.0f * qx + 1.7f, z + 4.0f * qz + 9.2f);
    float rz = noise2D(x + 4.0f * qx + 8.3f, z + 4.0f * qz + 2.8f);
    
    // Apply warp with variable strength
    float warpStrength = 3.0f;
    x += warpStrength * rx;
    z += warpStrength * rz;
    
    return noise2D(x, z);
}

float WorldGenerator::getSplineHeight(float continentalness, float erosion, float pv) const {
    // Minecraft-like Spline Logic with Interpolation
    
    // 1. Calculate Base Height from Continentalness 
    float baseHeight = 0.0f;
    
    // Control points for continent height
    struct Point { float c; float h; };
    Point points[] = {
        {-1.0f, 5.0f},    // Deep Ocean
        {-0.6f, 15.0f},   // Ocean
        {-0.3f, 25.0f},   // Shallow Ocean
        {-0.15f, 32.0f},  // Shore Start
        {0.0f, 40.0f},    // Beach/Coastal Plains
        {0.3f, 55.0f},    // Lowlands
        {0.6f, 70.0f},    // Highlands
        {1.0f, 85.0f}     // Deep Inland
    };
    const int numPoints = 8;
    
    // Interpolate Base Height
    if (continentalness <= points[0].c) { 
        baseHeight = points[0].h;
    } else if (continentalness >= points[numPoints-1].c) {
        baseHeight = points[numPoints-1].h;
    } else {
        for (int i = 0; i < numPoints - 1; ++i) {
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
    float terrainOffset = 0.0f;
    float landFactor = std::clamp((continentalness + 0.15f) * 4.0f, 0.0f, 1.0f); 
    
    if (landFactor > 0.0f) {
        // Erosion Spline with Interpolation
        struct ErosionPoint { float e; float offset; float roughness; };
        ErosionPoint ePoints[] = {
            {-1.0f, -5.0f, 1.0f},   // Flat Plains (can dip below base)
            {-0.6f, 0.0f, 2.0f},    // Gentle Slopes
            {-0.3f, 5.0f, 4.0f},    // Low Hills
            {0.0f, 12.0f, 6.0f},    // Rolling Hills
            {0.3f, 25.0f, 10.0f},   // Highlands
            {0.5f, 50.0f, 15.0f},   // Mountain Base
            {0.7f, 90.0f, 25.0f},   // High Mountains
            {0.9f, 130.0f, 35.0f},  // Sharp Peaks
            {1.0f, 160.0f, 45.0f}   // Extreme Peaks
        };
        const int numEPoints = 9;

        float baseOffset = 0.0f;
        float roughness = 0.0f;

        // Interpolate
        if (erosion <= ePoints[0].e) {
            baseOffset = ePoints[0].offset;
            roughness = ePoints[0].roughness;
        } else if (erosion >= ePoints[numEPoints-1].e) {
            baseOffset = ePoints[numEPoints-1].offset;
            roughness = ePoints[numEPoints-1].roughness;
        } else {
            for (int i = 0; i < numEPoints - 1; ++i) {
                if (erosion >= ePoints[i].e && erosion < ePoints[i+1].e) {
                    float t = (erosion - ePoints[i].e) / (ePoints[i+1].e - ePoints[i].e);
                    // Smoothstep for height transition
                    float heightT = t * t * (3.0f - 2.0f * t);
                    baseOffset = lerp(ePoints[i].offset, ePoints[i+1].offset, heightT);
                    // Linear for roughness
                    roughness = lerp(ePoints[i].roughness, ePoints[i+1].roughness, t);
                    break;
                }
            }
        }
        
        // Apply mountain scale bias
        if (baseOffset > 25.0f) {
             baseOffset = 25.0f + (baseOffset - 25.0f) * mountainScaleBias;
        }

        terrainOffset = baseOffset + pv * roughness;
    }
    
    return baseHeight + terrainOffset * landFactor;
}
