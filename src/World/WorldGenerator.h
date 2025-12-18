#pragma once

#include "../Util/Types.h"
#include "Chunk.h"
#include <memory>

enum class BiomeType {
    OCEAN,
    RIVER,
    PLAINS,
    DESERT,
    FOREST,
    MOUNTAINS,
    SNOWY_TUNDRA
};

struct BiomeInfo {
    BiomeType type;
    float temperature;    // 0 = cold, 1 = hot
    float humidity;       // 0 = dry, 1 = wet
    float heightVariation;
    BlockType surfaceBlock;
    BlockType subsurfaceBlock;
    int surfaceDepth;
};

class WorldGenerator {
public:
    WorldGenerator(unsigned int seed = 12345);
    ~WorldGenerator() = default;

    void setSeed(unsigned int s);
    void generate(std::shared_ptr<Chunk> chunk);
    
    float getNoise(float x, float y, float z) const;
    float getHeight(float x, float z) const;
    int getSurfaceHeight(int x, int z) const;
    BiomeType getBiome(float x, float z) const;
    BiomeInfo getBiomeInfo(BiomeType biome) const;

private:
    unsigned int seed;
    
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
    
    // Simple Perlin-like noise
    float noise3D(float x, float y, float z) const;
    float noise2D(float x, float z) const;
    float fbm(float x, float z, int octaves) const; // Fractal Brownian Motion
    float ridgeNoise(float x, float z) const;
    float ridgedMultifractal(float x, float z, int octaves, float lacunarity, float gain, float offset) const; // Alpine mountains
    float billowNoise(float x, float z) const; // Billow noise (abs of noise)
    float turbulence(float x, float z, int octaves) const; // Turbulent noise
    float domainWarp(float& x, float& z) const;
    float lerp(float a, float b, float t) const;
    float fade(float t) const;
    float grad(int hash, float x, float y, float z) const;
    
    // Spline helper
    float getSplineHeight(float continentalness, float erosion, float pv) const;
    
    // Biome-related
    float getTemperature(float x, float z) const;
    float getHumidity(float x, float z) const;
    
    // Cave generation
    bool isCave(float x, float y, float z) const;
    
    // Vegetation helpers
    bool hasTree(int x, int z, BiomeType biome) const;
    int getTreeHeight(int x, int z) const;
};
