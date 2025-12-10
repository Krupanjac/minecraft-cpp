#include "Chunk.h"

Chunk::Chunk(const ChunkPos& position)
    : position(position), state(ChunkState::UNLOADED), dirty(false) {
    blocks.fill(Block(BlockType::AIR));
}

Block Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return Block(BlockType::AIR);
    }
    return blocks[getIndex(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, Block block) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return;
    }
    int idx = getIndex(x, y, z);
    if (blocks[idx].getType() != block.getType()) {
        blocks[idx] = block;
        dirty = true;
        modified = true;
    }
}

bool Chunk::isBlockOpaque(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return false;
    }
    return blocks[getIndex(x, y, z)].isOpaque();
}
