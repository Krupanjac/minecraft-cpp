#pragma once

#include "../Util/Types.h"
#include "../World/Chunk.h"
#include "Vertex.h"
#include <vector>
#include <memory>

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    void clear() {
        vertices.clear();
        indices.clear();
    }
    
    bool isEmpty() const {
        return vertices.empty();
    }
};

class MeshBuilder {
public:
    MeshBuilder() = default;
    ~MeshBuilder() = default;

    // Build mesh with greedy meshing
    MeshData buildChunkMesh(std::shared_ptr<Chunk> chunk, 
                           std::shared_ptr<Chunk> chunkXPos,
                           std::shared_ptr<Chunk> chunkXNeg,
                           std::shared_ptr<Chunk> chunkYPos,
                           std::shared_ptr<Chunk> chunkYNeg,
                           std::shared_ptr<Chunk> chunkZPos,
                           std::shared_ptr<Chunk> chunkZNeg);

private:
    struct Quad {
        int x, y, z;
        int w, h;
        int u_axis, v_axis;
        int nx, ny, nz;
        u8 normal;
        u8 material;
    };
    
    void greedyMesh(std::shared_ptr<Chunk> chunk,
                   std::shared_ptr<Chunk> neighbors[6],
                   MeshData& meshData);
    
    bool isBlockSolid(std::shared_ptr<Chunk> chunk, int x, int y, int z,
                     std::shared_ptr<Chunk> neighbors[6]);
    
    void addQuad(const Quad& quad, MeshData& meshData);
};
