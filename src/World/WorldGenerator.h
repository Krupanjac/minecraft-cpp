#pragma once

#include "../Util/Types.h"
#include "Chunk.h"
#include <memory>
#include <string>

// Forward declaration
class StructurePlacer;

enum class BiomeType {
    OCEAN,
    RIVER,
    PLAINS,
    DESERT,
    FOREST,           // Oak forest
    BIRCH_FOREST,     // Birch forest
    TAIGA,            // Spruce forest (cold)
    JUNGLE,           // Jungle
    SWAMP,            // Swamp
    MOUNTAINS,
    SNOWY_TUNDRA,
    SAVANNA,          // Hot plains with acacia-like trees
    VILLAGE,          // Flat plains with village structures
    CITY              // Flat area with city buildings
};

enum class TreeType {
    OAK,
    BIRCH,
    SPRUCE,
    JUNGLE,
    NONE
};

struct BiomeInfo {
    BiomeType type;
    float temperature;    // 0 = cold, 1 = hot
    float humidity;       // 0 = dry, 1 = wet
    float heightVariation;
    BlockType surfaceBlock;
    BlockType subsurfaceBlock;
    int surfaceDepth;
    
    // Biome-specific colors for grass/foliage tinting
    float grassColorR, grassColorG, grassColorB;
    float foliageColorR, foliageColorG, foliageColorB;
    // Map color for minimap display
    float mapColorR, mapColorG, mapColorB;
};

class WorldGenerator {
public:
    WorldGenerator(unsigned int seed = 12345);
    ~WorldGenerator();  // Defined in .cpp since StructurePlacer is incomplete here

    void setSeed(unsigned int s);
    void initializeStructures(const std::string& structuresPath);
    void generate(std::shared_ptr<Chunk> chunk);
    
    float getNoise(float x, float y, float z) const;
    float getHeight(float x, float z) const;
    int getSurfaceHeight(int x, int z) const;
    BiomeType getBiome(float x, float z) const;
    BiomeInfo getBiomeInfo(BiomeType biome) const;
    unsigned int getSeed() const { return seed; }
    
    // Structure placement
    StructurePlacer* getStructurePlacer() { return structurePlacer.get(); }
    const StructurePlacer* getStructurePlacer() const { return structurePlacer.get(); }

private:
    unsigned int seed;
    std::unique_ptr<StructurePlacer> structurePlacer;
    
    // Randomized World Parameters
    float offsetContinentX = 0.0f;
    float offsetContinentZ = 0.0f;
    float offsetTempX = 0.0f;
    float offsetTempZ = 0.0f;
    float offsetHumidX = 0.0f;
    float offsetHumidZ = 0.0f;
    float offsetErosionX = 0.0f;
    float offsetErosionZ = 0.0f;
    float offsetPVX = 0.0f;
    float offsetPVZ = 0.0f;

    float globalTempBias = 0.0f;  // -0.2 to 0.2
    float globalHumidBias = 0.0f; // -0.2 to 0.2
    float mountainScaleBias = 1.0f; // 0.8 to 1.2
    float globalCaveDensityBias = 0.0f; // -0.05 to 0.05
    float globalCaveWaterBias = 0.0f; // -1.0 to 0.0 (Dryer caves to Normal)
    float globalFrequencyBias = 1.0f; // 0.5 to 1.5 (Larger vs Smaller features)
    
    // Simple Perlin-like noise - hot path functions inlined for performance
    float noise3D(float x, float y, float z) const;
    inline float noise2D(float x, float z) const { return noise3D(x, 0, z); }
    float fbm(float x, float z, int octaves) const; // Fractal Brownian Motion
    float ridgeNoise(float x, float z) const;
    float ridgedMultifractal(float x, float z, int octaves, float lacunarity, float gain, float offset) const; // Alpine mountains
    float billowNoise(float x, float z) const; // Billow noise (abs of noise)
    float turbulence(float x, float z, int octaves) const; // Turbulent noise
    float domainWarp(float& x, float& z) const;
    inline float lerp(float a, float b, float t) const { return a + t * (b - a); }
    inline float fade(float t) const { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    inline float grad(int hash, float x, float y, float z) const {
        // Convert low 4 bits of hash code into 12 gradient directions
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
    
    // Cached biome info for fast lookups
    static BiomeInfo biomeInfoCache[14];
    static bool biomeInfoCacheInitialized;
    
    // Spline helper
    float getSplineHeight(float continentalness, float erosion, float pv) const;
    
    // Biome-related
    float getTemperature(float x, float z) const;
    float getHumidity(float x, float z) const;
    
    // River helpers - returns river strength [0,1] and mountain factor
    float getRiverMask(float x, float z) const;
    float getMountainFactor(float x, float z) const;
    bool isUndergroundRiver(float x, float y, float z, float riverMask, float mountainFactor) const;
    
    // Cave generation
    bool isCave(float x, float y, float z) const;
    bool isCave(float x, float y, float z, int precomputedSurfaceHeight) const;
    
    // Vegetation helpers
    bool hasTree(int x, int z, BiomeType biome) const;
    int getTreeHeight(int x, int z, BiomeType biome) const;
    TreeType getTreeType(BiomeType biome) const;
    BlockType getLogType(TreeType tree) const;
    BlockType getLeavesType(TreeType tree) const;
};

    
