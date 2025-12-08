#include "MeshBuilder.h"
#include "../Util/Config.h"
#include <array>
#include <cstring>

MeshData MeshBuilder::buildChunkMesh(std::shared_ptr<Chunk> chunk,
                                     std::shared_ptr<Chunk> chunkXPos,
                                     std::shared_ptr<Chunk> chunkXNeg,
                                     std::shared_ptr<Chunk> chunkYPos,
                                     std::shared_ptr<Chunk> chunkYNeg,
                                     std::shared_ptr<Chunk> chunkZPos,
                                     std::shared_ptr<Chunk> chunkZNeg) {
    MeshData meshData;
    
    std::shared_ptr<Chunk> neighbors[6] = {
        chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg
    };
    
    greedyMesh(chunk, neighbors, meshData);
    
    return meshData;
}

void MeshBuilder::greedyMesh(std::shared_ptr<Chunk> chunk,
                             std::shared_ptr<Chunk> neighbors[6],
                             MeshData& meshData) {
    // Greedy meshing for each axis and direction
    const int dirs[6][3] = {
        {1, 0, 0}, {-1, 0, 0},  // X+, X-
        {0, 1, 0}, {0, -1, 0},  // Y+, Y-
        {0, 0, 1}, {0, 0, -1}   // Z+, Z-
    };
    
    for (int dir = 0; dir < 6; ++dir) {
        int nx = dirs[dir][0];
        int ny = dirs[dir][1];
        int nz = dirs[dir][2];
        
        // Determine sweep axes
        int u_axis, v_axis, w_axis;
        if (nx != 0) { u_axis = 1; v_axis = 2; w_axis = 0; }
        else if (ny != 0) { u_axis = 0; v_axis = 2; w_axis = 1; }
        else { u_axis = 0; v_axis = 1; w_axis = 2; }
        
        // Create mask for this direction
        std::array<u8, CHUNK_SIZE * CHUNK_SIZE> mask;
        
        for (int d = 0; d < CHUNK_SIZE; ++d) {
            mask.fill(0);
            
            // Build mask
            for (int v = 0; v < CHUNK_SIZE; ++v) {
                for (int u = 0; u < CHUNK_SIZE; ++u) {
                    int x = (w_axis == 0) ? d : (u_axis == 0) ? u : v;
                    int y = (w_axis == 1) ? d : (u_axis == 1) ? u : v;
                    int z = (w_axis == 2) ? d : (u_axis == 2) ? u : v;
                    
                    if (x >= CHUNK_SIZE || y >= CHUNK_HEIGHT || z >= CHUNK_SIZE) continue;
                    
                    Block block = chunk->getBlock(x, y, z);
                    if (!block.isSolid()) continue;
                    
                    // Check if face should be rendered
                    int adjX = x + nx;
                    int adjY = y + ny;
                    int adjZ = z + nz;
                    
                    bool shouldRender = false;
                    if (adjX < 0 || adjX >= CHUNK_SIZE || 
                        adjY < 0 || adjY >= CHUNK_HEIGHT || 
                        adjZ < 0 || adjZ >= CHUNK_SIZE) {
                        // Check neighbor chunk
                        shouldRender = !isBlockSolid(chunk, adjX, adjY, adjZ, neighbors);
                    } else {
                        Block adjBlock = chunk->getBlock(adjX, adjY, adjZ);
                        shouldRender = !adjBlock.isOpaque();
                    }
                    
                    if (shouldRender) {
                        mask[v * CHUNK_SIZE + u] = block.getMaterialID();
                    }
                }
            }
            
            // Generate mesh from mask using greedy algorithm
            for (int v = 0; v < CHUNK_SIZE; ++v) {
                for (int u = 0; u < CHUNK_SIZE; ) {
                    u8 material = mask[v * CHUNK_SIZE + u];
                    if (material == 0) {
                        ++u;
                        continue;
                    }
                    
                    // Compute width
                    int w = 1;
                    while (u + w < CHUNK_SIZE && mask[v * CHUNK_SIZE + u + w] == material) {
                        ++w;
                    }
                    
                    // Compute height
                    int h = 1;
                    bool done = false;
                    while (v + h < CHUNK_SIZE && !done) {
                        for (int k = 0; k < w; ++k) {
                            if (mask[(v + h) * CHUNK_SIZE + u + k] != material) {
                                done = true;
                                break;
                            }
                        }
                        if (!done) ++h;
                    }
                    
                    // Clear mask
                    for (int l = 0; l < h; ++l) {
                        for (int k = 0; k < w; ++k) {
                            mask[(v + l) * CHUNK_SIZE + u + k] = 0;
                        }
                    }
                    
                    // Add quad
                    Quad quad;
                    if (w_axis == 0) {
                        quad.x = d; quad.y = u; quad.z = v;
                    } else if (w_axis == 1) {
                        quad.x = u; quad.y = d; quad.z = v;
                    } else {
                        quad.x = u; quad.y = v; quad.z = d;
                    }
                    
                    if (u_axis == 0) {
                        quad.w = w; quad.h = h;
                    } else if (u_axis == 1) {
                        quad.w = w; quad.h = h;
                    } else {
                        quad.w = w; quad.h = h;
                    }
                    
                    quad.normal = Vertex::packNormal(nx, ny, nz);
                    quad.material = material;
                    
                    addQuad(quad, meshData);
                    
                    u += w;
                }
            }
        }
    }
}

bool MeshBuilder::isBlockSolid(std::shared_ptr<Chunk> chunk, int x, int y, int z,
                                std::shared_ptr<Chunk> neighbors[6]) {
    // Check in neighbor chunks if out of bounds
    if (x < 0 && neighbors[1]) return neighbors[1]->getBlock(x + CHUNK_SIZE, y, z).isOpaque();
    if (x >= CHUNK_SIZE && neighbors[0]) return neighbors[0]->getBlock(x - CHUNK_SIZE, y, z).isOpaque();
    if (y < 0 && neighbors[3]) return neighbors[3]->getBlock(x, y + CHUNK_HEIGHT, z).isOpaque();
    if (y >= CHUNK_HEIGHT && neighbors[2]) return neighbors[2]->getBlock(x, y - CHUNK_HEIGHT, z).isOpaque();
    if (z < 0 && neighbors[5]) return neighbors[5]->getBlock(x, y, z + CHUNK_SIZE).isOpaque();
    if (z >= CHUNK_SIZE && neighbors[4]) return neighbors[4]->getBlock(x, y, z - CHUNK_SIZE).isOpaque();
    
    return false;
}

void MeshBuilder::addQuad(const Quad& quad, MeshData& meshData) {
    u32 baseIdx = static_cast<u32>(meshData.vertices.size());
    
    // Create 4 vertices for the quad (simplified for now - proper positioning needed)
    i16 x = static_cast<i16>(quad.x);
    i16 y = static_cast<i16>(quad.y);
    i16 z = static_cast<i16>(quad.z);
    i16 w = static_cast<i16>(quad.w);
    i16 h = static_cast<i16>(quad.h);
    
    u16 uv00 = Vertex::packUV(0.0f, 0.0f);
    u16 uv10 = Vertex::packUV(1.0f, 0.0f);
    u16 uv11 = Vertex::packUV(1.0f, 1.0f);
    u16 uv01 = Vertex::packUV(0.0f, 1.0f);
    
    meshData.vertices.emplace_back(x, y, z, quad.normal, quad.material, uv00);
    meshData.vertices.emplace_back(x + w, y, z, quad.normal, quad.material, uv10);
    meshData.vertices.emplace_back(x + w, y + h, z, quad.normal, quad.material, uv11);
    meshData.vertices.emplace_back(x, y + h, z, quad.normal, quad.material, uv01);
    
    // Add indices (two triangles)
    meshData.indices.push_back(baseIdx + 0);
    meshData.indices.push_back(baseIdx + 1);
    meshData.indices.push_back(baseIdx + 2);
    
    meshData.indices.push_back(baseIdx + 0);
    meshData.indices.push_back(baseIdx + 2);
    meshData.indices.push_back(baseIdx + 3);
}
