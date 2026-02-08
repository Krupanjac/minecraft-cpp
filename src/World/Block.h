#pragma once

#include "../Util/Types.h"

enum class BlockType : u8 {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    STONE = 3,
    SAND = 4,
    WATER = 5,
    WOOD = 6,          // Oak planks (legacy)
    LEAVES = 7,        // Oak leaves (legacy)
    SNOW = 8,
    ICE = 9,
    GRAVEL = 10,
    SANDSTONE = 11,
    LOG = 12,          // Oak log (legacy)
    TALL_GRASS = 13,
    ROSE = 14,         // Dandelion/flowers
    BEDROCK = 15,
    
    // Ores
    COBBLESTONE = 16,
    COAL_ORE = 17,
    IRON_ORE = 18,
    GOLD_ORE = 19,
    DIAMOND_ORE = 20,
    EMERALD_ORE = 21,
    REDSTONE_ORE = 22,
    LAPIS_ORE = 23,
    
    // Stone variants
    MOSSY_COBBLESTONE = 25,
    STONE_BRICKS = 26,
    MOSSY_STONE_BRICKS = 27,
    CRACKED_STONE_BRICKS = 28,
    CHISELED_STONE_BRICKS = 29,
    
    // Mineral blocks
    IRON_BLOCK = 33,
    GOLD_BLOCK = 34,
    DIAMOND_BLOCK = 35,
    EMERALD_BLOCK = 36,
    REDSTONE_BLOCK = 37,
    
    // Building blocks
    BRICKS = 38,
    OBSIDIAN = 39,
    GLASS = 41,
    BOOKSHELF = 42,
    TNT = 43,
    GLOWSTONE = 44,
    REDSTONE_LAMP = 46,
    
    // Wood - Planks
    OAK_PLANKS = 47,
    SPRUCE_PLANKS = 48,
    BIRCH_PLANKS = 49,
    JUNGLE_PLANKS = 50,
    
    // Wood - Logs
    OAK_LOG = 55,
    SPRUCE_LOG = 56,
    BIRCH_LOG = 57,
    JUNGLE_LOG = 58,
    
    // Wood - Leaves
    OAK_LEAVES = 61,
    SPRUCE_LEAVES = 62,
    BIRCH_LEAVES = 63,
    JUNGLE_LEAVES = 64,
    ACACIA_LEAVES = 65,
    DARK_OAK_LEAVES = 66,
    
    // Wool colors
    WHITE_WOOL = 67,
    ORANGE_WOOL = 68,
    MAGENTA_WOOL = 69,
    LIGHT_BLUE_WOOL = 70,
    YELLOW_WOOL = 71,
    LIME_WOOL = 72,
    PINK_WOOL = 73,
    GRAY_WOOL = 74,
    LIGHT_GRAY_WOOL = 75,
    CYAN_WOOL = 76,
    PURPLE_WOOL = 77,
    BLUE_WOOL = 78,
    BROWN_WOOL = 79,
    GREEN_WOOL = 80,
    RED_WOOL = 81,
    BLACK_WOOL = 82,
    
    // Sand variants
    CHISELED_SANDSTONE = 102,
    
    // Road blocks (glazed terracotta road surfaces)
    ROAD_STRAIGHT = 83,
    ROAD_LEFT = 84,
    ROAD_RIGHT = 85,
    ROAD_LEFT_RIGHT = 86,
    ROAD_T_JUNCTION = 87,                // left_right_no_forward
    ROAD_INTERSECTION_YELLOW = 88,
    ROAD_MIDDLE_LINES = 89,
    ROAD_MIDDLE_LINES_YELLOW = 90,
    ROAD_MIDDLE_RIGHT = 91,
    ROAD_MIDDLE_RIGHT_YELLOW = 92,
    ROAD_LEFT_DIAG_45 = 93,
    ROAD_LEFT_DIAG_45_YELLOW = 94,
    ROAD_LEFT_DIAG_60 = 95,
    ROAD_LEFT_DIAG_60_YELLOW = 96,
    ROAD_RIGHT_DIAG_60 = 97,
    ROAD_RIGHT_DIAG_YELLOW = 98,
    GLAZED_TERRACOTTA = 99,       // Plain glazed terracotta base (road base block)
    
    // Misc blocks
    CLAY = 104,
    SPONGE = 107,
    COBWEB = 109,
    CRAFTING_TABLE = 110,
    NOTE_BLOCK = 111,
    JUKEBOX = 112,
    
    // Farmland
    FARMLAND = 113,
    
    // Nature
    SUGAR_CANE = 116,
    
    // Block count marker (for iteration)
    BLOCK_TYPE_COUNT = 117
};

struct Block {
    BlockType type;
    u8 data; // Metadata (e.g. water level 0-7)
    
    Block() : type(BlockType::AIR), data(0) {}
    Block(BlockType type, u8 data = 0) : type(type), data(data) {}

    bool operator==(const Block& other) const {
        return type == other.type && data == other.data;
    }

    bool operator!=(const Block& other) const {
        return !(*this == other);
    }

    BlockType getType() const { return type; }
    u8 getData() const { return data; }
    void setData(u8 d) { data = d; }
    
    bool isOpaque() const {
        return type != BlockType::AIR && 
               type != BlockType::WATER && 
               type != BlockType::ICE &&
               type != BlockType::LEAVES &&
               type != BlockType::OAK_LEAVES &&
               type != BlockType::SPRUCE_LEAVES &&
               type != BlockType::BIRCH_LEAVES &&
               type != BlockType::JUNGLE_LEAVES &&
               type != BlockType::ACACIA_LEAVES &&
               type != BlockType::DARK_OAK_LEAVES &&
               type != BlockType::TALL_GRASS &&
               type != BlockType::ROSE &&
               type != BlockType::GLASS &&
               type != BlockType::COBWEB &&
               type != BlockType::SUGAR_CANE;
    }
    
    bool isSolid() const {
        return type != BlockType::AIR && 
               type != BlockType::WATER &&
               type != BlockType::TALL_GRASS &&
               type != BlockType::ROSE &&
               type != BlockType::COBWEB &&
               type != BlockType::SUGAR_CANE;
    }
    
    bool isWater() const {
        return type == BlockType::WATER;
    }
    
    bool isTransparent() const {
        return type == BlockType::WATER || 
               type == BlockType::ICE ||
               type == BlockType::LEAVES ||
               type == BlockType::OAK_LEAVES ||
               type == BlockType::SPRUCE_LEAVES ||
               type == BlockType::BIRCH_LEAVES ||
               type == BlockType::JUNGLE_LEAVES ||
               type == BlockType::ACACIA_LEAVES ||
               type == BlockType::DARK_OAK_LEAVES ||
               type == BlockType::TALL_GRASS ||
               type == BlockType::ROSE ||
               type == BlockType::GLASS ||
               type == BlockType::COBWEB ||
               type == BlockType::SUGAR_CANE;
    }
    
    bool isCrossModel() const {
        return type == BlockType::TALL_GRASS || 
               type == BlockType::ROSE ||
               type == BlockType::COBWEB ||
               type == BlockType::SUGAR_CANE;
    }
    
    bool isLeaves() const {
        return type == BlockType::LEAVES ||
               type == BlockType::OAK_LEAVES ||
               type == BlockType::SPRUCE_LEAVES ||
               type == BlockType::BIRCH_LEAVES ||
               type == BlockType::JUNGLE_LEAVES ||
               type == BlockType::ACACIA_LEAVES ||
               type == BlockType::DARK_OAK_LEAVES;
    }
    
    bool isGlass() const {
        return type == BlockType::GLASS;
    }
    
    bool isLog() const {
        return type == BlockType::LOG ||
               type == BlockType::OAK_LOG ||
               type == BlockType::SPRUCE_LOG ||
               type == BlockType::BIRCH_LOG ||
               type == BlockType::JUNGLE_LOG;
    }
    
    bool isPlanks() const {
        return type == BlockType::WOOD ||
               type == BlockType::OAK_PLANKS ||
               type == BlockType::SPRUCE_PLANKS ||
               type == BlockType::BIRCH_PLANKS ||
               type == BlockType::JUNGLE_PLANKS;
    }
    
    bool isFlammable() const {
        // Organic blocks that can catch fire and burn
        return isLeaves() || isLog() || isPlanks() ||
               type == BlockType::GRASS ||       // Grass-topped dirt
               type == BlockType::TALL_GRASS ||  // Tall grass plants
               type == BlockType::ROSE ||        // Flowers
               type == BlockType::COBWEB ||      // Webs burn fast
               type == BlockType::BOOKSHELF;     // Paper burns
    }
    
    bool isLightEmitting() const {
        return type == BlockType::GLOWSTONE ||
               type == BlockType::REDSTONE_LAMP;
    }
    
    u8 getMaterialID() const {
        return static_cast<u8>(type);
    }
};
