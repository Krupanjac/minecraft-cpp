#pragma once

#include "../Util/Types.h"
#include "../Util/Config.h"
#include "Block.h"
#include <array>
#include <memory>
#include <atomic>

enum class ChunkState {
    UNLOADED,
    GENERATING,
    MESH_BUILD,
    READY,
    GPU_UPLOADED
};

class Chunk {
public:
    Chunk(const ChunkPos& position);
    ~Chunk() = default;

    const ChunkPos& getPosition() const { return position; }
    ChunkState getState() const { return state.load(); }
    void setState(ChunkState newState) { state.store(newState); }

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);
    
    bool isBlockOpaque(int x, int y, int z) const;
    
    const std::array<Block, CHUNK_VOLUME>& getBlocks() const { return blocks; }
    std::array<Block, CHUNK_VOLUME>& getBlocks() { return blocks; }

    bool isDirty() const { return dirty; }
    void setDirty(bool value) { dirty = value; }

    bool isModified() const { return modified; }
    void setModified(bool value) { modified = value; }

    int getCurrentLOD() const { return currentLOD; }
    void setCurrentLOD(int lod) { currentLOD = lod; }

private:
    ChunkPos position;
    std::array<Block, CHUNK_VOLUME> blocks;
    std::atomic<ChunkState> state;
    bool dirty;
    bool modified = false;
    int currentLOD = 0;

    static int getIndex(int x, int y, int z) {
        return y * CHUNK_AREA + z * CHUNK_SIZE + x;
    }
};
