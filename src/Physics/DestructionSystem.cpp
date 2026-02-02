#include "DestructionSystem.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace Physics {

static std::random_device rd;
static std::mt19937 gen(rd());

DestructionSystem::DestructionSystem(PhysicsWorld& physicsWorld)
    : physicsWorld(physicsWorld)
{
}

void DestructionSystem::breakBlock(const glm::ivec3& position, float damage,
                                    const glm::vec3& impulseDir) {
    if (!blockQuery || !blockSet) return;
    
    Block block = blockQuery(position.x, position.y, position.z);
    if (block.type == BlockType::AIR) return;
    
    const auto& physics = getBlockPhysics(block.type);
    
    // Check if damage is enough to break
    if (physics.hardness < 0.0f) {
        return; // Unbreakable (bedrock)
    }
    
    // Queue destruction event
    DestructionEvent event;
    event.position = position;
    event.blockType = block.type;
    event.damage = damage;
    event.impulseDirection = impulseDir;
    event.createDebris = config.enableDebris && physics.mass > 0.01f;
    
    pendingDestructions.push(event);
}

void DestructionSystem::damageBlock(const glm::ivec3& position, float damage) {
    if (!blockQuery) return;
    
    Block block = blockQuery(position.x, position.y, position.z);
    if (block.type == BlockType::AIR) return;
    
    const auto& physics = getBlockPhysics(block.type);
    if (physics.hardness < 0.0f) return; // Unbreakable
    
    uint64_t key = positionToKey(position);
    blockDamage[key] += damage;
    
    // Calculate damage threshold based on hardness
    float threshold = physics.hardness * 100.0f;
    
    if (blockDamage[key] >= threshold) {
        breakBlock(position, blockDamage[key], glm::vec3(0.0f, 0.0f, 0.0f));
        blockDamage.erase(key);
    }
}

void DestructionSystem::damageRadius(const glm::vec3& center, float radius, float maxDamage,
                                      const glm::vec3& impulseDir) {
    if (!blockQuery) return;
    
    int minX = static_cast<int>(std::floor(center.x - radius));
    int maxX = static_cast<int>(std::ceil(center.x + radius));
    int minY = static_cast<int>(std::floor(center.y - radius));
    int maxY = static_cast<int>(std::ceil(center.y + radius));
    int minZ = static_cast<int>(std::floor(center.z - radius));
    int maxZ = static_cast<int>(std::ceil(center.z + radius));
    
    float radiusSq = radius * radius;
    
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                glm::vec3 blockCenter(x + 0.5f, y + 0.5f, z + 0.5f);
                float distSq = glm::dot(blockCenter - center, blockCenter - center);
                
                if (distSq <= radiusSq) {
                    float dist = std::sqrt(distSq);
                    float falloff = 1.0f - (dist / radius);
                    float damage = maxDamage * falloff * falloff; // Quadratic falloff
                    
                    glm::vec3 impulse = impulseDir;
                    if (glm::length(impulse) < 0.01f && dist > 0.01f) {
                        impulse = glm::normalize(blockCenter - center);
                    }
                    
                    Block block = blockQuery(x, y, z);
                    if (block.type != BlockType::AIR) {
                        const auto& physics = getBlockPhysics(block.type);
                        
                        if (damage >= physics.blastResistance) {
                            breakBlock(glm::ivec3(x, y, z), damage, impulse);
                        }
                    }
                }
            }
        }
    }
}

FracturePattern DestructionSystem::getFracturePattern(BlockType type, float damage) const {
    const auto& physics = getBlockPhysics(type);
    FracturePattern pattern;
    
    // Fragile materials shatter
    if (physics.fragile) {
        pattern.type = FracturePattern::Type::Shatter;
        pattern.minPieces = 3;
        pattern.maxPieces = 6;
        pattern.minPieceScale = 0.15f;
        pattern.maxPieceScale = 0.35f;
        pattern.velocityScale = 1.5f;
        pattern.angularVelocityScale = 2.0f;
        return pattern;
    }
    
    // Very high damage causes explosion-style breakup
    if (damage > physics.blastResistance * 3.0f) {
        pattern.type = FracturePattern::Type::Explode;
        pattern.minPieces = 4;
        pattern.maxPieces = 8;
        pattern.minPieceScale = 0.2f;
        pattern.maxPieceScale = 0.4f;
        pattern.velocityScale = 2.0f;
        pattern.angularVelocityScale = 3.0f;
        return pattern;
    }
    
    // Soft materials crumble
    if (physics.hardness < 0.5f) {
        pattern.type = FracturePattern::Type::Crumble;
        pattern.minPieces = 2;
        pattern.maxPieces = 4;
        pattern.minPieceScale = 0.25f;
        pattern.maxPieceScale = 0.5f;
        pattern.velocityScale = 0.5f;
        pattern.angularVelocityScale = 1.0f;
        return pattern;
    }
    
    // Hard materials split
    if (physics.hardness > 2.0f) {
        pattern.type = FracturePattern::Type::Split;
        pattern.minPieces = 2;
        pattern.maxPieces = 3;
        pattern.minPieceScale = 0.4f;
        pattern.maxPieceScale = 0.6f;
        pattern.velocityScale = 0.8f;
        pattern.angularVelocityScale = 1.5f;
        return pattern;
    }
    
    // Default: simple breakup
    pattern.type = FracturePattern::Type::Simple;
    pattern.minPieces = 1;
    pattern.maxPieces = 2;
    pattern.minPieceScale = 0.5f;
    pattern.maxPieceScale = 0.8f;
    pattern.velocityScale = 0.6f;
    pattern.angularVelocityScale = 1.0f;
    
    return pattern;
}

void DestructionSystem::createDebris(const glm::ivec3& position, BlockType type,
                                      const glm::vec3& impulseDir, const FracturePattern& pattern) {
    if (!debrisSpawn) return;
    
    std::uniform_int_distribution<int> pieceDist(pattern.minPieces, pattern.maxPieces);
    std::uniform_real_distribution<float> scaleDist(pattern.minPieceScale, pattern.maxPieceScale);
    std::uniform_real_distribution<float> offsetDist(-0.3f, 0.3f);
    std::uniform_real_distribution<float> angleDist(-3.14159f, 3.14159f);
    std::uniform_real_distribution<float> spreadDist(-1.0f, 1.0f);
    
    int numPieces = std::min(pieceDist(gen), config.maxDebrisPerBlock);
    
    const auto& physics = getBlockPhysics(type);
    
    for (int i = 0; i < numPieces; i++) {
        // Random position within block
        glm::vec3 pos(
            position.x + 0.5f + offsetDist(gen),
            position.y + 0.5f + offsetDist(gen),
            position.z + 0.5f + offsetDist(gen)
        );
        
        // Random scale
        float scale = scaleDist(gen);
        scale = std::clamp(scale, config.debrisMinScale, config.debrisMaxScale);
        
        // Velocity based on impulse direction + randomness
        glm::vec3 velocity = impulseDir * pattern.velocityScale;
        
        // Add spread
        velocity.x += spreadDist(gen) * 2.0f;
        velocity.y += spreadDist(gen) * 2.0f + 3.0f; // Add upward component
        velocity.z += spreadDist(gen) * 2.0f;
        
        velocity *= pattern.velocityScale;
        
        // Random angular velocity
        glm::vec3 angularVel(
            angleDist(gen) * pattern.angularVelocityScale,
            angleDist(gen) * pattern.angularVelocityScale,
            angleDist(gen) * pattern.angularVelocityScale
        );
        
        debrisSpawn(pos, type, velocity, angularVel, scale);
    }
}

bool DestructionSystem::wouldCauseCollapse(const glm::ivec3& position) const {
    if (!config.enableStructuralIntegrity || !blockQuery) return false;
    
    // Check blocks above
    for (int y = position.y + 1; y < position.y + 16; y++) {
        Block above = blockQuery(position.x, y, position.z);
        if (above.type == BlockType::AIR) break;
        
        const auto& physics = getBlockPhysics(above.type);
        if (!physics.isSupport) continue;
        
        // Check if this block has other support
        bool hasOtherSupport = false;
        
        // Check adjacent blocks at same level
        const glm::ivec3 neighbors[] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
        };
        
        for (const auto& offset : neighbors) {
            glm::ivec3 neighbor(position.x + offset.x, y + offset.y, position.z + offset.z);
            
            // Skip the block we're removing
            if (neighbor.x == position.x && neighbor.y == position.y && neighbor.z == position.z) {
                continue;
            }
            
            Block neighborBlock = blockQuery(neighbor.x, neighbor.y, neighbor.z);
            if (neighborBlock.type != BlockType::AIR) {
                const auto& neighborPhysics = getBlockPhysics(neighborBlock.type);
                if (neighborPhysics.isSupport) {
                    hasOtherSupport = true;
                    break;
                }
            }
        }
        
        if (!hasOtherSupport) {
            return true;
        }
    }
    
    return false;
}

void DestructionSystem::updateStructure(const glm::ivec3& changedPosition) {
    if (!config.enableStructuralIntegrity) return;
    
    // Check for unsupported blocks in a radius
    checkStructuralSupport(changedPosition);
}

void DestructionSystem::checkStructuralSupport(const glm::ivec3& startPos) {
    if (!blockQuery) return;
    
    // Simple check: look for floating blocks above the changed position
    for (int dy = 1; dy <= 32; dy++) {
        int y = startPos.y + dy;
        
        for (int dx = -2; dx <= 2; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
                int x = startPos.x + dx;
                int z = startPos.z + dz;
                
                Block block = blockQuery(x, y, z);
                if (block.type == BlockType::AIR) continue;
                
                const auto& physics = getBlockPhysics(block.type);
                if (!physics.isSupport) continue; // Blocks that don't need support
                
                // Check if block has support below or to the side
                bool supported = false;
                
                // Check directly below
                Block below = blockQuery(x, y - 1, z);
                if (below.type != BlockType::AIR) {
                    const auto& belowPhysics = getBlockPhysics(below.type);
                    if (belowPhysics.isSupport) {
                        supported = true;
                    }
                }
                
                // Check adjacent blocks
                if (!supported) {
                    const glm::ivec3 neighbors[] = {
                        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
                    };
                    
                    for (const auto& offset : neighbors) {
                        Block neighbor = blockQuery(x + offset.x, y + offset.y, z + offset.z);
                        if (neighbor.type != BlockType::AIR) {
                            const auto& neighborPhysics = getBlockPhysics(neighbor.type);
                            if (neighborPhysics.isSupport) {
                                // Additional check: does this neighbor have support?
                                Block neighborBelow = blockQuery(x + offset.x, y - 1, z + offset.z);
                                if (neighborBelow.type != BlockType::AIR) {
                                    supported = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                
                if (!supported) {
                    pendingCollapses.push_back(glm::ivec3(x, y, z));
                }
            }
        }
    }
}

void DestructionSystem::processCollapses() {
    if (pendingCollapses.empty()) return;
    
    // Process one collapse at a time for visual effect
    if (collapseTimer > 0.0f) return;
    
    glm::ivec3 pos = pendingCollapses.front();
    pendingCollapses.erase(pendingCollapses.begin());
    
    if (blockQuery) {
        Block block = blockQuery(pos.x, pos.y, pos.z);
        if (block.type != BlockType::AIR) {
            breakBlock(pos, 50.0f, glm::vec3(0.0f, -1.0f, 0.0f));
        }
    }
    
    collapseTimer = config.collapseDelay;
}

float DestructionSystem::calculateStress(const glm::ivec3& position) const {
    if (!blockQuery) return 0.0f;
    
    float stress = 0.0f;
    
    // Count weight of blocks above
    for (int y = position.y + 1; y < position.y + 32; y++) {
        Block above = blockQuery(position.x, y, position.z);
        if (above.type == BlockType::AIR) break;
        
        const auto& physics = getBlockPhysics(above.type);
        stress += physics.mass;
    }
    
    return stress;
}

bool DestructionSystem::isBlockSupported(const glm::ivec3& position) const {
    if (!blockQuery) return true;
    
    // Check if connected to ground through chain of supporting blocks
    std::queue<glm::ivec3> toCheck;
    std::unordered_set<uint64_t> visited;
    
    toCheck.push(position);
    
    while (!toCheck.empty()) {
        glm::ivec3 current = toCheck.front();
        toCheck.pop();
        
        uint64_t key = positionToKey(current);
        if (visited.count(key)) continue;
        visited.insert(key);
        
        // At ground level = supported
        if (current.y <= 0) return true;
        
        Block block = blockQuery(current.x, current.y, current.z);
        if (block.type == BlockType::AIR) continue;
        
        // Bedrock is always supported
        if (block.type == BlockType::BEDROCK) return true;
        
        // Check below
        Block below = blockQuery(current.x, current.y - 1, current.z);
        if (below.type != BlockType::AIR) {
            const auto& belowPhysics = getBlockPhysics(below.type);
            if (belowPhysics.isSupport) {
                toCheck.push(glm::ivec3(current.x, current.y - 1, current.z));
            }
        }
    }
    
    return false;
}

std::vector<glm::ivec3> DestructionSystem::getSupportingNeighbors(const glm::ivec3& position) const {
    std::vector<glm::ivec3> neighbors;
    
    if (!blockQuery) return neighbors;
    
    const glm::ivec3 offsets[] = {
        {0, -1, 0},  // Below (primary support)
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}  // Adjacent
    };
    
    for (const auto& offset : offsets) {
        glm::ivec3 neighbor = position + offset;
        Block block = blockQuery(neighbor.x, neighbor.y, neighbor.z);
        
        if (block.type != BlockType::AIR) {
            const auto& physics = getBlockPhysics(block.type);
            if (physics.isSupport) {
                neighbors.push_back(neighbor);
            }
        }
    }
    
    return neighbors;
}

void DestructionSystem::update(float deltaTime) {
    // Update collapse timer
    if (collapseTimer > 0.0f) {
        collapseTimer -= deltaTime;
    }
    
    // Process pending collapses
    processCollapses();
    
    // Process pending destruction events
    while (!pendingDestructions.empty()) {
        DestructionEvent event = pendingDestructions.front();
        pendingDestructions.pop();
        
        // Remove the block from the world
        if (blockSet) {
            blockSet(event.position.x, event.position.y, event.position.z, Block(BlockType::AIR));
        }
        
        // Create debris
        if (event.createDebris) {
            FracturePattern pattern = getFracturePattern(event.blockType, event.damage);
            createDebris(event.position, event.blockType, event.impulseDirection, pattern);
        }
        
        // Check for TNT chain reaction
        const auto& physics = getBlockPhysics(event.blockType);
        if (physics.canExplode) {
            // Trigger explosion (handled by ExplosionSystem)
            // For now, just apply radial damage
            glm::vec3 center(event.position.x + 0.5f, event.position.y + 0.5f, event.position.z + 0.5f);
            damageRadius(center, physics.explosionPower * 2.0f, physics.explosionPower * 100.0f, glm::vec3(0.0f));
        }
        
        // Update structural integrity
        if (config.enableStructuralIntegrity) {
            updateStructure(event.position);
        }
    }
}

} // namespace Physics
