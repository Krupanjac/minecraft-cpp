#pragma once

#include "../Util/Types.h"
#include "Chunk.h"
#include <memory>

class WorldGenerator {
public:
    WorldGenerator();
    ~WorldGenerator() = default;

    void generate(std::shared_ptr<Chunk> chunk);
    
    float getNoise(float x, float y, float z) const;
    float getHeight(float x, float z) const;

private:
    unsigned int seed;
    
    // Simple Perlin-like noise
    float noise3D(float x, float y, float z) const;
    float noise2D(float x, float z) const;
    float lerp(float a, float b, float t) const;
    float fade(float t) const;
};
