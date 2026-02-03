#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <random>

class ChunkManager;
class ExplosionVolumeSystem;
struct Block;

class FireSystem {
public:
    FireSystem();
    
    void igniteBlock(const glm::ivec3& pos, float durationSeconds = 4.0f,
                     bool consumes = true, bool canSpread = true);
    void update(float deltaTime, ChunkManager& chunkManager, ExplosionVolumeSystem* vfx);
    bool isBurning(const glm::ivec3& pos) const;
    void getFireLightPositions(std::vector<glm::vec3>& outPositions, size_t maxCount) const;

    void setSpreadChance(float chance) { spreadChance = chance; }
    void setSpreadInterval(float interval) { spreadInterval = interval; }
    void setFireVfxInterval(float interval) { fireVfxInterval = interval; }

private:
    struct BurningBlock {
        glm::ivec3 pos;
        float age = 0.0f;
        float duration = 4.0f;
        float spreadTimer = 0.0f;
        float vfxTimer = 0.0f;
        bool vfxSpawned = false;
        float vfxActiveTime = 0.0f;
        float vfxDuration = 0.0f;
        bool consumes = true;
        bool canSpread = true;
    };

    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const noexcept {
            size_t h1 = std::hash<int>()(v.x);
            size_t h2 = std::hash<int>()(v.y);
            size_t h3 = std::hash<int>()(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::vector<BurningBlock> burning;
    std::unordered_map<glm::ivec3, size_t, IVec3Hash> index;
    std::mt19937 rng;

    float spreadChance = 0.22f;
    float spreadInterval = 0.8f;
    float fireVfxInterval = 0.6f;      // Used only to delay initial spawn
    int maxVfxPerUpdate = 32;          // Allow multi-sided fire spawns per frame
    int maxSpreadsPerUpdate = 16;      // Reduced from 24 - slower spread
    int maxActiveFireVfx = 160;        // Cap total active fire VFX (higher for multi-sided fire)

    bool isFlammableBlock(const Block& block) const;
    void removeAtIndex(size_t i);
};
