#pragma once

#include "../World/Block.h"
#include "PhysicsTypes.h"
#include <array>

namespace Physics {

// ============================================================================
// Block Physics Data - Physical properties of each block type
// ============================================================================
struct BlockPhysicsData {
    float hardness = 1.0f;          // Time multiplier to break (0 = instant, -1 = unbreakable)
    float blastResistance = 1.0f;   // Resistance to explosions
    float mass = 1.0f;              // Mass when converted to debris (kg per block)
    float friction = 0.6f;          // Surface friction
    float restitution = 0.2f;       // Bounciness
    
    // Destruction properties
    bool canExplode = false;        // Does this block explode (TNT, beds in nether, etc)
    float explosionPower = 0.0f;    // Power if it explodes
    bool dropsSelf = true;          // Does the block drop itself when broken?
    bool fragile = false;           // Breaks easily (glass, ice)
    
    // Structural properties
    bool isFluid = false;           // Water, lava
    bool isSupport = true;          // Can support other blocks above
    float structuralStrength = 1.0f; // How much weight can it support before collapsing
};

// ============================================================================
// Block Physics Database - Lookup table for block physics properties
// ============================================================================
class BlockPhysicsDatabase {
public:
    static BlockPhysicsDatabase& getInstance() {
        static BlockPhysicsDatabase instance;
        return instance;
    }
    
    const BlockPhysicsData& getData(BlockType type) const {
        size_t index = static_cast<size_t>(type);
        if (index < data.size()) {
            return data[index];
        }
        return defaultData;
    }
    
    PhysicsMaterial getMaterial(BlockType type) const {
        const auto& blockData = getData(type);
        PhysicsMaterial mat;
        mat.mass = blockData.mass;
        mat.friction = blockData.friction;
        mat.restitution = blockData.restitution;
        mat.hardness = blockData.hardness;
        mat.blastResistance = blockData.blastResistance;
        return mat;
    }
    
    // Check if block can be destroyed by explosion with given power
    bool canBeDestroyedByExplosion(BlockType type, float explosionPower) const {
        const auto& blockData = getData(type);
        if (blockData.hardness < 0.0f) return false; // Unbreakable
        return explosionPower >= blockData.blastResistance;
    }
    
    // Get time to break block with given tool efficiency
    float getBreakTime(BlockType type, float toolEfficiency = 1.0f) const {
        const auto& blockData = getData(type);
        if (blockData.hardness < 0.0f) return -1.0f; // Unbreakable
        if (blockData.hardness == 0.0f) return 0.0f; // Instant
        return blockData.hardness * 1.5f / toolEfficiency;
    }
    
private:
    BlockPhysicsDatabase() {
        initializeData();
    }
    
    void initializeData();
    
    std::array<BlockPhysicsData, static_cast<size_t>(BlockType::BLOCK_TYPE_COUNT)> data;
    BlockPhysicsData defaultData;
};

// ============================================================================
// Initialization of block physics data
// ============================================================================
inline void BlockPhysicsDatabase::initializeData() {
    // Default data
    defaultData = { 1.0f, 1.0f, 1.0f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 1.0f };
    
    // Initialize all to default
    for (auto& d : data) {
        d = defaultData;
    }
    
    // Air
    data[static_cast<size_t>(BlockType::AIR)] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0.0f, false, false, false, false, 0.0f
    };
    
    // Stone variants - hard and heavy
    data[static_cast<size_t>(BlockType::STONE)] = {
        1.5f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 10.0f
    };
    data[static_cast<size_t>(BlockType::COBBLESTONE)] = {
        2.0f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 8.0f
    };
    data[static_cast<size_t>(BlockType::STONE_BRICKS)] = {
        1.5f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 10.0f
    };
    data[static_cast<size_t>(BlockType::MOSSY_COBBLESTONE)] = {
        2.0f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 7.0f
    };
    data[static_cast<size_t>(BlockType::MOSSY_STONE_BRICKS)] = {
        1.5f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 9.0f
    };
    data[static_cast<size_t>(BlockType::CRACKED_STONE_BRICKS)] = {
        1.5f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 8.0f
    };
    data[static_cast<size_t>(BlockType::CHISELED_STONE_BRICKS)] = {
        1.5f, 6.0f, 2.5f, 0.7f, 0.1f, false, 0.0f, true, false, false, true, 10.0f
    };
    
    // Dirt/Earth - soft and light
    data[static_cast<size_t>(BlockType::DIRT)] = {
        0.5f, 0.5f, 1.2f, 0.8f, 0.1f, false, 0.0f, true, false, false, true, 2.0f
    };
    data[static_cast<size_t>(BlockType::GRASS)] = {
        0.6f, 0.6f, 1.2f, 0.8f, 0.1f, false, 0.0f, false, false, false, true, 2.0f
    };
    data[static_cast<size_t>(BlockType::FARMLAND)] = {
        0.6f, 0.6f, 1.2f, 0.8f, 0.1f, false, 0.0f, false, false, false, true, 1.5f
    };
    
    // Sand/Gravel - falls with gravity
    data[static_cast<size_t>(BlockType::SAND)] = {
        0.5f, 0.5f, 1.5f, 0.7f, 0.05f, false, 0.0f, true, false, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::GRAVEL)] = {
        0.6f, 0.6f, 1.8f, 0.6f, 0.1f, false, 0.0f, false, false, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::SANDSTONE)] = {
        0.8f, 0.8f, 2.0f, 0.6f, 0.15f, false, 0.0f, true, false, false, true, 4.0f
    };
    data[static_cast<size_t>(BlockType::CHISELED_SANDSTONE)] = {
        0.8f, 0.8f, 2.0f, 0.6f, 0.15f, false, 0.0f, true, false, false, true, 4.0f
    };
    data[static_cast<size_t>(BlockType::CLAY)] = {
        0.6f, 0.6f, 1.8f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 3.0f
    };
    
    // Wood - medium hardness, flammable
    data[static_cast<size_t>(BlockType::WOOD)] = {
        2.0f, 3.0f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::LOG)] = {
        2.0f, 2.0f, 0.9f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 6.0f
    };
    data[static_cast<size_t>(BlockType::OAK_PLANKS)] = {
        2.0f, 3.0f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::SPRUCE_PLANKS)] = {
        2.0f, 3.0f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::BIRCH_PLANKS)] = {
        2.0f, 3.0f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::JUNGLE_PLANKS)] = {
        2.0f, 3.0f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::OAK_LOG)] = {
        2.0f, 2.0f, 0.9f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 6.0f
    };
    data[static_cast<size_t>(BlockType::SPRUCE_LOG)] = {
        2.0f, 2.0f, 0.9f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 6.0f
    };
    data[static_cast<size_t>(BlockType::BIRCH_LOG)] = {
        2.0f, 2.0f, 0.9f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 6.0f
    };
    data[static_cast<size_t>(BlockType::JUNGLE_LOG)] = {
        2.0f, 2.0f, 0.9f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 6.0f
    };
    
    // Leaves - very soft and light
    data[static_cast<size_t>(BlockType::LEAVES)] = {
        0.2f, 0.2f, 0.1f, 0.4f, 0.1f, false, 0.0f, false, true, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::OAK_LEAVES)] = {
        0.2f, 0.2f, 0.1f, 0.4f, 0.1f, false, 0.0f, false, true, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::SPRUCE_LEAVES)] = {
        0.2f, 0.2f, 0.1f, 0.4f, 0.1f, false, 0.0f, false, true, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::BIRCH_LEAVES)] = {
        0.2f, 0.2f, 0.1f, 0.4f, 0.1f, false, 0.0f, false, true, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::JUNGLE_LEAVES)] = {
        0.2f, 0.2f, 0.1f, 0.4f, 0.1f, false, 0.0f, false, true, false, false, 0.0f
    };
    
    // Glass - fragile
    data[static_cast<size_t>(BlockType::GLASS)] = {
        0.3f, 0.3f, 2.5f, 0.4f, 0.1f, false, 0.0f, false, true, false, true, 1.0f
    };
    
    // Ice - slippery and fragile
    data[static_cast<size_t>(BlockType::ICE)] = {
        0.5f, 0.5f, 0.9f, 0.02f, 0.3f, false, 0.0f, false, true, false, true, 2.0f
    };
    
    // Snow - very soft
    data[static_cast<size_t>(BlockType::SNOW)] = {
        0.1f, 0.1f, 0.3f, 0.5f, 0.05f, false, 0.0f, true, false, false, false, 0.0f
    };
    
    // Ores - valuable, medium-hard
    data[static_cast<size_t>(BlockType::COAL_ORE)] = {
        3.0f, 3.0f, 2.7f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 8.0f
    };
    data[static_cast<size_t>(BlockType::IRON_ORE)] = {
        3.0f, 3.0f, 3.5f, 0.7f, 0.15f, false, 0.0f, true, false, false, true, 9.0f
    };
    data[static_cast<size_t>(BlockType::GOLD_ORE)] = {
        3.0f, 3.0f, 4.0f, 0.65f, 0.2f, false, 0.0f, true, false, false, true, 9.0f
    };
    data[static_cast<size_t>(BlockType::DIAMOND_ORE)] = {
        3.0f, 3.0f, 3.5f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 10.0f
    };
    data[static_cast<size_t>(BlockType::EMERALD_ORE)] = {
        3.0f, 3.0f, 3.5f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 10.0f
    };
    data[static_cast<size_t>(BlockType::REDSTONE_ORE)] = {
        3.0f, 3.0f, 3.0f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 8.0f
    };
    data[static_cast<size_t>(BlockType::LAPIS_ORE)] = {
        3.0f, 3.0f, 3.0f, 0.7f, 0.1f, false, 0.0f, false, false, false, true, 8.0f
    };
    
    // Mineral blocks - hard, heavy
    data[static_cast<size_t>(BlockType::IRON_BLOCK)] = {
        5.0f, 6.0f, 7.8f, 0.5f, 0.3f, false, 0.0f, true, false, false, true, 15.0f
    };
    data[static_cast<size_t>(BlockType::GOLD_BLOCK)] = {
        3.0f, 6.0f, 19.3f, 0.4f, 0.35f, false, 0.0f, true, false, false, true, 12.0f
    };
    data[static_cast<size_t>(BlockType::DIAMOND_BLOCK)] = {
        5.0f, 6.0f, 3.5f, 0.5f, 0.2f, false, 0.0f, true, false, false, true, 20.0f
    };
    data[static_cast<size_t>(BlockType::EMERALD_BLOCK)] = {
        5.0f, 6.0f, 2.8f, 0.5f, 0.2f, false, 0.0f, true, false, false, true, 20.0f
    };
    data[static_cast<size_t>(BlockType::REDSTONE_BLOCK)] = {
        5.0f, 6.0f, 5.0f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 12.0f
    };
    
    // Bricks - durable building material
    data[static_cast<size_t>(BlockType::BRICKS)] = {
        2.0f, 6.0f, 2.2f, 0.7f, 0.15f, false, 0.0f, true, false, false, true, 8.0f
    };
    
    // Obsidian - extremely hard, explosion resistant
    data[static_cast<size_t>(BlockType::OBSIDIAN)] = {
        50.0f, 1200.0f, 2.6f, 0.6f, 0.05f, false, 0.0f, true, false, false, true, 100.0f
    };
    
    // Bedrock - unbreakable
    data[static_cast<size_t>(BlockType::BEDROCK)] = {
        -1.0f, 3600000.0f, 10.0f, 0.8f, 0.0f, false, 0.0f, false, false, false, true, 99999.0f
    };
    
    // TNT - explosive!
    data[static_cast<size_t>(BlockType::TNT)] = {
        0.0f, 0.0f, 1.0f, 0.6f, 0.1f, true, 4.0f, true, false, false, true, 1.0f
    };
    
    // Water - fluid
    data[static_cast<size_t>(BlockType::WATER)] = {
        100.0f, 100.0f, 1.0f, 0.0f, 0.0f, false, 0.0f, false, false, true, false, 0.0f
    };
    
    // Glowstone - fragile light source
    data[static_cast<size_t>(BlockType::GLOWSTONE)] = {
        0.3f, 0.3f, 1.5f, 0.5f, 0.1f, false, 0.0f, false, true, false, true, 2.0f
    };
    
    // Wool - soft, flammable
    data[static_cast<size_t>(BlockType::WHITE_WOOL)] = {
        0.8f, 0.8f, 0.3f, 0.8f, 0.1f, false, 0.0f, true, false, false, true, 1.0f
    };
    // Apply same properties to all wool colors
    for (int i = static_cast<int>(BlockType::ORANGE_WOOL); 
         i <= static_cast<int>(BlockType::BLACK_WOOL); i++) {
        data[i] = data[static_cast<size_t>(BlockType::WHITE_WOOL)];
    }
    
    // Sponge - very soft
    data[static_cast<size_t>(BlockType::SPONGE)] = {
        0.6f, 0.6f, 0.2f, 0.6f, 0.4f, false, 0.0f, true, false, false, true, 1.0f
    };
    
    // Plants - instant break
    data[static_cast<size_t>(BlockType::TALL_GRASS)] = {
        0.0f, 0.0f, 0.01f, 0.3f, 0.0f, false, 0.0f, false, false, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::ROSE)] = {
        0.0f, 0.0f, 0.01f, 0.3f, 0.0f, false, 0.0f, true, false, false, false, 0.0f
    };
    data[static_cast<size_t>(BlockType::SUGAR_CANE)] = {
        0.0f, 0.0f, 0.02f, 0.3f, 0.0f, false, 0.0f, true, false, false, false, 0.0f
    };
    
    // Cobweb
    data[static_cast<size_t>(BlockType::COBWEB)] = {
        4.0f, 4.0f, 0.01f, 0.9f, 0.0f, false, 0.0f, false, false, false, false, 0.0f
    };
    
    // Crafting blocks
    data[static_cast<size_t>(BlockType::CRAFTING_TABLE)] = {
        2.5f, 2.5f, 0.8f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 4.0f
    };
    data[static_cast<size_t>(BlockType::NOTE_BLOCK)] = {
        0.8f, 0.8f, 0.7f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 3.0f
    };
    data[static_cast<size_t>(BlockType::JUKEBOX)] = {
        2.0f, 6.0f, 1.0f, 0.6f, 0.2f, false, 0.0f, true, false, false, true, 5.0f
    };
    data[static_cast<size_t>(BlockType::BOOKSHELF)] = {
        1.5f, 1.5f, 0.6f, 0.6f, 0.2f, false, 0.0f, false, false, false, true, 3.0f
    };
    data[static_cast<size_t>(BlockType::REDSTONE_LAMP)] = {
        0.3f, 0.3f, 1.2f, 0.5f, 0.1f, false, 0.0f, true, false, false, true, 2.0f
    };
}

// ============================================================================
// Helper function to get physics data easily
// ============================================================================
inline const BlockPhysicsData& getBlockPhysics(BlockType type) {
    return BlockPhysicsDatabase::getInstance().getData(type);
}

inline PhysicsMaterial getBlockMaterial(BlockType type) {
    return BlockPhysicsDatabase::getInstance().getMaterial(type);
}

} // namespace Physics
