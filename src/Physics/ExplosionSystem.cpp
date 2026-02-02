#include "ExplosionSystem.h"
#include "BlockPhysics.h"
#include <algorithm>
#include <cmath>

namespace Physics {

static std::random_device rd;

ExplosionSystem::ExplosionSystem()
    : rng(rd())
{
}

ExplosionResult ExplosionSystem::explode(const ExplosionParams& params) {
    ExplosionResult result;
    
    if (!blockQuery || !blockSet) {
        return result;
    }
    
    stats.totalExplosions++;
    
    float radius = params.getRadius();
    
    // Generate rays for explosion sampling
    std::vector<ExplosionRay> rays;
    generateExplosionRays(rays, config.maxRays);
    
    // Collect affected blocks
    std::unordered_set<uint64_t> processedBlocks;
    std::vector<glm::ivec3> affectedBlocks;
    
    for (const auto& ray : rays) {
        std::vector<glm::ivec3> hitBlocks;
        float remainingIntensity = traceExplosionRay(params.center, ray, radius, hitBlocks);
        
        for (const auto& blockPos : hitBlocks) {
            uint64_t key = (static_cast<uint64_t>(blockPos.x & 0x3FFFFFF) << 38) |
                          (static_cast<uint64_t>(blockPos.y & 0xFFF) << 26) |
                          (static_cast<uint64_t>(blockPos.z & 0x3FFFFFF));
            
            if (processedBlocks.find(key) == processedBlocks.end()) {
                processedBlocks.insert(key);
                affectedBlocks.push_back(blockPos);
            }
        }
    }
    
    // Process block damage
    if (params.breakBlocks) {
        processBlockDamage(params.center, params.power, affectedBlocks, result);
    }

    // Trigger volumetric explosion VFX only if blocks were destroyed
    if (explosionVfx && !result.destroyedBlocks.empty()) {
        explosionVfx(params.center, params.power);
    }
    
    // Only play sound if blocks were actually destroyed or affected
    if (soundPlay && !result.destroyedBlocks.empty()) {
        // Volume scales with explosion power (clamped between 0.5 and 1.5)
        float volume = std::clamp(params.power * 0.25f, 0.5f, 1.5f);
        soundPlay(params.center, "entity.generic.explode", volume);
    }
    
    // Trigger screen shake if blocks were destroyed
    if (screenShake && config.enableScreenShake && !result.destroyedBlocks.empty()) {
        screenShake(params.center, params.power);
    }
    
    // Process entity damage
    if (params.damageEntities && entityDamage) {
        processEntityDamage(params.center, radius, params.power);
    }
    
    // Create debris
    if (params.createDebris && debrisSpawn) {
        createExplosionDebris(params.center, result.destroyedBlocks, params.power, result);
    }
    
    // Spread fire
    if (params.createFire) {
        spreadFire(params.center, affectedBlocks, result);
    }
    
    // Trigger chain reactions
    if (params.chainReaction && currentChainDepth < config.maxChainReactions) {
        triggerChainReactions(params.center, radius, result);
    }
    
    stats.totalBlocksDestroyed += result.destroyedBlocks.size();
    
    return result;
}

void ExplosionSystem::update(float deltaTime) {
    if (pendingExplosions.empty()) return;
    
    std::vector<PendingExplosion> stillPending;
    
    for (auto& pending : pendingExplosions) {
        pending.delay -= deltaTime;
        
        if (pending.delay <= 0.0f) {
            // Detonate!
            currentChainDepth++;
            explode(pending.params);
            currentChainDepth--;
            stats.chainReactionsProcessed++;
        } else {
            stillPending.push_back(pending);
        }
    }
    
    pendingExplosions = std::move(stillPending);
    stats.pendingExplosions = pendingExplosions.size();
}

void ExplosionSystem::queueExplosion(const ExplosionParams& params, float delay) {
    PendingExplosion pending;
    pending.params = params;
    pending.delay = delay;
    pending.sourceType = BlockType::TNT;
    pendingExplosions.push_back(pending);
    stats.pendingExplosions = pendingExplosions.size();
}

void ExplosionSystem::generateExplosionRays(std::vector<ExplosionRay>& rays, int count) {
    rays.clear();
    rays.reserve(count);
    
    // Use fibonacci sphere for uniform distribution
    float goldenRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;
    float angleIncrement = 3.14159f * 2.0f * goldenRatio;
    
    for (int i = 0; i < count; i++) {
        float t = static_cast<float>(i) / static_cast<float>(count);
        float inclination = std::acos(1.0f - 2.0f * t);
        float azimuth = angleIncrement * i;
        
        ExplosionRay ray;
        ray.direction = glm::vec3(
            std::sin(inclination) * std::cos(azimuth),
            std::sin(inclination) * std::sin(azimuth),
            std::cos(inclination)
        );
        
        // Add slight randomness to intensity
        std::uniform_real_distribution<float> dist(0.7f, 1.3f);
        ray.intensity = dist(rng);
        
        rays.push_back(ray);
    }
}

float ExplosionSystem::traceExplosionRay(const glm::vec3& start, const ExplosionRay& ray,
                                          float maxDist, std::vector<glm::ivec3>& hitBlocks) {
    hitBlocks.clear();
    
    if (!config.useRaycasting) {
        // Simple sphere-based approach
        int maxR = static_cast<int>(std::ceil(maxDist));
        for (int r = 0; r <= maxR; r++) {
            glm::vec3 pos = start + ray.direction * static_cast<float>(r);
            hitBlocks.push_back(glm::ivec3(
                static_cast<int>(std::floor(pos.x)),
                static_cast<int>(std::floor(pos.y)),
                static_cast<int>(std::floor(pos.z))
            ));
        }
        return ray.intensity;
    }
    
    // Ray marching with intensity attenuation
    float intensity = ray.intensity;
    float distance = 0.0f;
    
    glm::ivec3 lastBlock(-999999, -999999, -999999);
    
    while (distance < maxDist && intensity > 0.0f) {
        glm::vec3 pos = start + ray.direction * distance;
        
        glm::ivec3 blockPos(
            static_cast<int>(std::floor(pos.x)),
            static_cast<int>(std::floor(pos.y)),
            static_cast<int>(std::floor(pos.z))
        );
        
        // Only process each block once per ray
        if (blockPos != lastBlock) {
            lastBlock = blockPos;
            
            Block block = blockQuery(blockPos.x, blockPos.y, blockPos.z);
            
            if (block.type != BlockType::AIR) {
                hitBlocks.push_back(blockPos);
                
                // Reduce intensity based on blast resistance
                const auto& physics = getBlockPhysics(block.type);
                intensity -= physics.blastResistance * 0.1f;
                
                // Hard blocks stop the ray faster
                if (physics.blastResistance > 100.0f) {
                    intensity -= 0.5f;
                }
            }
        }
        
        distance += config.rayStepSize;
    }
    
    return std::max(0.0f, intensity);
}

void ExplosionSystem::processBlockDamage(const glm::vec3& center, float power,
                                          const std::vector<glm::ivec3>& affectedBlocks,
                                          ExplosionResult& result) {
    std::uniform_real_distribution<float> dropDist(0.0f, 1.0f);
    
    for (const auto& pos : affectedBlocks) {
        Block block = blockQuery(pos.x, pos.y, pos.z);
        if (block.type == BlockType::AIR) continue;
        
        const auto& physics = getBlockPhysics(block.type);
        
        // Unbreakable blocks
        if (physics.hardness < 0.0f) continue;
        
        // Calculate damage based on distance
        float damage = calculateDamageAtPoint(center, glm::vec3(pos) + 0.5f, power);
        
        // Check if block is destroyed
        if (damage >= physics.blastResistance) {
            // Store block type before destroying for debris creation
            result.destroyedBlockTypes.push_back(block.type);
            
            // Destroy block
            blockSet(pos.x, pos.y, pos.z, Block(BlockType::AIR));
            result.destroyedBlocks.push_back(pos);
            
            // Check for TNT chain reaction
            if (physics.canExplode) {
                // Will be handled by triggerChainReactions
            }
        } else if (damage >= physics.blastResistance * 0.5f) {
            // Block is damaged but not destroyed
            result.damagedBlocks.push_back(pos);
        }
    }
}

void ExplosionSystem::processEntityDamage(const glm::vec3& center, float radius, float power) {
    if (!entityDamage) return;
    
    float damage = config.baseDamage * power / 4.0f; // Scale damage
    entityDamage(center, radius * 1.5f, damage, center);
}

void ExplosionSystem::createExplosionDebris(const glm::vec3& center,
                                             const std::vector<glm::ivec3>& destroyedBlocks,
                                             float power, ExplosionResult& result) {
    if (!debrisSpawn) return;
    
    std::uniform_real_distribution<float> spawnChance(0.0f, 1.0f);
    std::uniform_real_distribution<float> scaleDist(0.2f, 0.5f);
    std::uniform_real_distribution<float> angularDist(-5.0f, 5.0f);
    
    // Limit debris for performance
    int maxDebris = std::min(static_cast<int>(destroyedBlocks.size()), 50);
    int created = 0;
    
    for (size_t i = 0; i < destroyedBlocks.size(); i++) {
        if (created >= maxDebris) break;
        
        // Random chance to spawn debris
        if (spawnChance(rng) > 0.4f) continue;
        
        const auto& pos = destroyedBlocks[i];
        
        // Get block type from stored types (matching index)
        BlockType type = BlockType::STONE; // fallback
        if (i < result.destroyedBlockTypes.size()) {
            type = result.destroyedBlockTypes[i];
        }
        
        // Skip debris for non-visible blocks
        if (type == BlockType::AIR || type == BlockType::WATER) continue;
        
        glm::vec3 blockCenter(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
        glm::vec3 direction = blockCenter - center;
        float dist = glm::length(direction);
        
        if (dist > 0.01f) {
            direction = glm::normalize(direction);
        } else {
            direction = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        // Velocity based on explosion power and distance
        float velocityMagnitude = power * 5.0f / (dist + 1.0f);
        glm::vec3 velocity = direction * velocityMagnitude;
        velocity.y += power * 2.0f; // Add upward component
        
        glm::vec3 angularVel(angularDist(rng), angularDist(rng), angularDist(rng));
        float scale = scaleDist(rng);
        
        debrisSpawn(blockCenter, type, velocity, angularVel, scale);
        created++;
        result.debrisCreated++;
    }
}

void ExplosionSystem::spreadFire(const glm::vec3& center,
                                  const std::vector<glm::ivec3>& affectedBlocks,
                                  ExplosionResult& result) {
    std::uniform_real_distribution<float> fireChance(0.0f, 1.0f);
    
    for (const auto& pos : affectedBlocks) {
        Block block = blockQuery(pos.x, pos.y, pos.z);
        
        // Only spread fire on air blocks adjacent to flammable blocks
        if (block.type != BlockType::AIR) continue;
        
        // Check for flammable neighbors
        bool hasFlammableNeighbor = false;
        const glm::ivec3 offsets[] = {
            {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
        };
        
        for (const auto& offset : offsets) {
            glm::ivec3 neighbor = pos + offset;
            Block neighborBlock = blockQuery(neighbor.x, neighbor.y, neighbor.z);
            
            if (isBlockFlammable(neighborBlock.type)) {
                hasFlammableNeighbor = true;
                break;
            }
        }
        
        // Fire spreading disabled - no FIRE block type in current engine
        // if (hasFlammableNeighbor && fireChance(rng) < config.fireSpreadChance) {
        //     blockSet(pos.x, pos.y, pos.z, Block(BlockType::FIRE));
        //     result.fireBlocks.push_back(pos);
        // }
        (void)hasFlammableNeighbor;
        (void)fireChance;
    }
}

void ExplosionSystem::triggerChainReactions(const glm::vec3& center, float radius,
                                             ExplosionResult& result) {
    if (currentChainDepth >= config.maxChainReactions) return;
    
    std::uniform_real_distribution<float> delayDist(
        config.chainReactionMinDelay,
        config.chainReactionMaxDelay
    );
    
    // Check for TNT in affected radius
    int searchRadius = static_cast<int>(std::ceil(radius * 1.5f));
    
    for (int x = -searchRadius; x <= searchRadius; x++) {
        for (int y = -searchRadius; y <= searchRadius; y++) {
            for (int z = -searchRadius; z <= searchRadius; z++) {
                glm::ivec3 pos(
                    static_cast<int>(center.x) + x,
                    static_cast<int>(center.y) + y,
                    static_cast<int>(center.z) + z
                );
                
                float dist = glm::length(glm::vec3(x, y, z));
                if (dist > radius * 1.2f) continue;
                
                Block block = blockQuery(pos.x, pos.y, pos.z);
                const auto& physics = getBlockPhysics(block.type);
                
                if (physics.canExplode && block.type != BlockType::AIR) {
                    // Remove the TNT block
                    blockSet(pos.x, pos.y, pos.z, Block(BlockType::AIR));
                    
                    // Queue explosion with delay
                    ExplosionParams tntParams;
                    tntParams.center = glm::vec3(pos) + 0.5f;
                    tntParams.power = physics.explosionPower;
                    tntParams.chainReaction = true;
                    
                    float delay = delayDist(rng);
                    queueExplosion(tntParams, delay);
                    
                    result.chainReactionsTriggered++;
                }
            }
        }
    }
}

bool ExplosionSystem::isBlockExposed(const glm::ivec3& pos) const {
    if (!blockQuery) return true;
    
    const glm::ivec3 offsets[] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    
    for (const auto& offset : offsets) {
        glm::ivec3 neighbor = pos + offset;
        Block block = blockQuery(neighbor.x, neighbor.y, neighbor.z);
        if (block.type == BlockType::AIR || !block.isSolid()) {
            return true;
        }
    }
    
    return false;
}

bool ExplosionSystem::isBlockFlammable(BlockType type) const {
    // Wood types, leaves, wool, etc.
    switch (type) {
        // Logs
        case BlockType::LOG:
        case BlockType::OAK_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::JUNGLE_LOG:
        // Planks
        case BlockType::WOOD:
        case BlockType::OAK_PLANKS:
        case BlockType::SPRUCE_PLANKS:
        case BlockType::BIRCH_PLANKS:
        case BlockType::JUNGLE_PLANKS:
        // Leaves
        case BlockType::LEAVES:
        case BlockType::OAK_LEAVES:
        case BlockType::SPRUCE_LEAVES:
        case BlockType::BIRCH_LEAVES:
        case BlockType::JUNGLE_LEAVES:
        // Wool
        case BlockType::WHITE_WOOL:
        case BlockType::ORANGE_WOOL:
        case BlockType::MAGENTA_WOOL:
        case BlockType::LIGHT_BLUE_WOOL:
        case BlockType::YELLOW_WOOL:
        case BlockType::LIME_WOOL:
        case BlockType::PINK_WOOL:
        case BlockType::GRAY_WOOL:
        case BlockType::LIGHT_GRAY_WOOL:
        case BlockType::CYAN_WOOL:
        case BlockType::PURPLE_WOOL:
        case BlockType::BLUE_WOOL:
        case BlockType::BROWN_WOOL:
        case BlockType::GREEN_WOOL:
        case BlockType::RED_WOOL:
        case BlockType::BLACK_WOOL:
        // Other flammable
        case BlockType::BOOKSHELF:
            return true;
        default:
            return false;
    }
}

float ExplosionSystem::calculateDamageAtPoint(const glm::vec3& center, const glm::vec3& point,
                                               float power) const {
    float dist = glm::length(point - center);
    float radius = power * 1.5f;
    
    if (dist >= radius) return 0.0f;
    
    // Quadratic falloff
    float falloff = 1.0f - (dist / radius);
    return config.baseDamage * power * falloff * falloff;
}

glm::vec3 ExplosionSystem::calculateKnockback(const glm::vec3& center, const glm::vec3& point,
                                               float power) const {
    glm::vec3 direction = point - center;
    float dist = glm::length(direction);
    
    if (dist < 0.01f) {
        return glm::vec3(0.0f, power * config.knockbackStrength, 0.0f);
    }
    
    direction = glm::normalize(direction);
    float radius = power * 1.5f;
    float falloff = 1.0f - std::min(dist / radius, 1.0f);
    
    return direction * power * falloff * config.knockbackStrength;
}

// ============================================================================
// ExplosionEffects namespace implementation
// ============================================================================

namespace ExplosionEffects {

float calculateExposure(const glm::vec3& center, const AABB& targetBounds,
                        std::function<bool(const glm::vec3&)> isBlockSolid) {
    // Sample multiple points on the target
    int visibleSamples = 0;
    int totalSamples = 0;
    
    glm::vec3 size = targetBounds.max - targetBounds.min;
    
    for (float x = 0.0f; x <= 1.0f; x += 0.5f) {
        for (float y = 0.0f; y <= 1.0f; y += 0.5f) {
            for (float z = 0.0f; z <= 1.0f; z += 0.5f) {
                glm::vec3 samplePoint = targetBounds.min + size * glm::vec3(x, y, z);
                
                // Ray from center to sample point
                glm::vec3 direction = samplePoint - center;
                float dist = glm::length(direction);
                
                if (dist < 0.01f) {
                    visibleSamples++;
                    totalSamples++;
                    continue;
                }
                
                direction = glm::normalize(direction);
                
                // Step through ray
                bool blocked = false;
                for (float t = 0.5f; t < dist - 0.5f; t += 0.5f) {
                    glm::vec3 pos = center + direction * t;
                    if (isBlockSolid(pos)) {
                        blocked = true;
                        break;
                    }
                }
                
                if (!blocked) visibleSamples++;
                totalSamples++;
            }
        }
    }
    
    return totalSamples > 0 ? static_cast<float>(visibleSamples) / totalSamples : 1.0f;
}

std::vector<glm::ivec3> generateCraterShape(const glm::vec3& center, float radius, float depth) {
    std::vector<glm::ivec3> craterBlocks;
    
    int iRadius = static_cast<int>(std::ceil(radius));
    int iDepth = static_cast<int>(std::ceil(depth));
    
    for (int x = -iRadius; x <= iRadius; x++) {
        for (int z = -iRadius; z <= iRadius; z++) {
            float horizontalDist = std::sqrt(static_cast<float>(x * x + z * z));
            if (horizontalDist > radius) continue;
            
            // Calculate crater depth at this point (bowl shape)
            float craterDepth = depth * (1.0f - (horizontalDist / radius) * (horizontalDist / radius));
            
            for (int y = 0; y > -iDepth && y > -craterDepth; y--) {
                craterBlocks.push_back(glm::ivec3(
                    static_cast<int>(center.x) + x,
                    static_cast<int>(center.y) + y,
                    static_cast<int>(center.z) + z
                ));
            }
        }
    }
    
    return craterBlocks;
}

glm::vec3 calculateDropPosition(const glm::ivec3& blockPos) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> offsetDist(-0.25f, 0.25f);
    
    return glm::vec3(
        blockPos.x + 0.5f + offsetDist(rng),
        blockPos.y + 0.5f,
        blockPos.z + 0.5f + offsetDist(rng)
    );
}

} // namespace ExplosionEffects

} // namespace Physics
