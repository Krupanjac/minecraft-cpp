#pragma once

#include "../Util/Types.h"
#include "../World/Chunk.h"
#include "Vertex.h"
#include <vector>
#include <memory>

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<Vertex> waterVertices;
    std::vector<u32> waterIndices;
    
    void clear() {
        vertices.clear();
        indices.clear();
        waterVertices.clear();
        waterIndices.clear();
    }
    
    bool isEmpty() const {
        return vertices.empty() && waterVertices.empty();
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
                           std::shared_ptr<Chunk> chunkZNeg,
                           int lod = 0);

private:
    struct Quad {
        int x, y, z;
        int w, h;
        int u_axis, v_axis;
        int nx, ny, nz;
        u8 normal;
        u8 material;
        u8 ao[4]; // AO for each vertex
    };
    
    void greedyMesh(std::shared_ptr<Chunk> chunk,
                   std::shared_ptr<Chunk> neighbors[6],
                   MeshData& meshData,
                   int lod);
    
    bool isBlockSolid(std::shared_ptr<Chunk> chunk, int x, int y, int z,
                     std::shared_ptr<Chunk> neighbors[6]);
                     
    u8 calculateVertexAO(std::shared_ptr<Chunk> chunk, int x, int y, int z, 
                        const int* u, const int* v,
                        std::shared_ptr<Chunk> neighbors[6]);
    
    void addQuad(const Quad& quad, MeshData& meshData);
    
    void addCross(int x, int y, int z, u8 material, u8 ao, MeshData& meshData);
};
