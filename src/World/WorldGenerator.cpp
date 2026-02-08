#include "WorldGenerator.h"
#include "../Util/Config.h"
#include "../Core/Logger.h"
#include "ChunkManager.h"
#include "StructurePlacer.h"
#include "Structure.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <utility>

// Static BiomeInfo cache - initialized once for fast lookups
BiomeInfo WorldGenerator::biomeInfoCache[14];
bool WorldGenerator::biomeInfoCacheInitialized = false;

static void initBiomeInfoCache(BiomeInfo* cache) {
    // OCEAN
    cache[0].type = BiomeType::OCEAN;
    cache[0].temperature = 0.5f; cache[0].humidity = 1.0f; cache[0].heightVariation = 0.3f;
    cache[0].surfaceBlock = BlockType::SAND; cache[0].subsurfaceBlock = BlockType::SAND; cache[0].surfaceDepth = 3;
    cache[0].grassColorR = 0.30f; cache[0].grassColorG = 0.65f; cache[0].grassColorB = 0.55f;
    cache[0].foliageColorR = 0.30f; cache[0].foliageColorG = 0.65f; cache[0].foliageColorB = 0.55f;
    cache[0].mapColorR = 0.15f; cache[0].mapColorG = 0.40f; cache[0].mapColorB = 0.80f;
    
    // RIVER
    cache[1].type = BiomeType::RIVER;
    cache[1].temperature = 0.5f; cache[1].humidity = 0.8f; cache[1].heightVariation = 0.15f;
    cache[1].surfaceBlock = BlockType::SAND; cache[1].subsurfaceBlock = BlockType::GRAVEL; cache[1].surfaceDepth = 3;
    cache[1].grassColorR = 0.55f; cache[1].grassColorG = 0.75f; cache[1].grassColorB = 0.45f;
    cache[1].foliageColorR = 0.45f; cache[1].foliageColorG = 0.70f; cache[1].foliageColorB = 0.40f;
    cache[1].mapColorR = 0.25f; cache[1].mapColorG = 0.50f; cache[1].mapColorB = 0.85f;
    
    // PLAINS
    cache[2].type = BiomeType::PLAINS;
    cache[2].temperature = 0.6f; cache[2].humidity = 0.5f; cache[2].heightVariation = 0.5f;
    cache[2].surfaceBlock = BlockType::GRASS; cache[2].subsurfaceBlock = BlockType::DIRT; cache[2].surfaceDepth = 4;
    cache[2].grassColorR = 0.57f; cache[2].grassColorG = 0.74f; cache[2].grassColorB = 0.35f;
    cache[2].foliageColorR = 0.47f; cache[2].foliageColorG = 0.68f; cache[2].foliageColorB = 0.32f;
    cache[2].mapColorR = 0.55f; cache[2].mapColorG = 0.75f; cache[2].mapColorB = 0.35f;
    
    // DESERT
    cache[3].type = BiomeType::DESERT;
    cache[3].temperature = 0.9f; cache[3].humidity = 0.1f; cache[3].heightVariation = 0.4f;
    cache[3].surfaceBlock = BlockType::SAND; cache[3].subsurfaceBlock = BlockType::SANDSTONE; cache[3].surfaceDepth = 5;
    cache[3].grassColorR = 0.75f; cache[3].grassColorG = 0.72f; cache[3].grassColorB = 0.42f;
    cache[3].foliageColorR = 0.68f; cache[3].foliageColorG = 0.65f; cache[3].foliageColorB = 0.38f;
    cache[3].mapColorR = 0.86f; cache[3].mapColorG = 0.78f; cache[3].mapColorB = 0.55f;
    
    // FOREST
    cache[4].type = BiomeType::FOREST;
    cache[4].temperature = 0.5f; cache[4].humidity = 0.8f; cache[4].heightVariation = 0.6f;
    cache[4].surfaceBlock = BlockType::GRASS; cache[4].subsurfaceBlock = BlockType::DIRT; cache[4].surfaceDepth = 4;
    cache[4].grassColorR = 0.35f; cache[4].grassColorG = 0.60f; cache[4].grassColorB = 0.28f;
    cache[4].foliageColorR = 0.30f; cache[4].foliageColorG = 0.55f; cache[4].foliageColorB = 0.25f;
    cache[4].mapColorR = 0.20f; cache[4].mapColorG = 0.50f; cache[4].mapColorB = 0.20f;
    
    // BIRCH_FOREST
    cache[5].type = BiomeType::BIRCH_FOREST;
    cache[5].temperature = 0.45f; cache[5].humidity = 0.7f; cache[5].heightVariation = 0.5f;
    cache[5].surfaceBlock = BlockType::GRASS; cache[5].subsurfaceBlock = BlockType::DIRT; cache[5].surfaceDepth = 4;
    cache[5].grassColorR = 0.52f; cache[5].grassColorG = 0.72f; cache[5].grassColorB = 0.38f;
    cache[5].foliageColorR = 0.48f; cache[5].foliageColorG = 0.68f; cache[5].foliageColorB = 0.35f;
    cache[5].mapColorR = 0.45f; cache[5].mapColorG = 0.68f; cache[5].mapColorB = 0.38f;
    
    // TAIGA
    cache[6].type = BiomeType::TAIGA;
    cache[6].temperature = 0.2f; cache[6].humidity = 0.6f; cache[6].heightVariation = 0.6f;
    cache[6].surfaceBlock = BlockType::GRASS; cache[6].subsurfaceBlock = BlockType::DIRT; cache[6].surfaceDepth = 4;
    cache[6].grassColorR = 0.45f; cache[6].grassColorG = 0.60f; cache[6].grassColorB = 0.45f;
    cache[6].foliageColorR = 0.38f; cache[6].foliageColorG = 0.55f; cache[6].foliageColorB = 0.40f;
    cache[6].mapColorR = 0.35f; cache[6].mapColorG = 0.55f; cache[6].mapColorB = 0.40f;
    
    // JUNGLE
    cache[7].type = BiomeType::JUNGLE;
    cache[7].temperature = 0.85f; cache[7].humidity = 0.95f; cache[7].heightVariation = 0.8f;
    cache[7].surfaceBlock = BlockType::GRASS; cache[7].subsurfaceBlock = BlockType::DIRT; cache[7].surfaceDepth = 5;
    cache[7].grassColorR = 0.35f; cache[7].grassColorG = 0.78f; cache[7].grassColorB = 0.22f;
    cache[7].foliageColorR = 0.30f; cache[7].foliageColorG = 0.75f; cache[7].foliageColorB = 0.18f;
    cache[7].mapColorR = 0.25f; cache[7].mapColorG = 0.65f; cache[7].mapColorB = 0.15f;
    
    // SWAMP
    cache[8].type = BiomeType::SWAMP;
    cache[8].temperature = 0.6f; cache[8].humidity = 0.9f; cache[8].heightVariation = 0.2f;
    cache[8].surfaceBlock = BlockType::GRASS; cache[8].subsurfaceBlock = BlockType::DIRT; cache[8].surfaceDepth = 4;
    cache[8].grassColorR = 0.42f; cache[8].grassColorG = 0.52f; cache[8].grassColorB = 0.32f;
    cache[8].foliageColorR = 0.40f; cache[8].foliageColorG = 0.48f; cache[8].foliageColorB = 0.30f;
    cache[8].mapColorR = 0.38f; cache[8].mapColorG = 0.48f; cache[8].mapColorB = 0.30f;
    
    // MOUNTAINS
    cache[9].type = BiomeType::MOUNTAINS;
    cache[9].temperature = 0.3f; cache[9].humidity = 0.4f; cache[9].heightVariation = 1.5f;
    cache[9].surfaceBlock = BlockType::GRASS; cache[9].subsurfaceBlock = BlockType::DIRT; cache[9].surfaceDepth = 3;
    cache[9].grassColorR = 0.50f; cache[9].grassColorG = 0.62f; cache[9].grassColorB = 0.42f;
    cache[9].foliageColorR = 0.45f; cache[9].foliageColorG = 0.58f; cache[9].foliageColorB = 0.38f;
    cache[9].mapColorR = 0.50f; cache[9].mapColorG = 0.50f; cache[9].mapColorB = 0.55f;
    
    // SNOWY_TUNDRA
    cache[10].type = BiomeType::SNOWY_TUNDRA;
    cache[10].temperature = 0.0f; cache[10].humidity = 0.3f; cache[10].heightVariation = 0.4f;
    cache[10].surfaceBlock = BlockType::SNOW; cache[10].subsurfaceBlock = BlockType::DIRT; cache[10].surfaceDepth = 3;
    cache[10].grassColorR = 0.50f; cache[10].grassColorG = 0.65f; cache[10].grassColorB = 0.55f;
    cache[10].foliageColorR = 0.42f; cache[10].foliageColorG = 0.58f; cache[10].foliageColorB = 0.48f;
    cache[10].mapColorR = 0.86f; cache[10].mapColorG = 0.90f; cache[10].mapColorB = 0.94f;
    
    // SAVANNA
    cache[11].type = BiomeType::SAVANNA;
    cache[11].temperature = 0.85f; cache[11].humidity = 0.3f; cache[11].heightVariation = 0.4f;
    cache[11].surfaceBlock = BlockType::GRASS; cache[11].subsurfaceBlock = BlockType::DIRT; cache[11].surfaceDepth = 4;
    cache[11].grassColorR = 0.72f; cache[11].grassColorG = 0.72f; cache[11].grassColorB = 0.35f;
    cache[11].foliageColorR = 0.68f; cache[11].foliageColorG = 0.68f; cache[11].foliageColorB = 0.32f;
    cache[11].mapColorR = 0.70f; cache[11].mapColorG = 0.70f; cache[11].mapColorB = 0.40f;
    
    // VILLAGE - flat plains with village structures
    cache[12].type = BiomeType::VILLAGE;
    cache[12].temperature = 0.6f; cache[12].humidity = 0.5f; cache[12].heightVariation = 0.15f;
    cache[12].surfaceBlock = BlockType::GRASS; cache[12].subsurfaceBlock = BlockType::DIRT; cache[12].surfaceDepth = 4;
    cache[12].grassColorR = 0.55f; cache[12].grassColorG = 0.72f; cache[12].grassColorB = 0.38f;
    cache[12].foliageColorR = 0.50f; cache[12].foliageColorG = 0.68f; cache[12].foliageColorB = 0.35f;
    cache[12].mapColorR = 0.70f; cache[12].mapColorG = 0.55f; cache[12].mapColorB = 0.35f;  // Warm brown
    
    // CITY - flat area with city buildings (grass/dirt base - roads/structures placed in generate())
    cache[13].type = BiomeType::CITY;
    cache[13].temperature = 0.5f; cache[13].humidity = 0.4f; cache[13].heightVariation = 0.1f;
    cache[13].surfaceBlock = BlockType::GRASS; cache[13].subsurfaceBlock = BlockType::DIRT; cache[13].surfaceDepth = 4;
    cache[13].grassColorR = 0.45f; cache[13].grassColorG = 0.50f; cache[13].grassColorB = 0.40f;
    cache[13].foliageColorR = 0.40f; cache[13].foliageColorG = 0.45f; cache[13].foliageColorB = 0.38f;
    cache[13].mapColorR = 0.60f; cache[13].mapColorG = 0.60f; cache[13].mapColorB = 0.65f;  // Gray stone
}

WorldGenerator::WorldGenerator(unsigned int seed) : seed(seed), structurePlacer(std::make_unique<StructurePlacer>(seed)) {
    if (!biomeInfoCacheInitialized) {
        initBiomeInfoCache(biomeInfoCache);
        biomeInfoCacheInitialized = true;
    }
    setSeed(seed);
}

WorldGenerator::~WorldGenerator() = default;

void WorldGenerator::initializeStructures(const std::string& structuresPath) {
    structurePlacer->initialize(structuresPath);
    LOG_INFO("WorldGenerator: Structures initialized from " + structuresPath);
}

void WorldGenerator::setSeed(unsigned int s) {
    seed = s;
    structurePlacer->setSeed(s);
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
    // Fast array lookup using biome enum value as index
    return biomeInfoCache[static_cast<int>(biome)];
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

float WorldGenerator::getRiverMask(float x, float z) const {
    // Calculate river strength at this position [0, 1]
    float rX = x * 0.0035f + offsetPVX * 0.25f + 31000.0f;
    float rZ = z * 0.0035f + offsetPVZ * 0.25f + 42000.0f;
    float riverBase = fbm(rX, rZ, 3);
    float riverVal = 1.0f - std::abs(riverBase);
    return std::pow(std::clamp((riverVal - 0.78f) / 0.22f, 0.0f, 1.0f), 2.6f);
}

float WorldGenerator::getMountainFactor(float x, float z) const {
    // Calculate mountain factor at this position [0, 1]
    const float MOUNTAIN_SCALE = 0.002f * globalFrequencyBias;
    float mtX = x * MOUNTAIN_SCALE + offsetErosionX;
    float mtZ = z * MOUNTAIN_SCALE + offsetErosionZ;
    float mtWarpX = mtX + 0.5f * noise2D(mtX * 0.5f + 1000.0f, mtZ * 0.5f + 2000.0f);
    float mtWarpZ = mtZ + 0.5f * noise2D(mtX * 0.5f + 3000.0f, mtZ * 0.5f + 4000.0f);
    float mountainNoise = ridgedMultifractal(mtWarpX, mtWarpZ, 5, 2.2f, 0.6f, 1.0f);
    return std::clamp((mountainNoise - 0.40f) * 3.1f, 0.0f, 1.0f);
}

bool WorldGenerator::isUndergroundRiver(float x, float y, float z, float riverMask, float mountainFactor) const {
    // Underground river caves - where rivers meet mountains, they go underground
    // This creates natural cave river systems that follow the same path as surface rivers
    
    // Only create underground rivers where there's significant river AND mountain overlap
    float undergroundRiverStrength = riverMask * mountainFactor;
    if (undergroundRiverStrength < 0.15f) return false;
    
    // River tunnel should be around sea level (water level)
    float riverBedY = static_cast<float>(SEA_LEVEL) - 3.0f;
    float distFromRiverBed = y - riverBedY;  // Can be negative (below bed) or positive (above)
    
    // Tunnel shape - taller above water for air space, shorter below for riverbed
    float tunnelHeightAbove = 5.0f + undergroundRiverStrength * 3.0f;  // 5-8 blocks above bed
    float tunnelHeightBelow = 2.0f;  // 2 blocks below bed for gravel floor
    
    // Vertical check - are we within the tunnel?
    if (distFromRiverBed > tunnelHeightAbove || distFromRiverBed < -tunnelHeightBelow) return false;
    
    // Use river mask directly - this follows the exact same river path as surface rivers
    // riverMask is already calculated from the same noise, so underground river aligns perfectly
    float tunnelWidth = 0.5f + undergroundRiverStrength * 0.3f;  // Threshold for being in tunnel
    
    if (riverMask < tunnelWidth) return false;  // Not in river path
    
    // Add some waviness to walls but keep core path stable
    float wallWobble = noise2D(x * 0.06f + 8000.0f, z * 0.06f + 9000.0f) * 0.08f;
    
    // Smooth tunnel cross-section
    float normalizedY = distFromRiverBed / (distFromRiverBed > 0 ? tunnelHeightAbove : tunnelHeightBelow);
    float normalizedWidth = (tunnelWidth + wallWobble - riverMask) / 0.2f;  // How close to edge
    
    // Elliptical cross-section
    float tunnelShape = normalizedWidth * normalizedWidth + normalizedY * normalizedY;
    
    if (tunnelShape < 0.8f) return true;
    if (tunnelShape < 1.0f) {
        // Noisy edges for natural look
        float edgeNoise = noise3D(x * 0.12f + 500.0f, y * 0.18f + 600.0f, z * 0.12f + 700.0f);
        return edgeNoise > (tunnelShape - 0.8f) * 3.5f;
    }
    
    return false;
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
    
    // ========== SETTLEMENT GENERATION ==========
    // Settlements spawn on a jittered grid for natural-looking placement.
    // Each grid cell gets a deterministic random offset so settlements aren't in a perfect grid.
    // Some grid points are skipped based on noise for organic distribution.
    
    const float VILLAGE_GRID = 350.0f;  // Average village spacing (larger = fewer villages)
    const float CITY_GRID = 500.0f;     // Average city spacing  
    const float VILLAGE_RADIUS = 45.0f; // Village biome radius
    const float CITY_RADIUS = 160.0f;   // City biome radius
    const float CITY_JITTER = 120.0f;   // Max offset from grid center for cities
    const float VILLAGE_JITTER = 80.0f; // Max offset from grid center for villages
    
    // Helper lambda: deterministic jitter for a grid cell
    auto settlementJitter = [&](float gridCX, float gridCZ, float jitterAmount, float gridSize) -> std::pair<float, float> {
        // Hash the grid cell to get deterministic random offsets
        unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
        unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
        unsigned int h1 = (seed ^ hx ^ hz) * 2654435761u;
        unsigned int h2 = (seed ^ hz ^ (hx * 2246822519u)) * 3266489917u;
        float offsetX = ((h1 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
        float offsetZ = ((h2 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
        return {gridCX + offsetX, gridCZ + offsetZ};
    };
    
    // Helper lambda: should this grid cell spawn a settlement?
    auto shouldSpawnSettlement = [&](float gridCX, float gridCZ, float gridSize, float spawnChance) -> bool {
        unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
        unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
        unsigned int h = (seed ^ hx ^ hz ^ 987654321u) * 2246822519u;
        float roll = (h & 0xFFFF) / 65536.0f;
        return roll < spawnChance;
    };
    
    // City check first (takes priority)
    float cityGridBaseX = std::floor(x / CITY_GRID) * CITY_GRID + CITY_GRID / 2.0f;
    float cityGridBaseZ = std::floor(z / CITY_GRID) * CITY_GRID + CITY_GRID / 2.0f;
    auto [cityCenterX, cityCenterZ] = settlementJitter(cityGridBaseX, cityGridBaseZ, CITY_JITTER, CITY_GRID);
    float cityDist = std::sqrt((x - cityCenterX) * (x - cityCenterX) + (z - cityCenterZ) * (z - cityCenterZ));
    
    // Cities: ~70% of grid points spawn, avoid mountains
    if (cityDist < CITY_RADIUS && shouldSpawnSettlement(cityGridBaseX, cityGridBaseZ, CITY_GRID, 0.70f)) {
        // Check mountain factor at center AND cardinal edge points - cities need flat terrain everywhere
        float cityMtCenter = getMountainFactor(cityCenterX, cityCenterZ);
        float edgeCheckR = CITY_RADIUS * 0.7f;
        float cityMtEdge = std::max({
            getMountainFactor(cityCenterX + edgeCheckR, cityCenterZ),
            getMountainFactor(cityCenterX - edgeCheckR, cityCenterZ),
            getMountainFactor(cityCenterX, cityCenterZ + edgeCheckR),
            getMountainFactor(cityCenterX, cityCenterZ - edgeCheckR)
        });
        if (cityMtCenter < 0.15f && cityMtEdge < 0.30f) {
            return BiomeType::CITY;
        }
    }
    
    // Village check (more common, smaller)
    float villageGridBaseX = std::floor(x / VILLAGE_GRID) * VILLAGE_GRID + VILLAGE_GRID / 2.0f;
    float villageGridBaseZ = std::floor(z / VILLAGE_GRID) * VILLAGE_GRID + VILLAGE_GRID / 2.0f;
    auto [villageCenterX, villageCenterZ] = settlementJitter(villageGridBaseX, villageGridBaseZ, VILLAGE_JITTER, VILLAGE_GRID);
    float villageDist = std::sqrt((x - villageCenterX) * (x - villageCenterX) + (z - villageCenterZ) * (z - villageCenterZ));
    
    // Villages: ~45% of grid points spawn, skip if overlapping a city, avoid deep ocean
    if (villageDist < VILLAGE_RADIUS && cityDist > CITY_RADIUS + 10.0f &&
        shouldSpawnSettlement(villageGridBaseX, villageGridBaseZ, VILLAGE_GRID, 0.45f)) {
        float villageMtFactor = getMountainFactor(villageCenterX, villageCenterZ);
        if (villageMtFactor < 0.35f) {
            return BiomeType::VILLAGE;
        }
    }
    
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
    
    // Check for underground river first (rivers entering mountains)
    float riverMask = getRiverMask(x, z);
    float mountainFactor = getMountainFactor(x, z);
    if (isUndergroundRiver(x, y, z, riverMask, mountainFactor)) {
        return true;  // Underground river carves through here
    }
    
    // Don't carve regular caves through underwater areas (oceans/surface rivers)
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

// Optimized isCave overload that accepts precomputed surface height
bool WorldGenerator::isCave(float x, float y, float z, int surfaceHeight) const {
    // Extended cave system - deeper and more natural
    
    // Caves can go much deeper now (down to y=0, but not in bedrock)
    if (y < 2) return false;
    
    // Check for underground river first (rivers entering mountains)
    float riverMask = getRiverMask(x, z);
    float mountainFactor = getMountainFactor(x, z);
    if (isUndergroundRiver(x, y, z, riverMask, mountainFactor)) {
        return true;  // Underground river carves through here
    }
    
    // Don't carve regular caves through underwater areas (oceans/surface rivers)
    if (surfaceHeight < SEA_LEVEL && y < SEA_LEVEL) {
        return false;
    }
    
    // CRITICAL: Don't carve caves too close to surface to prevent ugly holes
    int depthBelowSurface = surfaceHeight - static_cast<int>(y);
    if (depthBelowSurface < 5) {
        return false;
    }
    
    // Natural cave entrance zones - only allow near-surface caves where terrain dips
    if (depthBelowSurface < 12) {
        float entranceNoise = noise2D(x * 0.02f + 5000.0f, z * 0.02f + 6000.0f);
        if (entranceNoise < 0.3f) {
            return false;
        }
        float heightNearby = getHeight(x + 4.0f, z) + getHeight(x - 4.0f, z) + 
                            getHeight(x, z + 4.0f) + getHeight(x, z - 4.0f);
        float avgHeight = heightNearby / 4.0f;
        float slope = std::abs(avgHeight - surfaceHeight);
        if (slope < 3.0f) {
            return false;
        }
    }
    
    // Depth factor - caves get larger and more common deeper down
    float depth = static_cast<float>(SEA_LEVEL) - y;
    float depthFactor = std::clamp(depth / 80.0f, 0.0f, 1.0f);
    
    // 1. Cheese Caves (Large Rooms)
    float cheese = noise3D(x * 0.010f, y * 0.010f, z * 0.010f);
    float cheeseThreshold = -0.50f + globalCaveDensityBias - depthFactor * 0.1f;
    bool isRoom = (cheese < cheeseThreshold);
    
    // 2. Spaghetti Caves (Tunnels)
    float worm1 = noise3D(x * 0.015f + 123.4f, y * 0.020f + 521.2f, z * 0.015f + 921.1f);
    float worm2 = noise3D(x * 0.015f + 921.4f, y * 0.020f + 123.2f, z * 0.015f + 521.1f);
    float tunnelWidth = 0.045f + depthFactor * 0.05f;
    bool isTunnel = (std::abs(worm1) < tunnelWidth && std::abs(worm2) < tunnelWidth);
    
    // 3. Noodle Caves (Thin, snaking passages)
    float noodle1 = noise3D(x * 0.025f + 333.0f, y * 0.030f + 444.0f, z * 0.025f + 555.0f);
    float noodle2 = noise3D(x * 0.025f + 666.0f, y * 0.030f + 777.0f, z * 0.025f + 888.0f);
    float noodleWidth = 0.03f + depthFactor * 0.02f;
    bool isNoodle = (std::abs(noodle1) < noodleWidth && std::abs(noodle2) < noodleWidth);
    
    // 4. Deep caverns
    bool isDeepCavern = false;
    if (y < 0) {
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
        case BiomeType::VILLAGE:       treeProb = 0.0f;   break; // No trees in settlements
        case BiomeType::CITY:          treeProb = 0.0f;   break; // No trees in settlements
        default:                       treeProb = 0.0f;   break; // No trees
    }
    
    if (r >= treeProb) return false;

    // 2. Settlement proximity check: suppress trees near village/city borders
    // This prevents tree canopies from bleeding into settlements
    {
        int checkRadius = 5; // tree canopy can extend ~3-4 blocks
        for (int dx = -checkRadius; dx <= checkRadius; dx += checkRadius) {
            for (int dz = -checkRadius; dz <= checkRadius; dz += checkRadius) {
                BiomeType nearBiome = getBiome(static_cast<float>(x + dx), static_cast<float>(z + dz));
                if (nearBiome == BiomeType::VILLAGE || nearBiome == BiomeType::CITY) {
                    return false;
                }
            }
        }
    }

    // 3. Spatial check: Suppress if a "better" candidate is nearby
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
    
    // Precompute per-column data to avoid redundant calculations
    // This eliminates repeated noise computations for height, biome, and temperature
    struct ColumnData {
        int height;
        BiomeType biome;
        const BiomeInfo* biomeInfo;
        float temp;
        float riverMask;      // River strength for this column
        float riverBankMask;  // Extended river bank influence
        float mountainFactor; // Mountain factor for underground rivers
    };
    ColumnData columnCache[CHUNK_SIZE][CHUNK_SIZE];
    
    // First pass: precompute all column data
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = static_cast<int>(worldPos.x) + x;
            int worldZ = static_cast<int>(worldPos.z) + z;
            float worldXf = static_cast<float>(worldX);
            float worldZf = static_cast<float>(worldZ);
            
            columnCache[x][z].height = static_cast<int>(getHeight(worldXf, worldZf));
            columnCache[x][z].biome = getBiome(worldXf, worldZf);
            columnCache[x][z].biomeInfo = &biomeInfoCache[static_cast<int>(columnCache[x][z].biome)];
            columnCache[x][z].temp = getTemperature(worldXf, worldZf);
            columnCache[x][z].riverMask = getRiverMask(worldXf, worldZf);
            columnCache[x][z].mountainFactor = getMountainFactor(worldXf, worldZf);
            
            // Calculate extended river bank mask (same calculation as in getHeight)
            float rX = worldXf * 0.0035f + offsetPVX * 0.25f + 31000.0f;
            float rZ = worldZf * 0.0035f + offsetPVZ * 0.25f + 42000.0f;
            float riverBase = fbm(rX, rZ, 3);
            float riverVal = 1.0f - std::abs(riverBase);
            
            // Continentalness for land factor
            const float CONTINENT_SCALE = 0.0008f * globalFrequencyBias;
            float contX = worldXf * CONTINENT_SCALE + offsetContinentX;
            float contZ = worldZf * CONTINENT_SCALE + offsetContinentZ;
            float warpX = contX, warpZ = contZ;
            domainWarp(warpX, warpZ);
            float continentalness = fbm(warpX, warpZ, 4);
            continentalness = std::clamp(continentalness + 0.20f, -1.0f, 1.0f);
            float landFactor = std::clamp((continentalness + 0.10f) * 3.5f, 0.0f, 1.0f);
            
            float riverBankMask = std::pow(std::clamp((riverVal - 0.70f) / 0.30f, 0.0f, 1.0f), 1.5f);
            columnCache[x][z].riverBankMask = riverBankMask * landFactor * (1.0f - columnCache[x][z].mountainFactor * 0.95f);
        }
    }
    
    // 1. Terrain Pass - now using precomputed data
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = static_cast<int>(worldPos.x) + x;
            int worldZ = static_cast<int>(worldPos.z) + z;
            float worldXf = static_cast<float>(worldX);
            float worldZf = static_cast<float>(worldZ);
            
            // Use precomputed column data
            const ColumnData& col = columnCache[x][z];
            BiomeType biome = col.biome;
            const BiomeInfo& biomeInfo = *col.biomeInfo;
            float temp = col.temp;
            int height = col.height;
            float riverMask = col.riverMask;
            float mountainFactor = col.mountainFactor;
            float undergroundRiverStrength = riverMask * mountainFactor;
            
            // River shore detection for sand/gravel placement
            // When mountainFactor >= 0.3, river goes underground - no surface river features
            float surfaceRiverMask = 0.0f;
            if (mountainFactor < 0.3f) {
                float mountainSuppression = mountainFactor / 0.3f;
                surfaceRiverMask = riverMask * (1.0f - mountainSuppression);
            }
            bool isRiverCenter = surfaceRiverMask > 0.5f;      // Underwater river bed
            bool isRiverShore = surfaceRiverMask > 0.4f && surfaceRiverMask <= 0.5f;  // Shore at water edge
            
            // Sand only appears if terrain is close to water level (within 3 blocks above SEA_LEVEL)
            // This prevents sand on top of cliffs where river goes underground
            bool isNearWaterLevel = (height <= SEA_LEVEL + 3);
            bool shouldHaveSand = isRiverShore && isNearWaterLevel;
            
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                int worldY = static_cast<int>(worldPos.y) + y;
                BlockType blockType = BlockType::AIR;
                
                // Use optimized isCave overload with precomputed surface height
                bool isInCave = isCave(worldXf, static_cast<float>(worldY), worldZf, height);
                
                // Check if this is an underground river cave - creates its own tunnel
                // regardless of natural cave system
                bool isUndergroundRiverCave = false;
                if (undergroundRiverStrength >= 0.15f && worldY < height - 2) {
                    // Only check underground river in the underground (below surface)
                    isUndergroundRiverCave = isUndergroundRiver(worldXf, static_cast<float>(worldY), worldZf, riverMask, mountainFactor);
                }
                
                // Bedrock Layer at Y = -64
                if (worldY <= -64) {
                    blockType = BlockType::BEDROCK;
                    isInCave = false; // No caves in bedrock
                } else if (isUndergroundRiverCave) {
                    // Underground river - either water or air depending on height
                    if (worldY < SEA_LEVEL) {
                        blockType = BlockType::WATER;
                    } else {
                        blockType = BlockType::AIR;  // Air above water in river cave
                    }
                } else if (!isInCave) {
                    if (worldY < height - biomeInfo.surfaceDepth) blockType = BlockType::STONE;
                    else if (worldY < height - 1) {
                        // Subsurface layer: gravel only for underwater river bed, normal otherwise
                        if (isRiverCenter) {
                            blockType = BlockType::GRAVEL;
                        } else {
                            blockType = biomeInfo.subsurfaceBlock;
                        }
                    } else if (worldY < height) {
                        // Surface layer: sand for river shores, gravel for underwater river bed
                        if (isRiverCenter) {
                            // Underwater river bed - gravel
                            blockType = BlockType::GRAVEL;
                        } else if (shouldHaveSand) {
                            // Sandy shore - only 1 block wide, only near water level
                            blockType = BlockType::SAND;
                        } else {
                            blockType = biomeInfo.surfaceBlock;
                        }

                        // MOUNTAINS: transition from grass to stone at higher elevations
                        // Below ~SEA_LEVEL+30: grass/dirt (tree zone)
                        // Above ~SEA_LEVEL+30: stone (barren peaks)
                        if (biome == BiomeType::MOUNTAINS && !isRiverCenter && !shouldHaveSand) {
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
                        // Fill water to sea level - this covers oceans AND rivers
                        // Rivers are carved below SEA_LEVEL, so this fills them automatically
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
    // Use precomputed column cache where possible to avoid redundant getBiome/getHeight calls
    int pad = 3; // Increased for larger jungle trees
    for (int nx = -pad; nx < CHUNK_SIZE + pad; ++nx) {
        for (int nz = -pad; nz < CHUNK_SIZE + pad; ++nz) {
            int worldX = static_cast<int>(worldPos.x) + nx;
            int worldZ = static_cast<int>(worldPos.z) + nz;
            float worldXf = static_cast<float>(worldX);
            float worldZf = static_cast<float>(worldZ);
            
            // EARLY REJECTION: Check if this location is in a river (no trees in rivers)
            float riverMaskCheck = getRiverMask(worldXf, worldZf);
            float mountainFactorCheck = getMountainFactor(worldXf, worldZf);
            float surfaceRiverCheck = riverMaskCheck * (1.0f - mountainFactorCheck * 0.95f);
            if (surfaceRiverCheck > 0.35f) continue;  // Skip river and shore areas entirely
            
            // Use precomputed data if within chunk bounds, otherwise compute
            BiomeType biome;
            int treeBaseY;
            float temp;
            bool inChunkBounds = (nx >= 0 && nx < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE);
            
            if (inChunkBounds) {
                const ColumnData& col = columnCache[nx][nz];
                biome = col.biome;
                treeBaseY = col.height;
                temp = col.temp;
            } else {
                biome = getBiome(worldXf, worldZf);
                treeBaseY = static_cast<int>(getHeight(worldXf, worldZf));
                temp = getTemperature(worldXf, worldZf);
            }
            
            // EARLY REJECTION: Below sea level or in cave - skip before hasTree check
            if (treeBaseY < SEA_LEVEL) continue;
            if (isCave(worldXf, static_cast<float>(treeBaseY - 1), worldZf, treeBaseY)) continue;
            
            if (hasTree(worldX, worldZ, biome)) {
                // Removed duplicate checks - already done above

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
                    const BiomeInfo& info = biomeInfoCache[static_cast<int>(biome)];
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
                
                // Check if this is snowy biome - we'll add snow to leaves (use precomputed temp)
                bool addSnowToLeaves = (biome == BiomeType::SNOWY_TUNDRA || 
                                       (biome == BiomeType::TAIGA && temp < 0.25f));
                
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
    
    // 4. STRUCTURE PASS - Place .vxstruct structures in settlement biomes
    // Uses a sub-grid system: cities place structures every 8 blocks (up to 4 per chunk),
    // villages every 16 blocks (1 per chunk). Roads in cities form a grid pattern.
    [&]() {
        int chunkBaseX = static_cast<int>(worldPos.x);
        int chunkBaseZ = static_cast<int>(worldPos.z);
        int chunkBaseY = chunkPos.y * CHUNK_HEIGHT;
        
        // Check if this XZ area is a settlement biome
        BiomeType centerBiome = getBiome(static_cast<float>(chunkBaseX + CHUNK_SIZE/2), 
                                          static_cast<float>(chunkBaseZ + CHUNK_SIZE/2));
        
        if (centerBiome != BiomeType::VILLAGE && centerBiome != BiomeType::CITY) return;
        
        bool isCity = (centerBiome == BiomeType::CITY);
        auto& registry = StructureRegistry::instance();
        auto allStructures = registry.getAllStructureIds();
        
        // Sub-grid spacing: cities 6 blocks (many per chunk), villages 16 blocks (1 per chunk)
        int structSpacing = isCity ? 6 : 16;
        // City block size for road grid (must be consistent between road and structure code)
        int cityBlockSize = 24;  // Bigger city blocks = more room for buildings
        
        // Find the settlement center for this biome (from jittered grid logic in getBiome)
        // Must match getBiome exactly!
        auto settlementJitterG = [&](float gridCX, float gridCZ, float jitterAmount, float gridSize) -> std::pair<float, float> {
            unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
            unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
            unsigned int h1 = (seed ^ hx ^ hz) * 2654435761u;
            unsigned int h2 = (seed ^ hz ^ (hx * 2246822519u)) * 3266489917u;
            float offsetX = ((h1 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
            float offsetZ = ((h2 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
            return {gridCX + offsetX, gridCZ + offsetZ};
        };
        
        float settlementCenterX, settlementCenterZ;
        if (isCity) {
            float cityGridBaseX = std::floor(static_cast<float>(chunkBaseX + 8) / 500.0f) * 500.0f + 250.0f;
            float cityGridBaseZ = std::floor(static_cast<float>(chunkBaseZ + 8) / 500.0f) * 500.0f + 250.0f;
            auto [cx, cz] = settlementJitterG(cityGridBaseX, cityGridBaseZ, 120.0f, 500.0f);
            settlementCenterX = cx;
            settlementCenterZ = cz;
        } else {
            float villageGridBaseX = std::floor(static_cast<float>(chunkBaseX + 8) / 350.0f) * 350.0f + 175.0f;
            float villageGridBaseZ = std::floor(static_cast<float>(chunkBaseZ + 8) / 350.0f) * 350.0f + 175.0f;
            auto [vx, vz] = settlementJitterG(villageGridBaseX, villageGridBaseZ, 80.0f, 350.0f);
            settlementCenterX = vx;
            settlementCenterZ = vz;
        }
        
        // ======== UNIFIED CITY Y LEVEL ========
        // One flat height for the entire city: roads, buildings, ground - all at the same level.
        // Rivers are preserved and get bridges where roads cross them.
        int cityY = getSurfaceHeight(static_cast<int>(settlementCenterX), static_cast<int>(settlementCenterZ));
        float cityRadius = isCity ? 160.0f : 45.0f;
        float edgeBlend = isCity ? 60.0f : 30.0f; // wide gradual transition at settlement edge
        
        // ---- PHASE 1: TERRAIN FLATTENING ----
        // Flatten the settlement area to cityY with grass/dirt, skip rivers.
        // Applied to BOTH cities and villages so roads sit flush with ground.
        {
            for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                    int worldFX = chunkBaseX + lx;
                    int worldFZ = chunkBaseZ + lz;
                    
                    // Distance from settlement center for edge blending
                    float dx = static_cast<float>(worldFX) - settlementCenterX;
                    float dz = static_cast<float>(worldFZ) - settlementCenterZ;
                    float dist = std::sqrt(dx * dx + dz * dz);
                    
                    if (dist > cityRadius + edgeBlend) continue; // Outside settlement
                    
                    // Check for rivers - preserve them! (rivers get bridges at roads)
                    float riverMask = getRiverMask(static_cast<float>(worldFX), static_cast<float>(worldFZ));
                    if (riverMask > 0.25f) continue; // This is a river area, skip flattening
                    
                    int naturalY = getSurfaceHeight(worldFX, worldFZ);
                    
                    // Blend factor: 1.0 inside settlement, fades to 0.0 at edge
                    float blend = 1.0f;
                    if (dist > cityRadius - edgeBlend) {
                        float t = std::clamp((cityRadius + edgeBlend - dist) / (edgeBlend * 2.0f), 0.0f, 1.0f);
                        blend = t * t * (3.0f - 2.0f * t); // smoothstep
                    }
                    
                    // Target Y: blend between cityY and natural height at edges
                    int targetY = static_cast<int>(std::round(
                        blend * static_cast<float>(cityY) + (1.0f - blend) * static_cast<float>(naturalY)));
                    
                    // Fill up to targetY with dirt, place grass on top
                    if (naturalY < targetY) {
                        for (int fy = naturalY + 1; fy <= targetY; fy++) {
                            int localFY = fy - chunkBaseY;
                            if (localFY < 0 || localFY >= CHUNK_HEIGHT) continue;
                            chunk->setBlock(lx, localFY, lz, Block(fy == targetY ? BlockType::GRASS : BlockType::DIRT));
                        }
                    } else if (naturalY > targetY) {
                        for (int fy = targetY + 1; fy <= naturalY; fy++) {
                            int localFY = fy - chunkBaseY;
                            if (localFY < 0 || localFY >= CHUNK_HEIGHT) continue;
                            chunk->setBlock(lx, localFY, lz, Block(BlockType::AIR));
                        }
                    }
                    // Ensure grass block on top surface
                    int localTY = targetY - chunkBaseY;
                    if (localTY >= 0 && localTY < CHUNK_HEIGHT) {
                        chunk->setBlock(lx, localTY, lz, Block(BlockType::GRASS));
                    }
                    // Ensure dirt layers below the grass
                    for (int d = 1; d <= 4; d++) {
                        int localDY = targetY - d - chunkBaseY;
                        if (localDY >= 0 && localDY < CHUNK_HEIGHT) {
                            Block below = chunk->getBlock(lx, localDY, lz);
                            BlockType bt = below.getType();
                            if (bt == BlockType::STONE || bt == BlockType::COBBLESTONE || bt == BlockType::AIR) {
                                chunk->setBlock(lx, localDY, lz, Block(BlockType::DIRT));
                            }
                        }
                    }
                    
                    // Clear any trees/vegetation above for 8 blocks
                    for (int clearY = 1; clearY <= 8; clearY++) {
                        int localCY = localTY + clearY;
                        if (localCY >= 0 && localCY < CHUNK_HEIGHT) {
                            Block above = chunk->getBlock(lx, localCY, lz);
                            BlockType atype = above.getType();
                            if (atype != BlockType::AIR && atype != BlockType::WATER) {
                                chunk->setBlock(lx, localCY, lz, Block(BlockType::AIR));
                            }
                        }
                    }
                }
            }
        }
        
        // ---- PHASE 2: CITY ROAD GRID ----
        // Cities get a road grid pattern at the unified cityY level.
        // Block materials are read from vxstruct files so they can be edited with the VxStruct editor.
        // Only the 2 main roads (through center) get bridges over rivers.
        if (isCity) {
            int roadSpacing = cityBlockSize; // road every cityBlockSize blocks
            int roadWidth = 7;   // 7-block wide roads (matches vxstruct size)
            
            // Load road/bridge vxstructs for material lookup
            auto roadStraightStruct = registry.getStructure("road_straight");
            auto roadTurnStruct = registry.getStructure("road_turn");
            auto bridgeStraightStruct = registry.getStructure("bridge_straight");
            
            // Helper: get block type + metadata from a vxstruct at a given cross-section position
            // crossPos = offset perpendicular to road direction (0..6)
            // alongPos = offset along road direction (0..6, wraps)
            auto getVxStructBlock = [](const std::shared_ptr<Structure>& structure, int crossPos, int alongPos) -> std::pair<BlockType, uint8_t> {
                if (!structure) return {BlockType::GLAZED_TERRACOTTA, uint8_t(0)};
                glm::ivec3 size = structure->getSize();
                int cx = std::clamp(crossPos, 0, size.x - 1);
                int az = ((alongPos % size.z) + size.z) % size.z;
                glm::ivec3 pos(cx, 0, az);
                BlockType bt = structure->getBlock(pos);
                uint8_t meta = structure->getBlockMetadata(pos);
                if (bt == BlockType::AIR) return {BlockType::GLAZED_TERRACOTTA, uint8_t(0)};
                return {bt, meta};
            };
            
            for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                    int worldRX = chunkBaseX + lx;
                    int worldRZ = chunkBaseZ + lz;
                    
                    // Distance check - only within city radius
                    float dx = static_cast<float>(worldRX) - settlementCenterX;
                    float dz = static_cast<float>(worldRZ) - settlementCenterZ;
                    float dist = std::sqrt(dx * dx + dz * dz);
                    if (dist > cityRadius) continue;
                    
                    // Offset from settlement center
                    int relX = worldRX - static_cast<int>(settlementCenterX);
                    int relZ = worldRZ - static_cast<int>(settlementCenterZ);
                    
                    // Check if on a road line (modulo roadSpacing)
                    int modX = ((relX % roadSpacing) + roadSpacing) % roadSpacing;
                    int modZ = ((relZ % roadSpacing) + roadSpacing) % roadSpacing;
                    
                    bool isRoadX = modX < roadWidth; // N-S road (perpendicular = X)
                    bool isRoadZ = modZ < roadWidth; // E-W road (perpendicular = Z)
                    
                    if (!isRoadX && !isRoadZ) continue;
                    
                    int localRY = cityY - chunkBaseY;
                    if (localRY < 0 || localRY >= CHUNK_HEIGHT) continue;
                    
                    // Check for river
                    float riverMask = getRiverMask(static_cast<float>(worldRX), static_cast<float>(worldRZ));
                    bool isOnRiver = riverMask > 0.25f;
                    
                    // Only the 2 main roads through center get bridges
                    bool isMainRoadNS = isRoadX && (std::abs(relX) < roadWidth);
                    bool isMainRoadEW = isRoadZ && (std::abs(relZ) < roadWidth);
                    bool isBridge = isOnRiver && (isMainRoadNS || isMainRoadEW);
                    
                    if (isOnRiver && !isBridge) continue; // Skip non-bridge road segments over rivers
                    
                    // Determine block type + metadata from vxstruct
                    std::pair<BlockType, uint8_t> roadMatPair;
                    bool isIntersection = isRoadX && isRoadZ;
                    
                    if (isBridge) {
                        // Bridge deck: sample from bridge vxstruct
                        if (isMainRoadNS) {
                            roadMatPair = getVxStructBlock(bridgeStraightStruct, modX, relZ);
                        } else {
                            roadMatPair = getVxStructBlock(bridgeStraightStruct, modZ, relX);
                            // Rotate face 90° CW to match E-W orientation
                            roadMatPair.second = (roadMatPair.second + 1) & 0x03;
                        }
                    } else if (isIntersection) {
                        // Intersection: use road_turn vxstruct for material
                        roadMatPair = getVxStructBlock(roadTurnStruct, modX, modZ);
                    } else if (isRoadX) {
                        // N-S road: cross-section is X, along is Z
                        roadMatPair = getVxStructBlock(roadStraightStruct, modX, relZ);
                    } else {
                        // E-W road: cross-section is Z, along is X
                        roadMatPair = getVxStructBlock(roadStraightStruct, modZ, relX);
                        // Rotate face 90° CW to match E-W orientation
                        roadMatPair.second = (roadMatPair.second + 1) & 0x03;
                    }
                    
                    chunk->setBlock(lx, localRY, lz, Block(roadMatPair.first, roadMatPair.second));
                    
                    // Clear blocks above road/bridge for headroom
                    for (int clearY = 1; clearY <= 6; clearY++) {
                        int localCY = localRY + clearY;
                        if (localCY >= 0 && localCY < CHUNK_HEIGHT) {
                            Block above = chunk->getBlock(lx, localCY, lz);
                            if (above.getType() != BlockType::AIR && above.getType() != BlockType::WATER) {
                                chunk->setBlock(lx, localCY, lz, Block(BlockType::AIR));
                            }
                        }
                    }
                }
            }
        }
        
        // ---- VILLAGE PATH NETWORK ----
        // Villages use vxstruct-defined road materials for main roads (7-wide),
        // and simple DIRT for short connecting paths (side lanes, perimeter ring).
        // Main roads get bridges over rivers (max 2 bridges per village).
        if (!isCity) {
            int centerIX = static_cast<int>(settlementCenterX);
            int centerIZ = static_cast<int>(settlementCenterZ);
            
            // Load village road/bridge vxstructs for material lookup
            auto vRoadStraightStruct = registry.getStructure("v_road_straight");
            auto vRoadTurnStruct = registry.getStructure("v_road_turn");
            auto vBridgeStraightStruct = registry.getStructure("v_bridge_straight");
            
            // Helper: get block type + metadata from a village vxstruct at a given position
            // Falls back to DIRT (village default) if structure is null or block is AIR
            auto getVxBlock = [](const std::shared_ptr<Structure>& structure, int crossPos, int alongPos) -> std::pair<BlockType, uint8_t> {
                if (!structure) return {BlockType::DIRT, uint8_t(0)};
                glm::ivec3 size = structure->getSize();
                int cx = std::clamp(crossPos, 0, size.x - 1);
                int az = ((alongPos % size.z) + size.z) % size.z;
                glm::ivec3 pos(cx, 0, az);
                BlockType bt = structure->getBlock(pos);
                uint8_t meta = structure->getBlockMetadata(pos);
                if (bt == BlockType::AIR) return {BlockType::DIRT, uint8_t(0)};
                return {bt, meta};
            };
            
            // --- Pre-scan for river crossings on main roads (for bridges) ---
            struct RiverCrossing { float centerCoord; };
            std::vector<RiverCrossing> crossingsNS;
            std::vector<RiverCrossing> crossingsEW;
            
            // Scan N-S main path for river crossings
            {
                bool inRiver = false;
                int riverStart = 0;
                for (int scanZ = centerIZ - 45; scanZ <= centerIZ + 45; scanZ++) {
                    float rm = getRiverMask(static_cast<float>(centerIX), static_cast<float>(scanZ));
                    if (rm > 0.25f && !inRiver) { inRiver = true; riverStart = scanZ; }
                    else if (rm <= 0.25f && inRiver) {
                        inRiver = false;
                        crossingsNS.push_back({static_cast<float>(riverStart + scanZ) * 0.5f});
                    }
                }
                if (inRiver) crossingsNS.push_back({static_cast<float>(riverStart + centerIZ + 45) * 0.5f});
            }
            // Scan E-W main path for river crossings
            {
                bool inRiver = false;
                int riverStart = 0;
                for (int scanX = centerIX - 45; scanX <= centerIX + 45; scanX++) {
                    float rm = getRiverMask(static_cast<float>(scanX), static_cast<float>(centerIZ));
                    if (rm > 0.25f && !inRiver) { inRiver = true; riverStart = scanX; }
                    else if (rm <= 0.25f && inRiver) {
                        inRiver = false;
                        crossingsEW.push_back({static_cast<float>(riverStart + scanX) * 0.5f});
                    }
                }
                if (inRiver) crossingsEW.push_back({static_cast<float>(riverStart + centerIX + 45) * 0.5f});
            }
            
            // Limit to 2 total bridges closest to village center
            if (static_cast<int>(crossingsNS.size() + crossingsEW.size()) > 2) {
                struct CrossingInfo { float dist; bool isNS; int index; };
                std::vector<CrossingInfo> allCrossings;
                for (int i = 0; i < static_cast<int>(crossingsNS.size()); i++)
                    allCrossings.push_back({std::abs(crossingsNS[i].centerCoord - static_cast<float>(centerIZ)), true, i});
                for (int i = 0; i < static_cast<int>(crossingsEW.size()); i++)
                    allCrossings.push_back({std::abs(crossingsEW[i].centerCoord - static_cast<float>(centerIX)), false, i});
                std::sort(allCrossings.begin(), allCrossings.end(),
                    [](const CrossingInfo& a, const CrossingInfo& b) { return a.dist < b.dist; });
                std::vector<bool> keepNS(crossingsNS.size(), false), keepEW(crossingsEW.size(), false);
                int kept = 0;
                for (auto& ci : allCrossings) {
                    if (kept >= 2) break;
                    if (ci.isNS) keepNS[ci.index] = true; else keepEW[ci.index] = true;
                    kept++;
                }
                std::vector<RiverCrossing> filteredNS, filteredEW;
                for (int i = 0; i < static_cast<int>(crossingsNS.size()); i++) if (keepNS[i]) filteredNS.push_back(crossingsNS[i]);
                for (int i = 0; i < static_cast<int>(crossingsEW.size()); i++) if (keepEW[i]) filteredEW.push_back(crossingsEW[i]);
                crossingsNS = filteredNS;
                crossingsEW = filteredEW;
            }
            
            // --- Place paths ---
            for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                    int worldPX = chunkBaseX + lx;
                    int worldPZ = chunkBaseZ + lz;
                    int relX = worldPX - centerIX;
                    int relZ = worldPZ - centerIZ;
                    
                    // Distance check - only within village radius
                    float pdx = static_cast<float>(worldPX) - settlementCenterX;
                    float pdz = static_cast<float>(worldPZ) - settlementCenterZ;
                    float pathDist = std::sqrt(pdx * pdx + pdz * pdz);
                    if (pathDist > 45.0f) continue;
                    
                    // Skip roads over water bodies (ocean/lake) - rivers handled separately via bridges
                    int waterCheckSurface = getSurfaceHeight(worldPX, worldPZ);
                    if (waterCheckSurface < SEA_LEVEL - 1) {
                        float waterCheckRm = getRiverMask(static_cast<float>(worldPX), static_cast<float>(worldPZ));
                        if (waterCheckRm <= 0.25f) continue; // Not a river → ocean/lake, skip
                    }
                    
                    // === Path type detection ===
                    // 1. Village square (7x7 center) — uses v_road_turn vxstruct
                    bool isVillageSquare = (std::abs(relX) <= 3 && std::abs(relZ) <= 3);
                    
                    // 2. Main roads (7 blocks wide through center) — uses v_road_straight vxstruct
                    bool isMainRoadNS = (std::abs(relX) <= 3); // N-S road
                    bool isMainRoadEW = (std::abs(relZ) <= 3); // E-W road
                    bool isMainRoad = isMainRoadNS || isMainRoadEW;
                    
                    // 3. Perimeter ring road at ~38 blocks from center (3-wide) — dirt path
                    int perimRoadRadius = 38;
                    int absRelX = std::abs(relX);
                    int absRelZ = std::abs(relZ);
                    int perimDistI = std::max(absRelX, absRelZ);
                    bool isPerimeterRoad = (perimDistI >= perimRoadRadius - 1 && perimDistI <= perimRoadRadius + 1);
                    if (isPerimeterRoad && absRelX > perimRoadRadius && absRelZ > perimRoadRadius)
                        isPerimeterRoad = false;
                    
                    // 4. Side connecting lanes (1 block wide, every ~16 blocks) — dirt path
                    int lanePeriod = 16;
                    int laneModZ = ((relZ % lanePeriod) + lanePeriod) % lanePeriod;
                    int laneModX = ((relX % lanePeriod) + lanePeriod) % lanePeriod;
                    bool isSideLaneEW = (laneModZ == 0) && (absRelX > 3) && (absRelX <= perimRoadRadius) && (relZ != 0);
                    bool isSideLaneNS = (laneModX == 0) && (absRelZ > 3) && (absRelZ <= perimRoadRadius) && (relX != 0);
                    bool isSideLane = isSideLaneEW || isSideLaneNS;
                    
                    bool isAnyPath = isVillageSquare || isMainRoad || isPerimeterRoad || isSideLane;
                    if (!isAnyPath) continue;
                    
                    // === River / bridge handling ===
                    float pathRiver = getRiverMask(static_cast<float>(worldPX), static_cast<float>(worldPZ));
                    bool isOnRiver = pathRiver > 0.25f;
                    
                    bool isBridge = false;
                    if (isOnRiver && (isMainRoad || isPerimeterRoad)) {
                        if (isMainRoadNS && !crossingsNS.empty()) isBridge = true;
                        if (!isBridge && isMainRoadEW && !crossingsEW.empty()) isBridge = true;
                        if (!isBridge && isPerimeterRoad) isBridge = true;
                    }
                    
                    if (isOnRiver && !isBridge) continue;
                    
                    // === Material selection ===
                    // Main roads & village square: read from village vxstruct files
                    // Short paths (side lanes, perimeter): plain dirt
                    std::pair<BlockType, uint8_t> pathMatPair = {BlockType::DIRT, uint8_t(0)};
                    if (isVillageSquare) {
                        // Village square 7x7: map relX+3 -> vxstruct x (0..6), relZ+3 -> z (0..6)
                        pathMatPair = getVxBlock(vRoadTurnStruct, relX + 3, relZ + 3);
                    } else if (isMainRoad && !isSideLane) {
                        // Main roads 7-wide: map directly to v_road_straight (7x1x7)
                        if (isMainRoadNS) {
                            // N-S road: cross-section is X (-3..3 → 0..6), along is Z
                            pathMatPair = getVxBlock(vRoadStraightStruct, relX + 3, relZ);
                        } else {
                            // E-W road: cross-section is Z (-3..3 → 0..6), along is X
                            pathMatPair = getVxBlock(vRoadStraightStruct, relZ + 3, relX);
                            // Rotate face 90° CW to match E-W orientation
                            pathMatPair.second = (pathMatPair.second + 1) & 0x03;
                        }
                    } else if (isPerimeterRoad || isSideLane) {
                        // Short paths: plain dirt
                        pathMatPair = {BlockType::DIRT, uint8_t(0)};
                    } else {
                        pathMatPair = {BlockType::DIRT, uint8_t(0)};
                    }
                    
                    if (isBridge) {
                        // Use bridge vxstruct for bridge deck material (7-wide)
                        if (isMainRoadNS) {
                            pathMatPair = getVxBlock(vBridgeStraightStruct, relX + 3, relZ);
                        } else if (isMainRoadEW) {
                            pathMatPair = getVxBlock(vBridgeStraightStruct, relZ + 3, relX);
                            // Rotate face 90° CW to match E-W orientation
                            pathMatPair.second = (pathMatPair.second + 1) & 0x03;
                        }
                        // Perimeter bridge stays as DIRT deck
                        
                        int bridgeY = cityY;
                        int localBridgeY = bridgeY - chunkBaseY;
                        if (localBridgeY < 0 || localBridgeY >= CHUNK_HEIGHT) continue;
                        
                        chunk->setBlock(lx, localBridgeY, lz, Block(pathMatPair.first, pathMatPair.second));
                        
                        // Support pillars every 4 blocks on edges only
                        bool isEdge = false;
                        if (isMainRoadNS && (std::abs(relX) == 3)) isEdge = true;
                        if (isMainRoadEW && (std::abs(relZ) == 3)) isEdge = true;
                        if (isPerimeterRoad && (perimDistI == perimRoadRadius - 1 || perimDistI == perimRoadRadius + 1)) isEdge = true;
                        
                        int pillarCoord = isMainRoadNS ? relZ : relX;
                        bool isPillarPos = (((pillarCoord % 4) + 4) % 4 == 0);
                        
                        if (isEdge && isPillarPos) {
                            for (int sy = bridgeY - 1; sy >= SEA_LEVEL - 2; sy--) {
                                int localSY = sy - chunkBaseY;
                                if (localSY < 0 || localSY >= CHUNK_HEIGHT) continue;
                                Block below = chunk->getBlock(lx, localSY, lz);
                                BlockType bt = below.getType();
                                if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                    chunk->setBlock(lx, localSY, lz, Block(BlockType::COBBLESTONE));
                                } else break;
                            }
                        }
                        
                        // Clear headroom above bridge
                        for (int clearY = 1; clearY <= 5; clearY++) {
                            int localCY = localBridgeY + clearY;
                            if (localCY >= 0 && localCY < CHUNK_HEIGHT) {
                                Block above = chunk->getBlock(lx, localCY, lz);
                                if (above.getType() != BlockType::AIR)
                                    chunk->setBlock(lx, localCY, lz, Block(BlockType::AIR));
                            }
                        }
                    } else {
                        // ---- FLAT ROAD with tunnel/bridge logic ----
                        int roadY = cityY;
                        int naturalY = getSurfaceHeight(worldPX, worldPZ);
                        int heightDiff = naturalY - roadY;
                        int localRoadY = roadY - chunkBaseY;
                        if (localRoadY < 0 || localRoadY >= CHUNK_HEIGHT) continue;
                        
                        // Determine road edge status for tunnels (outermost blocks of 7-wide road)
                        bool isEdgePath = false;
                        if (isMainRoadNS && std::abs(relX) == 3) isEdgePath = true;
                        if (isMainRoadEW && std::abs(relZ) == 3) isEdgePath = true;
                        if (isPerimeterRoad && (perimDistI == perimRoadRadius - 1 || perimDistI == perimRoadRadius + 1)) isEdgePath = true;
                        
                        // === TUNNEL: terrain significantly above road ===
                        if (heightDiff > 2) {
                            chunk->setBlock(lx, localRoadY, lz, Block(pathMatPair.first, pathMatPair.second));
                            
                            for (int cy = 1; cy <= 4; cy++) {
                                int clearY = localRoadY + cy;
                                if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                    if (isEdgePath) {
                                        chunk->setBlock(lx, clearY, lz, Block(BlockType::COBBLESTONE));
                                    } else {
                                        chunk->setBlock(lx, clearY, lz, Block(BlockType::AIR));
                                    }
                                }
                            }
                            int ceilingY = localRoadY + 5;
                            if (ceilingY >= 0 && ceilingY < CHUNK_HEIGHT) {
                                chunk->setBlock(lx, ceilingY, lz, Block(BlockType::COBBLESTONE));
                            }
                            int belowY = localRoadY - 1;
                            if (belowY >= 0 && belowY < CHUNK_HEIGHT) {
                                chunk->setBlock(lx, belowY, lz, Block(BlockType::COBBLESTONE));
                            }
                        }
                        // === BRIDGE: road over gap ===
                        else if (heightDiff < -2) {
                            chunk->setBlock(lx, localRoadY, lz, Block(pathMatPair.first, pathMatPair.second));
                            
                            int pillarCoord = (isMainRoadNS || isSideLaneNS) ? relZ : relX;
                            bool isPillarPos = (((pillarCoord % 4) + 4) % 4 == 0);
                            if (isEdgePath && isPillarPos) {
                                for (int py = localRoadY - 1; py >= 0; py--) {
                                    Block below = chunk->getBlock(lx, py, lz);
                                    BlockType bt = below.getType();
                                    if (bt == BlockType::AIR || bt == BlockType::WATER)
                                        chunk->setBlock(lx, py, lz, Block(BlockType::COBBLESTONE));
                                    else break;
                                }
                            }
                            
                            for (int cy = 1; cy <= 5; cy++) {
                                int clearY = localRoadY + cy;
                                if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                    Block above = chunk->getBlock(lx, clearY, lz);
                                    if (above.getType() != BlockType::AIR)
                                        chunk->setBlock(lx, clearY, lz, Block(BlockType::AIR));
                                }
                            }
                        }
                        // === NORMAL: terrain close to road level ===
                        else {
                            if (naturalY > roadY) {
                                for (int cy = roadY + 1; cy <= naturalY + 2; cy++) {
                                    int localCY = cy - chunkBaseY;
                                    if (localCY >= 0 && localCY < CHUNK_HEIGHT)
                                        chunk->setBlock(lx, localCY, lz, Block(BlockType::AIR));
                                }
                            } else if (naturalY < roadY) {
                                for (int fy = naturalY + 1; fy < roadY; fy++) {
                                    int localFY = fy - chunkBaseY;
                                    if (localFY >= 0 && localFY < CHUNK_HEIGHT)
                                        chunk->setBlock(lx, localFY, lz, Block(BlockType::DIRT));
                                }
                            }
                            
                            chunk->setBlock(lx, localRoadY, lz, Block(pathMatPair.first, pathMatPair.second));
                            
                            for (int fd = 1; fd <= 3; fd++) {
                                int localFY = localRoadY - fd;
                                if (localFY >= 0 && localFY < CHUNK_HEIGHT) {
                                    Block below = chunk->getBlock(lx, localFY, lz);
                                    BlockType bt = below.getType();
                                    if (bt == BlockType::AIR || bt == BlockType::WATER)
                                        chunk->setBlock(lx, localFY, lz, Block(BlockType::DIRT));
                                }
                            }
                            
                            for (int cy = 1; cy <= 4; cy++) {
                                int clearY = localRoadY + cy;
                                if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                    Block above = chunk->getBlock(lx, clearY, lz);
                                    if (above.getType() != BlockType::AIR && above.getType() != BlockType::WATER)
                                        chunk->setBlock(lx, clearY, lz, Block(BlockType::AIR));
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // ---- STRUCTURE PLACEMENT ----
        // For cities: place buildings on a WORLD-SPACE grid aligned to city blocks,
        // so structures never overlap and never get cut at chunk boundaries.
        // For villages: use chunk-relative grid as before.
        if (isCity) {
            int roadWidth = 3;
            int sidewalkWidth = 1;
            int roadZone = roadWidth + sidewalkWidth; // Blocks reserved for road+sidewalk on each axis
            int buildableSize = cityBlockSize - roadZone - sidewalkWidth; // Usable interior per city block
            int structFootprint = 8; // Max footprint per structure slot
            
            // We need to check structure anchors that could place blocks INTO this chunk,
            // including anchors from neighboring chunks. Scan a range larger than CHUNK_SIZE.
            int scanMargin = structFootprint + 2; // Extra blocks to check beyond chunk boundaries
            int scanMinX = chunkBaseX - scanMargin;
            int scanMinZ = chunkBaseZ - scanMargin;
            int scanMaxX = chunkBaseX + CHUNK_SIZE + scanMargin;
            int scanMaxZ = chunkBaseZ + CHUNK_SIZE + scanMargin;
            
            // Iterate over WORLD-SPACE city block grid
            // Each city block's interior can hold multiple structure slots
            int cityBlockStartX = static_cast<int>(std::floor(static_cast<float>(scanMinX - static_cast<int>(settlementCenterX)) / cityBlockSize)) * cityBlockSize + static_cast<int>(settlementCenterX);
            int cityBlockStartZ = static_cast<int>(std::floor(static_cast<float>(scanMinZ - static_cast<int>(settlementCenterZ)) / cityBlockSize)) * cityBlockSize + static_cast<int>(settlementCenterZ);
            
            for (int blockX = cityBlockStartX; blockX < scanMaxX; blockX += cityBlockSize) {
                for (int blockZ = cityBlockStartZ; blockZ < scanMaxZ; blockZ += cityBlockSize) {
                    // Interior of this city block starts after road+sidewalk zone
                    int interiorStartX = blockX + roadZone;
                    int interiorStartZ = blockZ + roadZone;
                    
                    // Place structures in a sub-grid within the interior
                    // Number of slots that fit: buildableSize / structFootprint
                    int slotsPerAxis = buildableSize / structFootprint;
                    if (slotsPerAxis < 1) slotsPerAxis = 1;
                    
                    for (int slotX = 0; slotX < slotsPerAxis; slotX++) {
                        for (int slotZ = 0; slotZ < slotsPerAxis; slotZ++) {
                            int anchorX = interiorStartX + slotX * structFootprint;
                            int anchorZ = interiorStartZ + slotZ * structFootprint;
                            
                            // Quick bounds check - does this anchor's footprint touch our chunk at all?
                            if (anchorX + structFootprint <= chunkBaseX || anchorX >= chunkBaseX + CHUNK_SIZE) continue;
                            if (anchorZ + structFootprint <= chunkBaseZ || anchorZ >= chunkBaseZ + CHUNK_SIZE) continue;
                            
                            // Deterministic hash for this world-space anchor
                            unsigned int h = seed;
                            h ^= static_cast<unsigned int>(anchorX) * 374761393u;
                            h ^= static_cast<unsigned int>(anchorZ) * 668265263u;
                            h = ((h << 17) | (h >> 15)) * 2654435761u;
                            
                            // Spawn chance: 80%
                            if ((h % 100) >= 80u) continue;
                            
                            // Check for river across entire footprint (not just anchor)
                            {
                                bool touchesRiver = false;
                                int riverCheckPoints[][2] = {
                                    {anchorX, anchorZ},
                                    {anchorX + structFootprint - 1, anchorZ},
                                    {anchorX, anchorZ + structFootprint - 1},
                                    {anchorX + structFootprint - 1, anchorZ + structFootprint - 1},
                                    {anchorX + structFootprint / 2, anchorZ + structFootprint / 2}
                                };
                                for (int rp = 0; rp < 5; rp++) {
                                    float rm = getRiverMask(static_cast<float>(riverCheckPoints[rp][0]),
                                                            static_cast<float>(riverCheckPoints[rp][1]));
                                    if (rm > 0.25f) { touchesRiver = true; break; }
                                }
                                if (touchesRiver) continue;
                            }
                            
                            // Check if anchor is within city radius
                            float adx = static_cast<float>(anchorX) - settlementCenterX;
                            float adz = static_cast<float>(anchorZ) - settlementCenterZ;
                            float anchorDist = std::sqrt(adx * adx + adz * adz);
                            if (anchorDist > cityRadius - 10.0f) continue;
                            
                            int groundY = cityY;
                            
                            // Check if this vertical chunk slice overlaps the structure vertically
                            int maxStructHeight = 40;
                            if (chunkBaseY > groundY + maxStructHeight) continue;
                            if (chunkBaseY + CHUNK_HEIGHT <= groundY - 2) continue;
                            
                            // ---- PLACE STRUCTURE ----
                            if (!allStructures.empty()) {
                                StructureCategory category;
                                int roll = h % 100;
                                if (roll < 45) category = StructureCategory::CITY_BUILDING;
                                else if (roll < 80) category = StructureCategory::CITY_SKYSCRAPER;
                                else if (roll < 90) category = StructureCategory::CITY_PARK;
                                else category = StructureCategory::CITY_DECORATION;
                                
                                auto structureIds = registry.getStructuresByCategory(category);
                                if (structureIds.empty()) structureIds = registry.getAllStructureIds();
                                if (structureIds.empty()) continue;
                                
                                size_t idx = (h >> 16) % structureIds.size();
                                auto structure = registry.getStructure(structureIds[idx]);
                                if (!structure) continue;
                                
                                int rotation = ((h >> 24) % 4) * 90;
                                auto blocks = structure->getRotatedBlocks(rotation);
                                
                                for (const auto& block : blocks) {
                                    int worldBX = anchorX + block.position.x;
                                    int worldBY = groundY + block.position.y;
                                    int worldBZ = anchorZ + block.position.z;
                                    
                                    int localBX = worldBX - chunkBaseX;
                                    int localBY = worldBY - chunkBaseY;
                                    int localBZ = worldBZ - chunkBaseZ;
                                    
                                    if (localBX < 0 || localBX >= CHUNK_SIZE) continue;
                                    if (localBZ < 0 || localBZ >= CHUNK_SIZE) continue;
                                    if (localBY < 0 || localBY >= CHUNK_HEIGHT) continue;
                                    if (block.type == BlockType::AIR) continue;
                                    
                                    Block existing = chunk->getBlock(localBX, localBY, localBZ);
                                    BlockType existingType = existing.getType();
                                    if (existingType != BlockType::BEDROCK && existingType != BlockType::WATER) {
                                        chunk->setBlock(localBX, localBY, localBZ, Block(block.type, block.metadata));
                                    }
                                }
                                
                                // Fill foundation below structure to prevent floating buildings
                                auto structSize = structure->getSize();
                                int footprintMaxX = std::min(structSize.x, structFootprint);
                                int footprintMaxZ = std::min(structSize.z, structFootprint);
                                for (int fx = 0; fx < footprintMaxX; fx++) {
                                    for (int fz = 0; fz < footprintMaxZ; fz++) {
                                        int worldFX = anchorX + fx;
                                        int worldFZ = anchorZ + fz;
                                        int localFX = worldFX - chunkBaseX;
                                        int localFZ = worldFZ - chunkBaseZ;
                                        if (localFX < 0 || localFX >= CHUNK_SIZE) continue;
                                        if (localFZ < 0 || localFZ >= CHUNK_SIZE) continue;
                                        
                                        // Fill downward from groundY until we hit solid ground
                                        for (int fy = groundY - 1; fy >= groundY - 12; fy--) {
                                            int localFY = fy - chunkBaseY;
                                            if (localFY < 0 || localFY >= CHUNK_HEIGHT) continue;
                                            Block below = chunk->getBlock(localFX, localFY, localFZ);
                                            BlockType bt = below.getType();
                                            if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                                chunk->setBlock(localFX, localFY, localFZ, Block(BlockType::DIRT));
                                            } else {
                                                break; // Hit solid ground, stop filling
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // ---- VILLAGE STRUCTURE PLACEMENT (chunk-relative, unchanged) ----
        if (!isCity) {
            int structFootprint = 14;
            for (int gridOffX = 0; gridOffX < CHUNK_SIZE; gridOffX += structSpacing) {
                for (int gridOffZ = 0; gridOffZ < CHUNK_SIZE; gridOffZ += structSpacing) {
                    int anchorX = chunkBaseX + gridOffX + 1;
                    int anchorZ = chunkBaseZ + gridOffZ + 1;
                    
                    unsigned int h = seed;
                    h ^= static_cast<unsigned int>(anchorX) * 374761393u;
                    h ^= static_cast<unsigned int>(anchorZ) * 668265263u;
                    h = ((h << 17) | (h >> 15)) * 2654435761u;
                    
                    if ((h % 100) >= 65u) continue;
                    
                    // Village: sample terrain heights
                    int samplePoints[5][2] = {
                        {anchorX, anchorZ},
                        {anchorX + structFootprint - 1, anchorZ},
                        {anchorX, anchorZ + structFootprint - 1},
                        {anchorX + structFootprint - 1, anchorZ + structFootprint - 1},
                        {anchorX + structFootprint / 2, anchorZ + structFootprint / 2}
                    };
                    
                    int minGroundY = 9999, maxGroundY = -9999;
                    bool hasWater = false;
                    bool touchesRiver = false;
                    for (int i = 0; i < 5; i++) {
                        int sy = getSurfaceHeight(samplePoints[i][0], samplePoints[i][1]);
                        minGroundY = std::min(minGroundY, sy);
                        maxGroundY = std::max(maxGroundY, sy);
                        if (sy <= SEA_LEVEL) hasWater = true;
                        float rm = getRiverMask(static_cast<float>(samplePoints[i][0]),
                                                static_cast<float>(samplePoints[i][1]));
                        if (rm > 0.25f) touchesRiver = true;
                    }
                    
                    if (hasWater || touchesRiver) continue;
                    if ((maxGroundY - minGroundY) > 4) continue;
                    int groundY = (minGroundY + maxGroundY) / 2;
                    
                    int maxStructHeight = 12;
                    if (chunkBaseY > groundY + maxStructHeight) continue;
                    if (chunkBaseY + CHUNK_HEIGHT <= groundY - 2) continue;
                    
                    // Terrain flattening for villages
                    {
                        int flatRadius = 1;
                        for (int fx = -flatRadius; fx < structFootprint + flatRadius; fx++) {
                            for (int fz = -flatRadius; fz < structFootprint + flatRadius; fz++) {
                                int worldFX = anchorX + fx;
                                int worldFZ = anchorZ + fz;
                                int localFX = worldFX - chunkBaseX;
                                int localFZ = worldFZ - chunkBaseZ;
                                
                                if (localFX < 0 || localFX >= CHUNK_SIZE) continue;
                                if (localFZ < 0 || localFZ >= CHUNK_SIZE) continue;
                                
                                // Skip river areas - don't flatten over rivers
                                float flatRiver = getRiverMask(static_cast<float>(worldFX), static_cast<float>(worldFZ));
                                if (flatRiver > 0.25f) continue;
                                
                                int naturalY = getSurfaceHeight(worldFX, worldFZ);
                                
                                // Fill up terrain to groundY
                                for (int fy = naturalY + 1; fy <= groundY; fy++) {
                                    int localFY = fy - chunkBaseY;
                                    if (localFY < 0 || localFY >= CHUNK_HEIGHT) continue;
                                    chunk->setBlock(localFX, localFY, localFZ, Block(fy == groundY ? BlockType::GRASS : BlockType::DIRT));
                                }
                                // Carve down terrain to groundY
                                for (int fy = groundY + 1; fy <= naturalY; fy++) {
                                    int localFY = fy - chunkBaseY;
                                    if (localFY < 0 || localFY >= CHUNK_HEIGHT) continue;
                                    chunk->setBlock(localFX, localFY, localFZ, Block(BlockType::AIR));
                                }
                                // Ensure grass on top
                                int localGY = groundY - chunkBaseY;
                                if (localGY >= 0 && localGY < CHUNK_HEIGHT) {
                                    chunk->setBlock(localFX, localGY, localFZ, Block(BlockType::GRASS));
                                }
                                // Replace exposed stone/cobble below grass with dirt
                                for (int d = 1; d <= 3; d++) {
                                    int localDY = groundY - d - chunkBaseY;
                                    if (localDY >= 0 && localDY < CHUNK_HEIGHT) {
                                        Block below = chunk->getBlock(localFX, localDY, localFZ);
                                        BlockType bt = below.getType();
                                        if (bt == BlockType::STONE || bt == BlockType::COBBLESTONE || bt == BlockType::AIR) {
                                            chunk->setBlock(localFX, localDY, localFZ, Block(BlockType::DIRT));
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // Place structure
                    if (!allStructures.empty()) {
                        StructureCategory category;
                        int roll = (h >> 8) % 100;
                        if (roll < 35) category = StructureCategory::VILLAGE_HOUSE;
                        else if (roll < 55) category = StructureCategory::VILLAGE_BUILDING;
                        else if (roll < 70) category = StructureCategory::VILLAGE_FARM;
                        else if (roll < 80) category = StructureCategory::VILLAGE_WELL;
                        else if (roll < 90) category = StructureCategory::VILLAGE_DECORATION;
                        else category = StructureCategory::VILLAGE_PATH;
                        
                        auto structureIds = registry.getStructuresByCategory(category);
                        if (structureIds.empty()) structureIds = registry.getAllStructureIds();
                        if (structureIds.empty()) continue;
                        
                        size_t idx = (h >> 16) % structureIds.size();
                        auto structure = registry.getStructure(structureIds[idx]);
                        if (!structure) continue;
                        
                        int rotation = ((h >> 24) % 4) * 90;
                        auto blocks = structure->getRotatedBlocks(rotation);
                        
                        for (const auto& block : blocks) {
                            int worldBX = anchorX + block.position.x;
                            int worldBY = groundY + block.position.y;
                            int worldBZ = anchorZ + block.position.z;
                            
                            int localBX = worldBX - chunkBaseX;
                            int localBY = worldBY - chunkBaseY;
                            int localBZ = worldBZ - chunkBaseZ;
                            
                            if (localBX < 0 || localBX >= CHUNK_SIZE) continue;
                            if (localBZ < 0 || localBZ >= CHUNK_SIZE) continue;
                            if (localBY < 0 || localBY >= CHUNK_HEIGHT) continue;
                            if (block.type == BlockType::AIR) continue;
                            
                            Block existing = chunk->getBlock(localBX, localBY, localBZ);
                            BlockType existingType = existing.getType();
                            if (existingType != BlockType::BEDROCK && existingType != BlockType::WATER) {
                                chunk->setBlock(localBX, localBY, localBZ, Block(block.type, block.metadata));
                            }
                        }
                    } else {
                        // Fallback: pink wool house
                        int houseSize = 5;
                        int houseHeight = 5;
                        for (int hx = 0; hx < houseSize; hx++) {
                            for (int hz = 0; hz < houseSize; hz++) {
                                for (int hy = 0; hy <= houseHeight; hy++) {
                                    int worldBY = groundY + hy;
                                    int localBX = (gridOffX + 1) + hx;
                                    int localBY = worldBY - chunkBaseY;
                                    int localBZ = (gridOffZ + 1) + hz;
                                    
                                    if (localBX < 0 || localBX >= CHUNK_SIZE) continue;
                                    if (localBZ < 0 || localBZ >= CHUNK_SIZE) continue;
                                    if (localBY < 0 || localBY >= CHUNK_HEIGHT) continue;
                                    
                                    bool isWall = (hx == 0 || hx == houseSize-1 || hz == 0 || hz == houseSize-1);
                                    bool isDoor = (hx == houseSize/2 && hz == 0 && hy >= 1 && hy <= 2);
                                    
                                    if (hy == 0)
                                        chunk->setBlock(localBX, localBY, localBZ, Block(BlockType::COBBLESTONE));
                                    else if (hy == houseHeight)
                                        chunk->setBlock(localBX, localBY, localBZ, Block(BlockType::RED_WOOL));
                                    else if (isWall && !isDoor)
                                        chunk->setBlock(localBX, localBY, localBZ, Block(BlockType::PINK_WOOL));
                                }
                            }
                        }
                    }
                } // gridOffZ
            } // gridOffX
        } // !isCity
    }();
    
    // ---- INTER-SETTLEMENT ROAD NETWORK ----
    // Connect cities and villages with roads, bridges, and tunnels
    {
        int roadChunkBaseX = static_cast<int>(worldPos.x);
        int roadChunkBaseZ = static_cast<int>(worldPos.z);
        int roadChunkBaseY = static_cast<int>(worldPos.y);
        m_roadNetwork.placeRoadsInChunk(roadChunkBaseX, roadChunkBaseZ, roadChunkBaseY, *this, chunk);
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
    
    // Combine all layers - this is the base terrain height BEFORE river carving
    float finalHeight = baseHeight + finalHillHeight + mountainHeight + detailHeight;

    // River carving: create river-like water channels instead of random inland water.
    float rX = x * 0.0035f + offsetPVX * 0.25f + 31000.0f;
    float rZ = z * 0.0035f + offsetPVZ * 0.25f + 42000.0f;
    float riverBase = fbm(rX, rZ, 3);
    float riverVal = 1.0f - std::abs(riverBase);
    float riverMask = std::pow(std::clamp((riverVal - 0.78f) / 0.22f, 0.0f, 1.0f), 2.6f);
    riverMask *= landFactor;
    
    // UNDERGROUND RIVERS: When rivers meet mountains, they go underground instead of cutting through
    // Calculate how much the river should be suppressed (goes underground) vs carve surface
    float undergroundRiverFactor = riverMask * mountainFactor;
    
    // Reduce surface river carving based on mountain presence
    // When mountainFactor > 0.3, river goes completely underground (no surface carving)
    // This prevents mountains from being cut through vertically
    float surfaceRiverMask = 0.0f;
    if (mountainFactor < 0.3f) {
        // Only carve surface rivers in flat/low areas
        float mountainSuppression = mountainFactor / 0.3f;  // 0 to 1 as mountain increases
        surfaceRiverMask = riverMask * (1.0f - mountainSuppression);
    }
    // In mountains (mountainFactor >= 0.3), surfaceRiverMask stays 0 - river goes underground

    // SIMPLE RIVER SYSTEM: Fixed water level, carve terrain down to it
    // River water surface is always at SEA_LEVEL (flat like a real river)
    // River bed is 3 blocks below water surface
    float riverWaterSurface = (float)SEA_LEVEL;
    float riverBedLevel = riverWaterSurface - 3.0f;
    
    if (surfaceRiverMask > 0.3f && finalHeight > riverBedLevel) {
        if (surfaceRiverMask > 0.5f) {
            // River center - carve down to river bed (underwater)
            float depthFactor = (surfaceRiverMask - 0.5f) / 0.5f;
            float bedDepth = riverBedLevel - depthFactor * 2.0f;  // Center slightly deeper
            finalHeight = std::min(finalHeight, bedDepth);
        } else if (surfaceRiverMask > 0.4f) {
            // Inner shore - slopes from water edge down to river bed
            float slopeFactor = (surfaceRiverMask - 0.4f) / 0.1f;
            float shoreLevel = lerp(riverWaterSurface + 1.0f, riverBedLevel, slopeFactor);
            finalHeight = std::min(finalHeight, shoreLevel);
        } else {
            // Outer shore (0.3-0.4) - just above water, sandy beach
            float slopeFactor = (surfaceRiverMask - 0.3f) / 0.1f;
            float beachLevel = lerp(finalHeight, riverWaterSurface + 1.0f, slopeFactor);
            finalHeight = std::min(finalHeight, beachLevel);
        }
    }

    // Prevent random inland lakes: keep most land above sea level unless we're in a river.
    // But allow underground river areas to have normal mountain height
    if (continentalness > 0.00f && surfaceRiverMask < 0.15f && undergroundRiverFactor < 0.15f) {
        finalHeight = std::max(finalHeight, (float)SEA_LEVEL + 3.0f);
    }
    
    // Ensure minimum heights
    finalHeight = std::max(finalHeight, 5.0f);
    
    // Apply height power curve for more dramatic peaks (but keep extremes rarer)
    if (finalHeight > 75.0f) {
        float excess = finalHeight - 75.0f;
        finalHeight = 75.0f + std::pow(excess / 120.0f, 0.85f) * 120.0f;
    }

    // ========== SETTLEMENT FLATTENING (applied last so nothing overrides it) ==========
    // Flatten terrain in city/village biome areas so structures sit on level ground.
    // Uses the same jittered grid logic as getBiome() for consistency.
    {
        const float CITY_GRID_F    = 500.0f;
        const float CITY_RADIUS_F  = 160.0f;
        const float CITY_JITTER_F  = 120.0f;
        const float VILLAGE_GRID_F    = 350.0f;
        const float VILLAGE_RADIUS_F  = 45.0f;
        const float VILLAGE_JITTER_F  = 80.0f;
        const float CITY_TRANSITION    = 100.0f;  // Wide gradual slope at city edges
        const float VILLAGE_TRANSITION = 40.0f;   // Wider transition to match edgeBlend=30

        // Same jitter function as getBiome
        auto settlementJitterH = [&](float gridCX, float gridCZ, float jitterAmount, float gridSize) -> std::pair<float, float> {
            unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
            unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
            unsigned int h1 = (seed ^ hx ^ hz) * 2654435761u;
            unsigned int h2 = (seed ^ hz ^ (hx * 2246822519u)) * 3266489917u;
            float offsetX = ((h1 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
            float offsetZ = ((h2 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
            return {gridCX + offsetX, gridCZ + offsetZ};
        };
        auto shouldSpawnH = [&](float gridCX, float gridCZ, float gridSize, float spawnChance) -> bool {
            unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
            unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
            unsigned int h = (seed ^ hx ^ hz ^ 987654321u) * 2246822519u;
            float roll = (h & 0xFFFF) / 65536.0f;
            return roll < spawnChance;
        };

        float cityGridBaseX = std::floor(x / CITY_GRID_F) * CITY_GRID_F + CITY_GRID_F / 2.0f;
        float cityGridBaseZ = std::floor(z / CITY_GRID_F) * CITY_GRID_F + CITY_GRID_F / 2.0f;
        auto [cityJX, cityJZ] = settlementJitterH(cityGridBaseX, cityGridBaseZ, CITY_JITTER_F, CITY_GRID_F);
        float cityDist = std::sqrt((x - cityJX) * (x - cityJX) + (z - cityJZ) * (z - cityJZ));
        
        float villageGridBaseX = std::floor(x / VILLAGE_GRID_F) * VILLAGE_GRID_F + VILLAGE_GRID_F / 2.0f;
        float villageGridBaseZ = std::floor(z / VILLAGE_GRID_F) * VILLAGE_GRID_F + VILLAGE_GRID_F / 2.0f;
        auto [villageJX, villageJZ] = settlementJitterH(villageGridBaseX, villageGridBaseZ, VILLAGE_JITTER_F, VILLAGE_GRID_F);
        float villageDist = std::sqrt((x - villageJX) * (x - villageJX) + (z - villageJZ) * (z - villageJZ));

        // Only flatten if this grid cell actually spawns a settlement
        float edgeCheckRH = CITY_RADIUS_F * 0.7f;
        bool citySpawns = shouldSpawnH(cityGridBaseX, cityGridBaseZ, CITY_GRID_F, 0.70f) &&
                          getMountainFactor(cityJX, cityJZ) < 0.15f &&
                          std::max({getMountainFactor(cityJX + edgeCheckRH, cityJZ),
                                    getMountainFactor(cityJX - edgeCheckRH, cityJZ),
                                    getMountainFactor(cityJX, cityJZ + edgeCheckRH),
                                    getMountainFactor(cityJX, cityJZ - edgeCheckRH)}) < 0.30f;
        bool villageSpawns = shouldSpawnH(villageGridBaseX, villageGridBaseZ, VILLAGE_GRID_F, 0.45f) &&
                             getMountainFactor(villageJX, villageJZ) < 0.35f;

        // Check river mask - NEVER flatten river areas (preserve natural river channels)
        float riverHere = getRiverMask(x, z);
        float riverPreserve = 1.0f; // 1.0 = full flatten allowed, 0.0 = no flatten (river)
        if (riverHere > 0.15f) {
            // Smooth transition: fully preserved at riverMask >= 0.30, partial at 0.15-0.30
            riverPreserve = 1.0f - std::clamp((riverHere - 0.15f) / 0.15f, 0.0f, 1.0f);
        }

        if (citySpawns && cityDist < CITY_RADIUS_F + CITY_TRANSITION) {
            float outerR = CITY_RADIUS_F + CITY_TRANSITION;
            float edgeFade = std::clamp((outerR - cityDist) / CITY_TRANSITION, 0.0f, 1.0f);
            edgeFade = edgeFade * edgeFade * (3.0f - 2.0f * edgeFade);
            float flattenStrength = edgeFade * 0.995f * riverPreserve;
            finalHeight = lerp(finalHeight, baseHeight, flattenStrength);
        } else if (villageSpawns && villageDist < VILLAGE_RADIUS_F + VILLAGE_TRANSITION &&
                   !(citySpawns && cityDist < CITY_RADIUS_F + CITY_TRANSITION)) {
            float outerR = VILLAGE_RADIUS_F + VILLAGE_TRANSITION;
            float edgeFade = std::clamp((outerR - villageDist) / VILLAGE_TRANSITION, 0.0f, 1.0f);
            edgeFade = edgeFade * edgeFade * (3.0f - 2.0f * edgeFade);
            // High flatten strength (matching cities) so roads sit flush with terrain.
            // Without this, terrain bumps remain and flat roads float above ground.
            float flattenStrength = edgeFade * 0.995f * riverPreserve;
            finalHeight = lerp(finalHeight, baseHeight, flattenStrength);
        }
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

// grad is inline in header for performance

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

// lerp and fade are now inline in the header

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
