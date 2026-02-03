#pragma once

#include "PhysicsTypes.h"
#include "DestructionSystem.h"
#include "../World/Block.h"
#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <functional>
#include <random>

namespace Physics {

/**
 * ExplosionParams - Configuration for an explosion
 */
struct ExplosionParams {
    glm::vec3 center;              // World position of explosion center
    float power = 4.0f;            // Base explosion power (TNT = 4)
    float radius = 0.0f;           // Override radius (0 = calculate from power)
    bool breakBlocks = true;       // Whether to destroy blocks
    bool damageEntities = true;    // Whether to damage entities
    bool createFire = false;       // Whether to create fire blocks
    bool createDebris = true;      // Whether to spawn debris entities
    bool chainReaction = true;     // Whether to trigger TNT chain reactions
    float dropChance = 0.3f;       // Chance for destroyed blocks to drop items
    
    // Calculated values
    float getRadius() const {
        if (radius > 0.0f) return radius;
        return power * 1.5f; // Default radius calculation
    }
};

/**
 * ExplosionRay - Used for raycasting damage
 */
struct ExplosionRay {
    glm::vec3 direction;
    float intensity;
};

/**
 * PendingExplosion - Queued explosion for chain reactions
 */
struct PendingExplosion {
    ExplosionParams params;
    float delay;           // Delay before detonation
    BlockType sourceType;  // What caused this explosion
};

/**
 * ExplosionResult - Data returned after processing an explosion
 */
struct ExplosionResult {
    std::vector<glm::ivec3> destroyedBlocks;
    std::vector<BlockType> destroyedBlockTypes;  // Matching block types for debris textures
    std::vector<glm::ivec3> damagedBlocks;
    std::vector<glm::ivec3> fireBlocks;
    size_t debrisCreated = 0;
    int entitiesDamaged = 0;
    int chainReactionsTriggered = 0;
    float totalDamageDealt = 0.0f;
};

/**
 * ExplosionSystem - Handles explosions, chain reactions, and crater generation
 * 
 * Features:
 * - Spherical explosion with ray-based damage attenuation
 * - Block destruction based on blast resistance
 * - Entity damage with knockback
 * - TNT chain reactions with realistic delays
 * - Fire spread on flammable blocks
 * - Debris creation
 * - Crater shaping with natural falloff
 */
class ExplosionSystem {
public:
    // Callbacks for world interaction
    using BlockQueryFunc = std::function<Block(int, int, int)>;
    using BlockSetFunc = std::function<void(int, int, int, Block)>;
    using EntityDamageFunc = std::function<void(const glm::vec3& pos, float radius, float damage, const glm::vec3& center)>;
    using DebrisSpawnFunc = std::function<void(const glm::vec3& pos, BlockType type, const glm::vec3& vel, const glm::vec3& angVel, float scale)>;
    using SoundPlayFunc = std::function<void(const glm::vec3& pos, const std::string& sound, float volume)>;
    using ScreenShakeFunc = std::function<void(const glm::vec3& explosionPos, float power)>;
    using ExplosionVfxFunc = std::function<void(const glm::vec3& explosionPos, float power)>;
    using FireStartFunc = std::function<void(const glm::ivec3& pos)>;

    ExplosionSystem();
    ~ExplosionSystem() = default;
    
    // Set callbacks
    void setBlockQuery(BlockQueryFunc func) { blockQuery = func; }
    void setBlockSet(BlockSetFunc func) { blockSet = func; }
    void setEntityDamage(EntityDamageFunc func) { entityDamage = func; }
    void setDebrisSpawn(DebrisSpawnFunc func) { debrisSpawn = func; }
    void setSoundPlay(SoundPlayFunc func) { soundPlay = func; }
    void setScreenShake(ScreenShakeFunc func) { screenShake = func; }
    void setExplosionVfx(ExplosionVfxFunc func) { explosionVfx = func; }
    void setFireStart(FireStartFunc func) { fireStart = func; }
    
    // Create an explosion
    ExplosionResult explode(const ExplosionParams& params);
    
    // Update pending explosions (chain reactions)
    void update(float deltaTime);
    
    // Queue an explosion for later (used for chain reactions)
    void queueExplosion(const ExplosionParams& params, float delay = 0.0f);
    
    // Configuration
    struct Config {
        int maxRays = 256;              // Number of rays for explosion sampling
        float rayStepSize = 0.3f;       // Step size for ray marching
        float baseDamage = 100.0f;      // Base damage at center
        float knockbackStrength = 1.0f; // Entity knockback multiplier
        float chainReactionMinDelay = 0.1f;  // Min delay for TNT chain
        float chainReactionMaxDelay = 0.4f;  // Max delay for TNT chain
        bool useRaycasting = true;      // Use raycasting for occlusion
        bool enableScreenShake = true;  // Trigger screen shake
        float fireSpreadChance = 0.2f;  // Chance to spread fire
        int maxChainReactions = 100;    // Prevent infinite chains
    };
    
    void setConfig(const Config& cfg) { config = cfg; }
    const Config& getConfig() const { return config; }
    
    // Statistics
    struct Stats {
        size_t totalExplosions = 0;
        size_t totalBlocksDestroyed = 0;
        size_t chainReactionsProcessed = 0;
        size_t pendingExplosions = 0;
    };
    
    const Stats& getStats() const { return stats; }
    void resetStats() { stats = Stats(); }

private:
    // Callbacks
    BlockQueryFunc blockQuery;
    BlockSetFunc blockSet;
    EntityDamageFunc entityDamage;
    DebrisSpawnFunc debrisSpawn;
    SoundPlayFunc soundPlay;
    ScreenShakeFunc screenShake;
    ExplosionVfxFunc explosionVfx;
    FireStartFunc fireStart;
    
    // Configuration
    Config config;
    
    // Pending explosions (chain reactions)
    std::vector<PendingExplosion> pendingExplosions;
    int currentChainDepth = 0;
    
    // Statistics
    Stats stats;
    
    // Random generator
    std::mt19937 rng;
    
    // Explosion processing
    void generateExplosionRays(std::vector<ExplosionRay>& rays, int count);
    float traceExplosionRay(const glm::vec3& start, const ExplosionRay& ray, float maxDist,
                            std::vector<glm::ivec3>& hitBlocks);
    void processBlockDamage(const glm::vec3& center, float power,
                           const std::vector<glm::ivec3>& affectedBlocks,
                           ExplosionResult& result);
    void processEntityDamage(const glm::vec3& center, float radius, float power);
    void createExplosionDebris(const glm::vec3& center, const std::vector<glm::ivec3>& destroyedBlocks,
                               float power, ExplosionResult& result);
    void spreadFire(const glm::vec3& center, const std::vector<glm::ivec3>& affectedBlocks,
                    ExplosionResult& result);
    void triggerChainReactions(const glm::vec3& center, float radius, ExplosionResult& result);
    
    // Helpers
    bool isBlockExposed(const glm::ivec3& pos) const;
    bool isBlockFlammable(BlockType type) const;
    float calculateDamageAtPoint(const glm::vec3& center, const glm::vec3& point, float power) const;
    glm::vec3 calculateKnockback(const glm::vec3& center, const glm::vec3& point, float power) const;
};

/**
 * Helper functions for explosion effects
 */
namespace ExplosionEffects {

// Calculate exposure factor (how much of the target is visible from center)
float calculateExposure(const glm::vec3& center, const AABB& targetBounds,
                        std::function<bool(const glm::vec3&)> isBlockSolid);

// Generate crater shape
std::vector<glm::ivec3> generateCraterShape(const glm::vec3& center, float radius, float depth);

// Calculate block drop position (with randomness)
glm::vec3 calculateDropPosition(const glm::ivec3& blockPos);

} // namespace ExplosionEffects

} // namespace Physics
