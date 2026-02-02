#pragma once

#include "PhysicsTypes.h"
#include "PhysicsWorld.h"
#include "BlockPhysics.h"
#include "../World/Block.h"
#include <vector>
#include <functional>
#include <queue>
#include <unordered_set>

namespace Physics {

// Forward declarations
class DebrisEntity;

// ============================================================================
// Destruction Event - Information about a block destruction
// ============================================================================
struct DestructionEvent {
    glm::ivec3 position;
    BlockType blockType;
    float damage;
    glm::vec3 impulseDirection;
    bool createDebris;
};

// ============================================================================
// Structural Node - For tracking structural integrity
// ============================================================================
struct StructuralNode {
    glm::ivec3 position;
    BlockType blockType;
    float stress = 0.0f;           // Current stress on the block
    float maxStress = 1.0f;        // Maximum stress before failure
    bool isSupported = true;       // Connected to ground/immovable block
    bool visited = false;          // For graph traversal
};

// ============================================================================
// Fracture Pattern - Defines how a block breaks apart
// ============================================================================
struct FracturePattern {
    enum class Type {
        Simple,      // Single debris piece
        Split,       // 2-4 pieces
        Shatter,     // Many small pieces
        Crumble,     // Powder/particles
        Explode      // Debris flies outward
    };
    
    Type type = Type::Simple;
    int minPieces = 1;
    int maxPieces = 1;
    float minPieceScale = 0.8f;
    float maxPieceScale = 1.0f;
    float velocityScale = 1.0f;
    float angularVelocityScale = 1.0f;
};

// ============================================================================
// Destruction System - Handles block breaking and debris creation
// ============================================================================
class DestructionSystem {
public:
    // Callback types
    using BlockQueryFunc = std::function<Block(int x, int y, int z)>;
    using BlockSetFunc = std::function<void(int x, int y, int z, Block block)>;
    using DebrisSpawnFunc = std::function<void(const glm::vec3& position, BlockType type,
                                                const glm::vec3& velocity, const glm::vec3& angularVel,
                                                float scale)>;
    
    DestructionSystem(PhysicsWorld& physicsWorld);
    
    // Set callbacks for world interaction
    void setBlockQueryFunc(BlockQueryFunc func) { blockQuery = func; }
    void setBlockSetFunc(BlockSetFunc func) { blockSet = func; }
    void setDebrisSpawnFunc(DebrisSpawnFunc func) { debrisSpawn = func; }
    
    // ========== Block Destruction ==========
    
    // Break a single block
    void breakBlock(const glm::ivec3& position, float damage = 100.0f, 
                    const glm::vec3& impulseDir = glm::vec3(0.0f));
    
    // Damage a block (may not break immediately)
    void damageBlock(const glm::ivec3& position, float damage);
    
    // Apply damage in a radius (for explosions, etc.)
    void damageRadius(const glm::vec3& center, float radius, float maxDamage,
                      const glm::vec3& impulseDir = glm::vec3(0.0f));
    
    // ========== Structural Integrity ==========
    
    // Check if removing a block would cause collapse
    bool wouldCauseCollapse(const glm::ivec3& position) const;
    
    // Update structural integrity after block changes
    void updateStructure(const glm::ivec3& changedPosition);
    
    // Process pending structural collapses
    void processCollapses();
    
    // ========== Fracture Patterns ==========
    
    // Get fracture pattern for a block type
    FracturePattern getFracturePattern(BlockType type, float damage) const;
    
    // ========== Update ==========
    
    // Process pending destruction events
    void update(float deltaTime);
    
    // ========== Configuration ==========
    
    struct Config {
        bool enableStructuralIntegrity = true;
        bool enableDebris = true;
        int maxDebrisPerBlock = 4;
        float debrisLifetime = 5.0f;
        float debrisMinScale = 0.2f;
        float debrisMaxScale = 0.5f;
        float structuralCheckRadius = 16.0f;
        float collapseDelay = 0.1f; // Time between chain collapses
    };
    
    void setConfig(const Config& cfg) { config = cfg; }
    const Config& getConfig() const { return config; }
    
private:
    PhysicsWorld& physicsWorld;
    Config config;
    
    // Callbacks
    BlockQueryFunc blockQuery;
    BlockSetFunc blockSet;
    DebrisSpawnFunc debrisSpawn;
    
    // Pending events
    std::queue<DestructionEvent> pendingDestructions;
    
    // Block damage accumulation (for blocks that take multiple hits)
    std::unordered_map<uint64_t, float> blockDamage;
    
    // Structural integrity cache
    std::unordered_map<uint64_t, StructuralNode> structuralNodes;
    std::vector<glm::ivec3> pendingCollapses;
    float collapseTimer = 0.0f;
    
    // ========== Internal Methods ==========
    
    // Create debris for a broken block
    void createDebris(const glm::ivec3& position, BlockType type, 
                      const glm::vec3& impulseDir, const FracturePattern& pattern);
    
    // Check structural support using flood fill from ground
    void checkStructuralSupport(const glm::ivec3& startPos);
    
    // Convert block position to unique key
    static uint64_t positionToKey(const glm::ivec3& pos) {
        return (static_cast<uint64_t>(pos.x + 30000000) << 40) |
               (static_cast<uint64_t>(pos.y + 512) << 20) |
               (static_cast<uint64_t>(pos.z + 30000000));
    }
    
    // Calculate stress on a block based on blocks above
    float calculateStress(const glm::ivec3& position) const;
    
    // Check if a block is supported (connected to ground)
    bool isBlockSupported(const glm::ivec3& position) const;
    
    // Get neighbors that could provide support
    std::vector<glm::ivec3> getSupportingNeighbors(const glm::ivec3& position) const;
};

} // namespace Physics
