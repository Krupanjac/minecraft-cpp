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
    LEAVES = 7
};

struct Block {
    BlockType type;
    
    Block() : type(BlockType::AIR) {}
    Block(BlockType type) : type(type) {}

    BlockType getType() const { return type; }
    
    bool isOpaque() const {
        return type != BlockType::AIR && type != BlockType::WATER;
    }
    
    bool isSolid() const {
        return type != BlockType::AIR;
    }
    
    u8 getMaterialID() const {
        return static_cast<u8>(type);
    }
};
