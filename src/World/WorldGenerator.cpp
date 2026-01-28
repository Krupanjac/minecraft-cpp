#include "WorldGenerator.h"
#include "../Util/Config.h"
#include "ChunkManager.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <map>

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
            // Ocean colors (used for seagrass if any)
            info.grassColorR = 0.30f; info.grassColorG = 0.65f; info.grassColorB = 0.55f;
            info.foliageColorR = 0.30f; info.foliageColorG = 0.65f; info.foliageColorB = 0.55f;
            info.mapColorR = 0.15f; info.mapColorG = 0.40f; info.mapColorB = 0.80f;
            break;

        case BiomeType::RIVER:
            info.temperature = 0.5f;
            info.humidity = 0.8f;
            info.heightVariation = 0.15f;
            info.surfaceBlock = BlockType::SAND;
            info.subsurfaceBlock = BlockType::GRAVEL;
            info.surfaceDepth = 3;
            info.grassColorR = 0.55f; info.grassColorG = 0.75f; info.grassColorB = 0.45f;
            info.foliageColorR = 0.45f; info.foliageColorG = 0.70f; info.foliageColorB = 0.40f;
            info.mapColorR = 0.25f; info.mapColorG = 0.50f; info.mapColorB = 0.85f;
            break;
            
        case BiomeType::PLAINS:
            info.temperature = 0.6f;
            info.humidity = 0.5f;
            info.heightVariation = 0.5f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Plains - bright yellowish green (Minecraft style)
            info.grassColorR = 0.57f; info.grassColorG = 0.74f; info.grassColorB = 0.35f;
            info.foliageColorR = 0.47f; info.foliageColorG = 0.68f; info.foliageColorB = 0.32f;
            info.mapColorR = 0.55f; info.mapColorG = 0.75f; info.mapColorB = 0.35f;
            break;
            
        case BiomeType::DESERT:
            info.temperature = 0.9f;
            info.humidity = 0.1f;
            info.heightVariation = 0.4f;
            info.surfaceBlock = BlockType::SAND;
            info.subsurfaceBlock = BlockType::SANDSTONE;
            info.surfaceDepth = 5;
            // Desert - no grass, but warm tones
            info.grassColorR = 0.75f; info.grassColorG = 0.72f; info.grassColorB = 0.42f;
            info.foliageColorR = 0.68f; info.foliageColorG = 0.65f; info.foliageColorB = 0.38f;
            info.mapColorR = 0.86f; info.mapColorG = 0.78f; info.mapColorB = 0.55f;
            break;
            
        case BiomeType::FOREST:
            info.temperature = 0.5f;
            info.humidity = 0.8f;
            info.heightVariation = 0.6f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Forest - rich dark green
            info.grassColorR = 0.35f; info.grassColorG = 0.60f; info.grassColorB = 0.28f;
            info.foliageColorR = 0.30f; info.foliageColorG = 0.55f; info.foliageColorB = 0.25f;
            info.mapColorR = 0.20f; info.mapColorG = 0.50f; info.mapColorB = 0.20f;
            break;
            
        case BiomeType::BIRCH_FOREST:
            info.temperature = 0.45f;
            info.humidity = 0.7f;
            info.heightVariation = 0.5f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Birch Forest - lighter, more vibrant green
            info.grassColorR = 0.52f; info.grassColorG = 0.72f; info.grassColorB = 0.38f;
            info.foliageColorR = 0.48f; info.foliageColorG = 0.68f; info.foliageColorB = 0.35f;
            info.mapColorR = 0.45f; info.mapColorG = 0.68f; info.mapColorB = 0.38f;
            break;
            
        case BiomeType::TAIGA:
            info.temperature = 0.2f;
            info.humidity = 0.6f;
            info.heightVariation = 0.6f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Taiga - cold blue-green tint
            info.grassColorR = 0.45f; info.grassColorG = 0.60f; info.grassColorB = 0.45f;
            info.foliageColorR = 0.38f; info.foliageColorG = 0.55f; info.foliageColorB = 0.40f;
            info.mapColorR = 0.35f; info.mapColorG = 0.55f; info.mapColorB = 0.40f;
            break;
            
        case BiomeType::JUNGLE:
            info.temperature = 0.85f;
            info.humidity = 0.95f;
            info.heightVariation = 0.8f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 5;
            // Jungle - lush vibrant green (most saturated)
            info.grassColorR = 0.35f; info.grassColorG = 0.78f; info.grassColorB = 0.22f;
            info.foliageColorR = 0.30f; info.foliageColorG = 0.75f; info.foliageColorB = 0.18f;
            info.mapColorR = 0.25f; info.mapColorG = 0.65f; info.mapColorB = 0.15f;
            break;
            
        case BiomeType::SWAMP:
            info.temperature = 0.6f;
            info.humidity = 0.9f;
            info.heightVariation = 0.2f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Swamp - murky olive/brown-green
            info.grassColorR = 0.42f; info.grassColorG = 0.52f; info.grassColorB = 0.32f;
            info.foliageColorR = 0.40f; info.foliageColorG = 0.48f; info.foliageColorB = 0.30f;
            info.mapColorR = 0.38f; info.mapColorG = 0.48f; info.mapColorB = 0.30f;
            break;
            
        case BiomeType::MOUNTAINS:
            info.temperature = 0.3f;
            info.humidity = 0.4f;
            info.heightVariation = 1.5f;
            // Mountains use grass at lower elevations, stone higher up (handled in generate)
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 3;
            // Mountains - cool grayish green
            info.grassColorR = 0.50f; info.grassColorG = 0.62f; info.grassColorB = 0.42f;
            info.foliageColorR = 0.45f; info.foliageColorG = 0.58f; info.foliageColorB = 0.38f;
            info.mapColorR = 0.50f; info.mapColorG = 0.50f; info.mapColorB = 0.55f;
            break;
            
        case BiomeType::SNOWY_TUNDRA:
            info.temperature = 0.0f;
            info.humidity = 0.3f;
            info.heightVariation = 0.4f;
            info.surfaceBlock = BlockType::SNOW;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 3;
            // Snowy - cold aqua/teal tint
            info.grassColorR = 0.50f; info.grassColorG = 0.65f; info.grassColorB = 0.55f;
            info.foliageColorR = 0.42f; info.foliageColorG = 0.58f; info.foliageColorB = 0.48f;
            info.mapColorR = 0.86f; info.mapColorG = 0.90f; info.mapColorB = 0.94f;
            break;
            
        case BiomeType::SAVANNA:
            info.temperature = 0.85f;
            info.humidity = 0.3f;
            info.heightVariation = 0.4f;
            info.surfaceBlock = BlockType::GRASS;
            info.subsurfaceBlock = BlockType::DIRT;
            info.surfaceDepth = 4;
            // Savanna - dry yellowish/tan green
            info.grassColorR = 0.72f; info.grassColorG = 0.72f; info.grassColorB = 0.35f;
            info.foliageColorR = 0.68f; info.foliageColorG = 0.68f; info.foliageColorB = 0.32f;
            info.mapColorR = 0.70f; info.mapColorG = 0.70f; info.mapColorB = 0.40f;
            break;
    }
    
    return info;
}

float WorldGenerator::getTemperature(float x, float z) const {
    // Use specific offset for temperature
    // Scale reduced significantly for much larger biomes (approx 2500+ blocks)
    float tempNoise = fbm(x * 0.00012f + offsetTempX, z * 0.00012f + offsetTempZ, 4);
    float t = (tempNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(t + globalTempBias, 0.0f, 1.0f);
}

float WorldGenerator::getHumidity(float x, float z) const {
    // Use specific offset for humidity
    // Scale reduced significantly for much larger biomes
    float humidNoise = fbm(x * 0.00012f + offsetHumidX, z * 0.00012f + offsetHumidZ, 4);
    float h = (humidNoise + 1.0f) * 0.5f; // Map to [0, 1]
    return std::clamp(h + globalHumidBias, 0.0f, 1.0f);
}

BiomeType WorldGenerator::getBiome(float x, float z) const {
    // Biome Selection matching the new terrain generation
    
    // SCALE FACTORS - Must match getHeight (reduced for larger biomes/oceans)
    const float CONTINENT_SCALE = 0.0004f * globalFrequencyBias;  // Larger continents/oceans
    const float MOUNTAIN_SCALE = 0.0015f * globalFrequencyBias;   // Larger mountain ranges
    
    // 1. Continentalness (ocean vs land)
    float contX = x * CONTINENT_SCALE + offsetContinentX;
    float contZ = z * CONTINENT_SCALE + offsetContinentZ;
    float warpX = contX, warpZ = contZ;
    domainWarp(warpX, warpZ);
    float continentalness = fbm(warpX, warpZ, 4);
    // Bias towards land but allow proper oceans
    continentalness = std::clamp(continentalness + 0.10f, -1.0f, 1.0f);
    
    // 2. Mountain factor (same as getHeight)
    float mtX = x * MOUNTAIN_SCALE + offsetErosionX;
    float mtZ = z * MOUNTAIN_SCALE + offsetErosionZ;
    float mtWarpX = mtX + 0.5f * noise2D(mtX * 0.5f + 1000.0f, mtZ * 0.5f + 2000.0f);
    float mtWarpZ = mtZ + 0.5f * noise2D(mtX * 0.5f + 3000.0f, mtZ * 0.5f + 4000.0f);
    float mountainNoise = ridgedMultifractal(mtWarpX, mtWarpZ, 5, 2.2f, 0.6f, 1.0f);
    // Match getHeight
    float mountainFactor = std::clamp((mountainNoise - 0.40f) * 3.1f, 0.0f, 1.0f);
    
    // 3. Temperature and humidity
    float temp = getTemperature(x, z);
    float humid = getHumidity(x, z);
    
    // Get actual height for height-based biome decisions
    float height = getHeight(x, z);

    // River mask - reduced probability (higher threshold = fewer rivers)
    float rX = x * 0.0025f + offsetPVX * 0.25f + 31000.0f;  // Lower frequency
    float rZ = z * 0.0025f + offsetPVZ * 0.25f + 42000.0f;
    float riverBase = fbm(rX, rZ, 3);
    float riverVal = 1.0f - std::abs(riverBase);
    float riverMask = std::pow(std::clamp((riverVal - 0.85f) / 0.15f, 0.0f, 1.0f), 3.0f);  // Higher threshold = rarer rivers
    
    // ========== BIOME SELECTION ==========
    
    // OCEAN (proper large oceans)
    if (continentalness < -0.25f && height < (float)SEA_LEVEL + 1.0f) {
        return BiomeType::OCEAN;
    }
    
    // Deep underwater is always ocean
    if (height < (float)SEA_LEVEL - 8.0f) {
        return BiomeType::OCEAN;
    }

    // RIVERS (river-like water channels) - much rarer
    if (riverMask > 0.5f && height < (float)SEA_LEVEL + 2.0f && continentalness > -0.1f) {
        return BiomeType::RIVER;
    }
    
    // SWAMP (low, wet areas near water level - but not too common)
    if (height < (float)SEA_LEVEL + 4.0f && humid > 0.75f && temp > 0.45f && temp < 0.75f) {
        return BiomeType::SWAMP;
    }
    
    // DESERT - hot and dry flatlands (expanded range)
    if (temp > 0.65f && humid < 0.30f && mountainFactor < 0.25f && height < 80.0f) {
        return BiomeType::DESERT;
    }
    
    // JUNGLE - hot and very wet (isolated, requires both high temp AND high humidity)
    if (temp > 0.70f && humid > 0.75f && mountainFactor < 0.30f) {
        return BiomeType::JUNGLE;
    }
    
    // MOUNTAINS (based on mountain factor and height) - NO trees on pure stone
    if (mountainFactor > 0.38f || height > 95.0f) {
        if (temp < 0.30f || height > 125.0f) {
            return BiomeType::SNOWY_TUNDRA; // Snowy mountain peaks
        }
        return BiomeType::MOUNTAINS;
    }
    
    // SNOWY TUNDRA - cold flat areas (for snowy trees)
    if (temp < 0.25f) {
        return BiomeType::SNOWY_TUNDRA;
    }
    
    // TAIGA - cold but not freezing (spruce forests)
    if (temp < 0.40f) {
        if (humid > 0.4f) return BiomeType::TAIGA;
        return BiomeType::SNOWY_TUNDRA;
    }
    
    // SAVANNA - hot and dry but not desert
    if (temp > 0.65f && humid < 0.45f) {
        return BiomeType::SAVANNA;
    }
    
    // HILLS / HIGHLANDS - with more variety
    if (mountainFactor > 0.18f || height > 72.0f) {
        if (humid > 0.6f) return BiomeType::FOREST;
        if (temp < 0.45f) return BiomeType::TAIGA;
        return BiomeType::BIRCH_FOREST;
    }
    
    // Temperate regions
    if (humid > 0.65f) return BiomeType::FOREST;
    if (humid > 0.50f) return BiomeType::BIRCH_FOREST;
    
    return BiomeType::PLAINS;
}

bool WorldGenerator::isCave(float x, float y, float z) const {
    // Extended cave system - deeper and more natural
    
    // Caves can go much deeper now (down to y=0, but not in bedrock)
    if (y < 2) return false;
    
    // Get surface height for this column
    int surfaceHeight = getSurfaceHeight(static_cast<int>(x), static_cast<int>(z));
    
    // Don't carve caves through underwater areas (oceans/rivers)
    if (surfaceHeight < SEA_LEVEL && y < SEA_LEVEL) {
        return false;
    }
    
    // CRITICAL: Don't carve caves too close to surface to prevent ugly holes
    // Caves should start at least 5 blocks below the surface
    int depthBelowSurface = surfaceHeight - static_cast<int>(y);
    if (depthBelowSurface < 5) {
        return false;  // Too close to surface - no caves here
    }
    
    // Natural cave entrance zones - only allow near-surface caves where terrain dips
    // This creates natural-looking cave entrances in hillsides
    if (depthBelowSurface < 12) {
        // Near surface - only allow caves if there's a "entrance" condition
        float entranceNoise = noise2D(x * 0.02f + 5000.0f, z * 0.02f + 6000.0f);
        if (entranceNoise < 0.3f) {
            return false;  // No entrance here
        }
        // Additional check: entrances only on slopes (where height changes)
        float heightNearby = getHeight(x + 4.0f, z) + getHeight(x - 4.0f, z) + 
                            getHeight(x, z + 4.0f) + getHeight(x, z - 4.0f);
        float avgHeight = heightNearby / 4.0f;
        float slope = std::abs(avgHeight - surfaceHeight);
        if (slope < 3.0f) {
            return false;  // Too flat for natural entrance
        }
    }
    
    // Depth factor - caves get larger and more common deeper down
    float depth = static_cast<float>(SEA_LEVEL) - y;
    float depthFactor = std::clamp(depth / 80.0f, 0.0f, 1.0f);
    
    // 1. Cheese Caves (Large Rooms) - more common deeper
    float cheese = noise3D(x * 0.010f, y * 0.010f, z * 0.010f);
    float cheeseThreshold = -0.50f + globalCaveDensityBias - depthFactor * 0.1f;
    bool isRoom = (cheese < cheeseThreshold);
    
    // 2. Spaghetti Caves (Tunnels) - winding tunnels
    float worm1 = noise3D(x * 0.015f + 123.4f, y * 0.020f + 521.2f, z * 0.015f + 921.1f);
    float worm2 = noise3D(x * 0.015f + 921.4f, y * 0.020f + 123.2f, z * 0.015f + 521.1f);
    
    // Tunnel width increases with depth
    float tunnelWidth = 0.045f + depthFactor * 0.05f;
    bool isTunnel = (std::abs(worm1) < tunnelWidth && std::abs(worm2) < tunnelWidth);
    
    // 3. Noodle Caves (Thin, snaking passages) - new layer
    float noodle1 = noise3D(x * 0.025f + 333.0f, y * 0.030f + 444.0f, z * 0.025f + 555.0f);
    float noodle2 = noise3D(x * 0.025f + 666.0f, y * 0.030f + 777.0f, z * 0.025f + 888.0f);
    float noodleWidth = 0.03f + depthFactor * 0.02f;
    bool isNoodle = (std::abs(noodle1) < noodleWidth && std::abs(noodle2) < noodleWidth);
    
    // 4. Deep caverns - massive caves only at great depths
    bool isDeepCavern = false;
    if (y < 0) {  // Below sea level origin
        float cavern = noise3D(x * 0.006f, y * 0.008f, z * 0.006f);
        isDeepCavern = (cavern < -0.55f);
    }
    
    return isTunnel || isRoom || isNoodle || isDeepCavern;
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
    switch (biome) {
        case BiomeType::FOREST:        treeProb = 0.025f; break;
        case BiomeType::BIRCH_FOREST:  treeProb = 0.022f; break;
        case BiomeType::TAIGA:         treeProb = 0.028f; break;
        case BiomeType::JUNGLE:        treeProb = 0.045f; break; // Dense jungle
        case BiomeType::SWAMP:         treeProb = 0.015f; break;
        case BiomeType::PLAINS:        treeProb = 0.001f; break;
        case BiomeType::SAVANNA:       treeProb = 0.003f; break;
        case BiomeType::MOUNTAINS:     treeProb = 0.004f; break;
        case BiomeType::SNOWY_TUNDRA:  treeProb = 0.002f; break; // Sparse spruce
        default:                       treeProb = 0.0f;   break; // No trees
    }
    
    if (r >= treeProb) return false;

    // 2. Spatial check: Suppress if a "better" candidate is nearby
    int radius = (biome == BiomeType::JUNGLE) ? 2 : 3; // Jungle trees closer together
    
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

int WorldGenerator::getTreeHeight(int x, int z, BiomeType biome) const {
    unsigned int h = seed + x * 123 + z * 456;
    h = (h ^ (h >> 13)) * 1274126177;
    
    // Height variation based on biome
    switch (biome) {
        case BiomeType::JUNGLE:       return 8 + (h % 8);  // 8-15 tall jungle trees
        case BiomeType::TAIGA:        return 6 + (h % 5);  // 6-10 tall spruce
        case BiomeType::SWAMP:        return 4 + (h % 3);  // 4-6 shorter swamp trees
        case BiomeType::BIRCH_FOREST: return 5 + (h % 4);  // 5-8 birch trees
        default:                      return 4 + (h % 5);  // 4-8 oak trees
    }
}

TreeType WorldGenerator::getTreeType(BiomeType biome) const {
    switch (biome) {
        case BiomeType::BIRCH_FOREST: return TreeType::BIRCH;
        case BiomeType::TAIGA:        return TreeType::SPRUCE;
        case BiomeType::SNOWY_TUNDRA: return TreeType::SPRUCE;
        case BiomeType::JUNGLE:       return TreeType::JUNGLE;
        case BiomeType::SWAMP:        return TreeType::OAK;     // Swamp oak
        case BiomeType::FOREST:       return TreeType::OAK;
        case BiomeType::PLAINS:       return TreeType::OAK;
        case BiomeType::SAVANNA:      return TreeType::OAK;     // Would be acacia if available
        case BiomeType::MOUNTAINS:    return TreeType::SPRUCE;
        default:                      return TreeType::NONE;
    }
}

BlockType WorldGenerator::getLogType(TreeType tree) const {
    switch (tree) {
        case TreeType::OAK:    return BlockType::OAK_LOG;
        case TreeType::BIRCH:  return BlockType::BIRCH_LOG;
        case TreeType::SPRUCE: return BlockType::SPRUCE_LOG;
        case TreeType::JUNGLE: return BlockType::JUNGLE_LOG;
        default:               return BlockType::OAK_LOG;
    }
}

BlockType WorldGenerator::getLeavesType(TreeType tree) const {
    switch (tree) {
        case TreeType::OAK:    return BlockType::OAK_LEAVES;
        case TreeType::BIRCH:  return BlockType::BIRCH_LEAVES;
        case TreeType::SPRUCE: return BlockType::SPRUCE_LEAVES;
        case TreeType::JUNGLE: return BlockType::JUNGLE_LEAVES;
        default:               return BlockType::OAK_LEAVES;
    }
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
            float temp = getTemperature(static_cast<float>(worldX), static_cast<float>(worldZ));
            
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
                    else if (worldY < height - 1) {
                        // River: gravel under the bed (especially underwater), sand on dry banks
                        if (biome == BiomeType::RIVER && height <= SEA_LEVEL) blockType = BlockType::GRAVEL;
                        else blockType = biomeInfo.subsurfaceBlock;
                    } else if (worldY < height) {
                        blockType = biomeInfo.surfaceBlock;

                        // River: if the bed is underwater, top layer should be gravel (sand is for shores)
                        if (biome == BiomeType::RIVER && worldY < SEA_LEVEL) blockType = BlockType::GRAVEL;

                        // MOUNTAINS: transition from grass to stone at higher elevations
                        // Below ~SEA_LEVEL+30: grass/dirt (tree zone)
                        // Above ~SEA_LEVEL+30: stone (barren peaks)
                        if (biome == BiomeType::MOUNTAINS) {
                            int mountainTreeLine = SEA_LEVEL + 30;
                            if (worldY > mountainTreeLine) {
                                // Use noise to create patchy transition zone
                                float transitionNoise = noise2D(worldX * 0.05f + 1234.0f, worldZ * 0.05f + 5678.0f);
                                int adjustedTreeLine = mountainTreeLine + static_cast<int>(transitionNoise * 8.0f);
                                if (worldY > adjustedTreeLine) {
                                    blockType = BlockType::STONE;
                                }
                            }
                        }

                        // Snow line: add more snow on mountain tops, and make peaks feel less bare
                        // (Temperature + altitude based, regardless of biome classification)
                        if (worldY >= SEA_LEVEL + 55 && temp < 0.55f) {
                            blockType = BlockType::SNOW;
                        }

                        if (blockType == BlockType::SNOW && worldY < SEA_LEVEL) blockType = BlockType::ICE;
                    } else if (worldY < SEA_LEVEL) {
                        // Fill water to sea level - this covers open water bodies
                        if (biome == BiomeType::SNOWY_TUNDRA && worldY == SEA_LEVEL - 1) blockType = BlockType::ICE;
                        else blockType = BlockType::WATER;
                    }
                }
                // Caves are just air - no water in caves at all
                
                chunk->setBlock(x, y, z, Block(blockType));
            }
            
            // 2. Vegetation Pass (Plants)
            int chunkBaseY = static_cast<int>(worldPos.y);
            if (height >= chunkBaseY && height < chunkBaseY + CHUNK_HEIGHT) {
                if (height < SEA_LEVEL) continue;

                int localY = height - chunkBaseY;
                Block below = chunk->getBlock(x, localY - 1, z);
                BlockType belowType = below.getType();
                
                unsigned int h = seed + worldX * 374761393 + worldZ * 668265263;
                h = (h ^ (h >> 13)) * 1274126177;
                float r = (h & 0xFFFF) / 65536.0f;
                
                // Sugar cane near water
                if (belowType == BlockType::GRASS || belowType == BlockType::SAND) {
                    // Check if near water
                    bool nearWater = false;
                    for (int dx = -1; dx <= 1 && !nearWater; ++dx) {
                        for (int dz = -1; dz <= 1 && !nearWater; ++dz) {
                            if (dx == 0 && dz == 0) continue;
                            int checkX = x + dx;
                            int checkZ = z + dz;
                            if (checkX >= 0 && checkX < CHUNK_SIZE && checkZ >= 0 && checkZ < CHUNK_SIZE) {
                                Block neighbor = chunk->getBlock(checkX, localY - 1, checkZ);
                                if (neighbor.getType() == BlockType::WATER) nearWater = true;
                            }
                        }
                    }
                    
                    if (nearWater && r < 0.08f && biome != BiomeType::DESERT && biome != BiomeType::SNOWY_TUNDRA) {
                        // Sugar cane (1-3 blocks tall)
                        int caneHeight = 1 + (h % 3);
                        for (int cy = 0; cy < caneHeight && (localY + cy) < CHUNK_HEIGHT; ++cy) {
                            chunk->setBlock(x, localY + cy, z, Block(BlockType::SUGAR_CANE));
                        }
                        continue;
                    }
                }
                
                // Normal vegetation on grass
                if (belowType == BlockType::GRASS) {
                    float plantProb = 0.0f;
                    float flowerChance = 0.1f; // 10% of plants are flowers
                    
                    switch (biome) {
                        case BiomeType::PLAINS:       plantProb = 0.25f; flowerChance = 0.15f; break;
                        case BiomeType::FOREST:       plantProb = 0.12f; flowerChance = 0.08f; break;
                        case BiomeType::BIRCH_FOREST: plantProb = 0.15f; flowerChance = 0.12f; break;
                        case BiomeType::JUNGLE:       plantProb = 0.35f; flowerChance = 0.05f; break;
                        case BiomeType::SWAMP:        plantProb = 0.20f; flowerChance = 0.02f; break;
                        case BiomeType::SAVANNA:      plantProb = 0.18f; flowerChance = 0.08f; break;
                        case BiomeType::TAIGA:        plantProb = 0.08f; flowerChance = 0.03f; break;
                        case BiomeType::MOUNTAINS:    plantProb = 0.06f; flowerChance = 0.05f; break;
                        default:                      plantProb = 0.0f;  break;
                    }
                    
                    if (r < plantProb) {
                        BlockType plant = BlockType::TALL_GRASS;
                        if (((h >> 16) & 0xFF) < static_cast<unsigned int>(flowerChance * 255.0f)) {
                            plant = BlockType::ROSE;
                        }
                        chunk->setBlock(x, localY, z, Block(plant));
                    }
                }
                
                // Dead bushes on sand (desert)
                if (belowType == BlockType::SAND && biome == BiomeType::DESERT) {
                    if (r < 0.02f) {
                        // Use cobweb as a placeholder for dead bush visual (or TALL_GRASS if no dead bush)
                        chunk->setBlock(x, localY, z, Block(BlockType::TALL_GRASS));
                    }
                }
            }
        }
    }
    
    // 3. Tree Pass (Neighborhood Search)
    int pad = 3; // Increased for larger jungle trees
    for (int nx = -pad; nx < CHUNK_SIZE + pad; ++nx) {
        for (int nz = -pad; nz < CHUNK_SIZE + pad; ++nz) {
            int worldX = static_cast<int>(worldPos.x) + nx;
            int worldZ = static_cast<int>(worldPos.z) + nz;
            
            BiomeType biome = getBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
            
            if (hasTree(worldX, worldZ, biome)) {
                int treeBaseY = getSurfaceHeight(worldX, worldZ);
                
                if (treeBaseY < SEA_LEVEL) continue;
                if (isCave(static_cast<float>(worldX), static_cast<float>(treeBaseY - 1), static_cast<float>(worldZ))) continue;

                // Check if surface block can support a tree (must be grass, dirt, or snow)
                // This prevents trees from spawning on stone in mountains
                int localTreeX = worldX - static_cast<int>(worldPos.x);
                int localTreeZ = worldZ - static_cast<int>(worldPos.z);
                int localBaseY = treeBaseY - static_cast<int>(worldPos.y) - 1;
                
                bool canSupportTree = false;
                if (localTreeX >= 0 && localTreeX < CHUNK_SIZE && localTreeZ >= 0 && localTreeZ < CHUNK_SIZE &&
                    localBaseY >= 0 && localBaseY < CHUNK_HEIGHT) {
                    Block groundBlock = chunk->getBlock(localTreeX, localBaseY, localTreeZ);
                    BlockType groundType = groundBlock.getType();
                    canSupportTree = (groundType == BlockType::GRASS || 
                                     groundType == BlockType::DIRT ||
                                     groundType == BlockType::SNOW);
                } else {
                    // For trees outside chunk bounds, check biome info
                    BiomeInfo info = getBiomeInfo(biome);
                    canSupportTree = (info.surfaceBlock == BlockType::GRASS ||
                                     info.surfaceBlock == BlockType::DIRT ||
                                     info.surfaceBlock == BlockType::SNOW);
                    // For mountains, check elevation for tree line
                    if (biome == BiomeType::MOUNTAINS && treeBaseY > SEA_LEVEL + 38) {
                        canSupportTree = false;  // Above tree line
                    }
                }
                
                if (!canSupportTree) continue;

                TreeType treeType = getTreeType(biome);
                if (treeType == TreeType::NONE) continue;
                
                int treeH = getTreeHeight(worldX, worldZ, biome);
                BlockType logType = getLogType(treeType);
                BlockType leavesType = getLeavesType(treeType);
                
                int chunkBaseY = static_cast<int>(worldPos.y);
                int treeTopY = treeBaseY + treeH + 1;
                
                if (treeTopY < chunkBaseY || treeBaseY > chunkBaseY + CHUNK_HEIGHT) continue;
                
                unsigned int h = seed + worldX * 34123 + worldZ * 23123;
                h = (h ^ (h >> 13)) * 1274126177;
                
                // Check if this is snowy biome - we'll add snow to leaves
                bool addSnowToLeaves = (biome == BiomeType::SNOWY_TUNDRA || 
                                       (biome == BiomeType::TAIGA && getTemperature(static_cast<float>(worldX), static_cast<float>(worldZ)) < 0.25f));
                
                // Draw tree based on type
                if (treeType == TreeType::SPRUCE) {
                    // Spruce tree - conical shape
                    // Draw Trunk
                    if (nx >= 0 && nx < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                        for (int i = 0; i < treeH; ++i) {
                            int wy = treeBaseY + i;
                            if (wy >= chunkBaseY && wy < chunkBaseY + CHUNK_HEIGHT) {
                                chunk->setBlock(nx, wy - chunkBaseY, nz, Block(logType));
                            }
                        }
                    }
                    
                    // Track highest leaf at each position for snow placement
                    std::map<std::pair<int,int>, int> highestLeafY;
                    
                    // Conical leaves
                    for (int ly = treeBaseY + 2; ly <= treeBaseY + treeH + 1; ++ly) {
                        if (ly < chunkBaseY || ly >= chunkBaseY + CHUNK_HEIGHT) continue;
                        
                        int dy = ly - (treeBaseY + treeH);
                        int radius = (dy >= 0) ? 0 : std::min(2, (-dy) / 2 + 1);
                        
                        for (int lx = worldX - radius; lx <= worldX + radius; ++lx) {
                            for (int lz = worldZ - radius; lz <= worldZ + radius; ++lz) {
                                int localX = lx - static_cast<int>(worldPos.x);
                                int localZ = lz - static_cast<int>(worldPos.z);
                                
                                if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE) {
                                    bool isCorner = std::abs(lx - worldX) == radius && std::abs(lz - worldZ) == radius;
                                    if (isCorner && radius > 1) continue;
                                    
                                    Block existing = chunk->getBlock(localX, ly - chunkBaseY, localZ);
                                    if (existing.getType() == BlockType::AIR || existing.isCrossModel()) {
                                        chunk->setBlock(localX, ly - chunkBaseY, localZ, Block(leavesType));
                                        // Track highest leaf for snow
                                        auto key = std::make_pair(localX, localZ);
                                        if (highestLeafY.find(key) == highestLeafY.end() || ly > highestLeafY[key]) {
                                            highestLeafY[key] = ly;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // Add snow layer on top of leaves for snowy biomes
                    if (addSnowToLeaves) {
                        for (auto& pair : highestLeafY) {
                            int localX = pair.first.first;
                            int localZ = pair.first.second;
                            int snowY = pair.second + 1;
                            
                            if (snowY >= chunkBaseY && snowY < chunkBaseY + CHUNK_HEIGHT) {
                                int localSnowY = snowY - chunkBaseY;
                                Block existing = chunk->getBlock(localX, localSnowY, localZ);
                                if (existing.getType() == BlockType::AIR) {
                                    chunk->setBlock(localX, localSnowY, localZ, Block(BlockType::SNOW));
                                }
                            }
                        }
                    }
                } else if (treeType == TreeType::JUNGLE) {
                    // Jungle tree - thick trunk, dense canopy
                    // Draw Trunk (2x2 for large trees)
                    bool largeTrunk = treeH > 10;
                    int trunkSize = largeTrunk ? 2 : 1;
                    
                    for (int tx = 0; tx < trunkSize; ++tx) {
                        for (int tz = 0; tz < trunkSize; ++tz) {
                            int localX = nx + tx;
                            int localZ = nz + tz;
                            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE) {
                                for (int i = 0; i < treeH; ++i) {
                                    int wy = treeBaseY + i;
                                    if (wy >= chunkBaseY && wy < chunkBaseY + CHUNK_HEIGHT) {
                                        chunk->setBlock(localX, wy - chunkBaseY, localZ, Block(logType));
                                    }
                                }
                            }
                        }
                    }
                    
                    // Dense canopy
                    int canopyStart = treeBaseY + treeH - 4;
                    for (int ly = canopyStart; ly <= treeBaseY + treeH + 1; ++ly) {
                        if (ly < chunkBaseY || ly >= chunkBaseY + CHUNK_HEIGHT) continue;
                        
                        int dy = ly - (treeBaseY + treeH);
                        int radius = (dy >= 0) ? 2 : 3;
                        
                        for (int lx = worldX - radius; lx <= worldX + radius + (largeTrunk ? 1 : 0); ++lx) {
                            for (int lz = worldZ - radius; lz <= worldZ + radius + (largeTrunk ? 1 : 0); ++lz) {
                                int localX = lx - static_cast<int>(worldPos.x);
                                int localZ = lz - static_cast<int>(worldPos.z);
                                
                                if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE) {
                                    bool isCorner = (std::abs(lx - worldX) >= radius) && (std::abs(lz - worldZ) >= radius);
                                    if (isCorner && ((h + ly) % 3 == 0)) continue;
                                    
                                    Block existing = chunk->getBlock(localX, ly - chunkBaseY, localZ);
                                    if (existing.getType() == BlockType::AIR || existing.isCrossModel()) {
                                        chunk->setBlock(localX, ly - chunkBaseY, localZ, Block(leavesType));
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // Oak/Birch tree - standard shape
                    // Draw Trunk
                    if (nx >= 0 && nx < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                        for (int i = 0; i < treeH; ++i) {
                            int wy = treeBaseY + i;
                            if (wy >= chunkBaseY && wy < chunkBaseY + CHUNK_HEIGHT) {
                                chunk->setBlock(nx, wy - chunkBaseY, nz, Block(logType));
                            }
                        }
                    }
                    
                    // Standard leaves
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
                                        chunk->setBlock(localX, ly - chunkBaseY, localZ, Block(leavesType));
                                    }
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
    // REALISTIC TERRAIN GENERATION
    // Creates: Ocean, Beaches, Plains, Hills, Mountain Ranges
    // =====================================================
    
    // SCALE FACTORS - Adjust these to change feature sizes
    const float CONTINENT_SCALE = 0.0008f * globalFrequencyBias;   // Very large landmasses (~1250 blocks)
    const float MOUNTAIN_SCALE = 0.002f * globalFrequencyBias;     // Mountain ranges (~500 blocks)
    const float HILLS_SCALE = 0.008f * globalFrequencyBias;        // Hills (~125 blocks)
    const float DETAIL_SCALE = 0.03f * globalFrequencyBias;        // Local detail (~33 blocks)
    
    // ========== 1. CONTINENTALNESS ==========
    // Determines ocean vs land. Warped for natural coastlines.
    float contX = x * CONTINENT_SCALE + offsetContinentX;
    float contZ = z * CONTINENT_SCALE + offsetContinentZ;
    
    // Domain warp for organic coastlines
    float warpX = contX, warpZ = contZ;
    domainWarp(warpX, warpZ);
    
    // Multi-octave for continental shapes
    float continentalness = fbm(warpX, warpZ, 4);
    // Bias towards land so the world isn't overly ocean-heavy.
    // This is the simplest way to get more flatlands without changing SEA_LEVEL.
    // Increased bias (0.20) for more land coverage, less ocean
    continentalness = std::clamp(continentalness + 0.20f, -1.0f, 1.0f);
    
    // ========== 2. MOUNTAIN RANGE NOISE ==========
    // Separate noise layer specifically for mountain ranges
    // Uses ridged noise for dramatic mountain chains
    float mtX = x * MOUNTAIN_SCALE + offsetErosionX;
    float mtZ = z * MOUNTAIN_SCALE + offsetErosionZ;
    
    // Warp to break grid alignment
    float mtWarpX = mtX + 0.5f * noise2D(mtX * 0.5f + 1000.0f, mtZ * 0.5f + 2000.0f);
    float mtWarpZ = mtZ + 0.5f * noise2D(mtX * 0.5f + 3000.0f, mtZ * 0.5f + 4000.0f);
    
    // Use ridged multifractal for mountain chains
    float mountainNoise = ridgedMultifractal(mtWarpX, mtWarpZ, 5, 2.2f, 0.6f, 1.0f);
    
    // Create clear mountain vs non-mountain distinction (visible ranges, but not everywhere)
    float mountainFactor = std::clamp((mountainNoise - 0.40f) * 3.1f, 0.0f, 1.0f);
    
    // Choose between sharp and gentle mountain styles per-range so both can spawn
    float mountainStyleNoise = (noise2D(mtWarpX * 0.6f + 7200.0f, mtWarpZ * 0.6f + 9100.0f) + 1.0f) * 0.5f;
    float sharpWeight = std::pow(std::clamp(mountainStyleNoise, 0.0f, 1.0f), 1.4f);
    float gentleWeight = std::pow(1.0f - mountainStyleNoise, 1.4f);
    float weightSum = sharpWeight + gentleWeight + 1e-6f;
    sharpWeight /= weightSum;
    gentleWeight /= weightSum;
    
    // ========== 3. HILLS / EROSION NOISE ==========
    // Medium scale for rolling hills
    float hillX = x * HILLS_SCALE + offsetPVX;
    float hillZ = z * HILLS_SCALE + offsetPVZ;
    
    float hillNoise = fbm(hillX, hillZ, 4);
    // Turbulence adds more interesting hill shapes
    float hillTurb = turbulence(hillX * 1.5f, hillZ * 1.5f, 3);
    float hills = lerp(hillNoise, hillTurb * 2.0f - 1.0f, 0.3f);
    
    // ========== 4. DETAIL NOISE ==========
    // High frequency detail for local terrain variation
    float detX = x * DETAIL_SCALE + 5000.0f;
    float detZ = z * DETAIL_SCALE + 6000.0f;
    float detail = fbm(detX, detZ, 4);
    
    // ========== 5. COMBINE INTO FINAL HEIGHT ==========
    
    // Base height from continentalness (tuned for SEA_LEVEL = 32: more land, less overall ocean)
    float baseHeight;
    if (continentalness < -0.55f) {
        // Deep ocean
        float t = (continentalness + 1.0f) / 0.45f; // [-1..-0.55] -> [0..1]
        baseHeight = lerp((float)SEA_LEVEL - 26.0f, (float)SEA_LEVEL - 16.0f, std::clamp(t, 0.0f, 1.0f));
    } else if (continentalness < -0.25f) {
        // Ocean / shelf
        float t = (continentalness + 0.55f) / 0.30f; // [-0.55..-0.25]
        baseHeight = lerp((float)SEA_LEVEL - 16.0f, (float)SEA_LEVEL - 7.0f, std::clamp(t, 0.0f, 1.0f));
    } else if (continentalness < -0.05f) {
        // Coast transition
        float t = (continentalness + 0.25f) / 0.20f; // [-0.25..-0.05]
        baseHeight = lerp((float)SEA_LEVEL - 7.0f, (float)SEA_LEVEL + 1.5f, std::clamp(t, 0.0f, 1.0f));
    } else if (continentalness < 0.20f) {
        // Low plains
        float t = (continentalness + 0.05f) / 0.25f; // [-0.05..0.20]
        baseHeight = lerp((float)SEA_LEVEL + 1.5f, (float)SEA_LEVEL + 10.0f, std::clamp(t, 0.0f, 1.0f));
    } else if (continentalness < 0.55f) {
        // Inland plains
        float t = (continentalness - 0.20f) / 0.35f; // [0.20..0.55]
        baseHeight = lerp((float)SEA_LEVEL + 10.0f, (float)SEA_LEVEL + 19.0f, std::clamp(t, 0.0f, 1.0f));
    } else {
        // Highlands base
        float t = (continentalness - 0.55f) / 0.45f; // [0.55..1.0]
        baseHeight = lerp((float)SEA_LEVEL + 19.0f, (float)SEA_LEVEL + 27.0f, std::clamp(t, 0.0f, 1.0f));
    }

    // Ocean islands: allow land to pop up inside ocean regions
    if (continentalness < -0.30f) {
        float iX = x * 0.004f + offsetContinentX * 0.15f + 10000.0f;
        float iZ = z * 0.004f + offsetContinentZ * 0.15f + 20000.0f;
        float islandN = (fbm(iX, iZ, 4) + 1.0f) * 0.5f;
        float islandMask = std::pow(std::clamp((islandN - 0.72f) / 0.28f, 0.0f, 1.0f), 2.2f);
        baseHeight += islandMask * 30.0f;
    }
    
    // Only add terrain features on land
    float landFactor = std::clamp((continentalness + 0.10f) * 3.5f, 0.0f, 1.0f);
    
    // Hill contribution (moderate height variation)
    float hillHeight = hills * 8.0f * landFactor;
    
    // Mountain contribution (two styles: sharp ridges vs gentle, walkable slopes)
    // Reduce high-frequency peak noise a bit to avoid extremely sharp spires / "holey" peaks
    float peakDetail = ridgedMultifractal(detX * 2.2f, detZ * 2.2f, 4, 2.0f, 0.5f, 1.0f);
    float sharpHeight = mountainFactor * (62.0f + peakDetail * 85.0f + detail * 16.0f) * landFactor;
    
    float gentleShape = (billowNoise(mtWarpX * 1.1f, mtWarpZ * 1.1f) + 1.0f) * 0.5f; // softer, rounded peaks
    float gentleDetail = (fbm(mtWarpX * 0.7f, mtWarpZ * 0.7f, 3) + 1.0f) * 0.5f;
    float gentleHeight = mountainFactor * (42.0f + gentleShape * 60.0f + gentleDetail * 14.0f) * landFactor;
    
    float mountainHeight = sharpWeight * sharpHeight + gentleWeight * gentleHeight;
    
    // Add hills only where there are no mountains
    float finalHillHeight = hillHeight * (1.0f - mountainFactor * 0.8f);
    
    // Detail adds small variations everywhere
    float detailHeight = detail * 3.0f * landFactor;
    
    // Combine all layers
    float finalHeight = baseHeight + finalHillHeight + mountainHeight + detailHeight;

    // River carving: create river-like water channels instead of random inland water.
    float rX = x * 0.0035f + offsetPVX * 0.25f + 31000.0f;
    float rZ = z * 0.0035f + offsetPVZ * 0.25f + 42000.0f;
    float riverBase = fbm(rX, rZ, 3);
    float riverVal = 1.0f - std::abs(riverBase);
    float riverMask = std::pow(std::clamp((riverVal - 0.78f) / 0.22f, 0.0f, 1.0f), 2.6f);
    riverMask *= landFactor;
    // Don't let rivers carve huge gashes through mountain cores
    riverMask *= (1.0f - mountainFactor * 0.85f);

    // River bed should be below sea level so water fills properly
    // Use a fixed river bed depth and force terrain down when riverMask is strong
    float riverBed = (float)SEA_LEVEL - 3.0f;  // River beds at Y=29
    
    if (riverMask > 0.3f) {
        // Strong river influence - force terrain below water level
        // The stronger the riverMask, the deeper we carve
        float carveDepth = riverMask * 4.0f;  // Max 4 blocks below riverBed
        float targetHeight = riverBed - (riverMask - 0.3f) * 2.0f;
        finalHeight = std::min(finalHeight, targetHeight);
    } else if (riverMask > 0.0f) {
        // Gentle river influence - smooth transition to river banks
        finalHeight = lerp(finalHeight, riverBed + 2.0f, riverMask / 0.3f * 0.5f);
    }

    // Prevent random inland lakes: keep most land above sea level unless we're in a river.
    if (continentalness > 0.00f && riverMask < 0.15f) {
        finalHeight = std::max(finalHeight, (float)SEA_LEVEL + 3.0f);
    }
    
    // Ensure minimum heights
    finalHeight = std::max(finalHeight, 5.0f);
    
    // Apply height power curve for more dramatic peaks (but keep extremes rarer)
    if (finalHeight > 75.0f) {
        float excess = finalHeight - 75.0f;
        finalHeight = 75.0f + std::pow(excess / 120.0f, 0.85f) * 120.0f;
    }
    
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
