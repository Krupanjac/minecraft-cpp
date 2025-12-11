#pragma once

#include "../Util/Types.h"

enum class BlockType : u8 {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    STONE = 3,
    SAND = 4,
    WATER = 5,
    WOOD = 6,
    LEAVES = 7,
    SNOW = 8,
    ICE = 9,
    GRAVEL = 10,
    SANDSTONE = 11,
    LOG = 12,
    TALL_GRASS = 13,
    ROSE = 14,
    BEDROCK = 15
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
               type != BlockType::TALL_GRASS &&
               type != BlockType::ROSE;
    }
    
    bool isSolid() const {
        return type != BlockType::AIR && 
               type != BlockType::WATER &&
               type != BlockType::TALL_GRASS &&
               type != BlockType::ROSE;
    }
    
    bool isWater() const {
        return type == BlockType::WATER;
    }
    
    bool isTransparent() const {
        return type == BlockType::WATER || 
               type == BlockType::ICE ||
               type == BlockType::LEAVES ||
               type == BlockType::TALL_GRASS ||
               type == BlockType::ROSE;
    }
    
    bool isCrossModel() const {
        return type == BlockType::TALL_GRASS || 
               type == BlockType::ROSE;
    }
    
    u8 getMaterialID() const {
        return static_cast<u8>(type);
    }
};
