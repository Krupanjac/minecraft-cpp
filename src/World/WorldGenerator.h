#pragma once

#include "../Util/Types.h"
#include "Chunk.h"
#include <memory>

enum class BiomeType {
    OCEAN,
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
    WorldGenerator();
    ~WorldGenerator() = default;

    void generate(std::shared_ptr<Chunk> chunk);
    
    float getNoise(float x, float y, float z) const;
    float getHeight(float x, float z) const;
    BiomeType getBiome(float x, float z) const;
    BiomeInfo getBiomeInfo(BiomeType biome) const;

private:
    unsigned int seed;
    
    // Simple Perlin-like noise
    float noise3D(float x, float y, float z) const;
    float noise2D(float x, float z) const;
    float lerp(float a, float b, float t) const;
    float fade(float t) const;
    
    // Biome-related
    float getTemperature(float x, float z) const;
    float getHumidity(float x, float z) const;
    
    // Cave generation
    bool isCave(float x, float y, float z) const;
};
