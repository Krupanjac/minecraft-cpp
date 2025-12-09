#include "MeshBuilder.h"
#include "../Util/Config.h"
#include <array>
#include <cstring>
#include <tuple>

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
                    if (!block.isSolid() && !block.isWater()) continue;
                    
                    // Check if face should be rendered
                    int adjX = x + nx;
                    int adjY = y + ny;
                    int adjZ = z + nz;
                    
                    bool shouldRender = false;
                    Block adjBlock;
                    
                    if (adjX < 0 || adjX >= CHUNK_SIZE || 
                        adjY < 0 || adjY >= CHUNK_HEIGHT || 
                        adjZ < 0 || adjZ >= CHUNK_SIZE) {
                        // Check neighbor chunk
                        // Simple helper to get block from neighbors
                        if (adjX < 0 && neighbors[1]) adjBlock = neighbors[1]->getBlock(adjX + CHUNK_SIZE, adjY, adjZ);
                        else if (adjX >= CHUNK_SIZE && neighbors[0]) adjBlock = neighbors[0]->getBlock(adjX - CHUNK_SIZE, adjY, adjZ);
                        else if (adjY < 0 && neighbors[3]) adjBlock = neighbors[3]->getBlock(adjX, adjY + CHUNK_HEIGHT, adjZ);
                        else if (adjY >= CHUNK_HEIGHT && neighbors[2]) adjBlock = neighbors[2]->getBlock(adjX, adjY - CHUNK_HEIGHT, adjZ);
                        else if (adjZ < 0 && neighbors[5]) adjBlock = neighbors[5]->getBlock(adjX, adjY, adjZ + CHUNK_SIZE);
                        else if (adjZ >= CHUNK_SIZE && neighbors[4]) adjBlock = neighbors[4]->getBlock(adjX, adjY, adjZ - CHUNK_SIZE);
                        else adjBlock = Block(BlockType::AIR); // Default if neighbor chunk missing
                    } else {
                        adjBlock = chunk->getBlock(adjX, adjY, adjZ);
                    }
                    
                    if (block.isWater()) {
                        // Render water face if neighbor is NOT water and NOT opaque (so Air or Glass)
                        // Actually, if neighbor is solid, we don't render.
                        // If neighbor is Air, we render.
                        // If neighbor is Water, we don't render.
                        shouldRender = !adjBlock.isWater() && !adjBlock.isOpaque();
                    } else if (block.isTransparent()) {
                        // ICE and other transparent blocks
                        shouldRender = !adjBlock.isOpaque() || adjBlock.isTransparent();
                    } else {
                        // Solid block: Render if neighbor is NOT opaque
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
                    // Disable greedy meshing for water and ice to prevent gaps with vertex displacement
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (u + w < CHUNK_SIZE && mask[v * CHUNK_SIZE + u + w] == material) {
                            ++w;
                        }
                    }
                    
                    // Compute height
                    int h = 1;
                    bool done = false;
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (v + h < CHUNK_SIZE && !done) {
                            for (int k = 0; k < w; ++k) {
                                if (mask[(v + h) * CHUNK_SIZE + u + k] != material) {
                                    done = true;
                                    break;
                                }
                            }
                            if (!done) ++h;
                        }
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
                        quad.x = (nx > 0) ? d + 1 : d; 
                        quad.y = u; 
                        quad.z = v;
                    } else if (w_axis == 1) {
                        quad.x = u; 
                        quad.y = (ny > 0) ? d + 1 : d; 
                        quad.z = v;
                    } else {
                        quad.x = u; 
                        quad.y = v; 
                        quad.z = (nz > 0) ? d + 1 : d;
                    }
                    
                    quad.w = w; 
                    quad.h = h;
                    
                    quad.u_axis = u_axis;
                    quad.v_axis = v_axis;
                    
                    quad.nx = nx;
                    quad.ny = ny;
                    quad.nz = nz;
                    
                    quad.normal = Vertex::packNormal(nx, ny, nz);
                    quad.material = material;
                    
                    // Calculate AO
                    int u_vec[3] = {0}; u_vec[u_axis] = 1;
                    int v_vec[3] = {0}; v_vec[v_axis] = 1;
                    int n_vec[3] = {nx, ny, nz};
                    int neg_u[3] = {-u_vec[0], -u_vec[1], -u_vec[2]};
                    int neg_v[3] = {-v_vec[0], -v_vec[1], -v_vec[2]};
                    
                    // V0: (0, 0) -> Block (0, 0), check -u, -v
                    quad.ao[0] = calculateVertexAO(chunk, quad.x, quad.y, quad.z, n_vec, neg_u, neg_v, neighbors);
                    
                    // V1: (w, 0) -> Block (w-1, 0), check +u, -v
                    int bx = quad.x + (w-1)*u_vec[0];
                    int by = quad.y + (w-1)*u_vec[1];
                    int bz = quad.z + (w-1)*u_vec[2];
                    quad.ao[1] = calculateVertexAO(chunk, bx, by, bz, n_vec, u_vec, neg_v, neighbors);
                    
                    // V2: (w, h) -> Block (w-1, h-1), check +u, +v
                    bx = quad.x + (w-1)*u_vec[0] + (h-1)*v_vec[0];
                    by = quad.y + (w-1)*u_vec[1] + (h-1)*v_vec[1];
                    bz = quad.z + (w-1)*u_vec[2] + (h-1)*v_vec[2];
                    quad.ao[2] = calculateVertexAO(chunk, bx, by, bz, n_vec, u_vec, v_vec, neighbors);
                    
                    // V3: (0, h) -> Block (0, h-1), check -u, +v
                    bx = quad.x + (h-1)*v_vec[0];
                    by = quad.y + (h-1)*v_vec[1];
                    bz = quad.z + (h-1)*v_vec[2];
                    quad.ao[3] = calculateVertexAO(chunk, bx, by, bz, n_vec, neg_u, v_vec, neighbors);
                    
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
    bool isWater = (quad.material == static_cast<u8>(BlockType::WATER));
    bool isIce = (quad.material == static_cast<u8>(BlockType::ICE));
    
    // Render water and ice as transparent
    bool isTransparent = isWater || isIce;
    
    // Use appropriate vertex/index list
    auto& vertices = isTransparent ? meshData.waterVertices : meshData.vertices;
    auto& indices = isTransparent ? meshData.waterIndices : meshData.indices;
    
    u32 baseIdx = static_cast<u32>(vertices.size());
    
    auto getPos = [&](int u, int v) {
        int px = quad.x;
        int py = quad.y;
        int pz = quad.z;
        
        if (quad.u_axis == 0) px += u;
        else if (quad.u_axis == 1) py += u;
        else pz += u;
        
        if (quad.v_axis == 0) px += v;
        else if (quad.v_axis == 1) py += v;
        else pz += v;
        
        return std::make_tuple(static_cast<i16>(px), static_cast<i16>(py), static_cast<i16>(pz));
    };
    
    auto [x0, y0, z0] = getPos(0, 0);
    auto [x1, y1, z1] = getPos(quad.w, 0);
    auto [x2, y2, z2] = getPos(quad.w, quad.h);
    auto [x3, y3, z3] = getPos(0, quad.h);
    
    // Pass dimensions (w, h) as UVs for tiling
    u16 uv00 = Vertex::packUV(0, 0);
    u16 uv10 = Vertex::packUV(quad.w, 0);
    u16 uv11 = Vertex::packUV(quad.w, quad.h);
    u16 uv01 = Vertex::packUV(0, quad.h);
    
    vertices.emplace_back(x0, y0, z0, quad.normal, quad.material, uv00, quad.ao[0]);
    vertices.emplace_back(x1, y1, z1, quad.normal, quad.material, uv10, quad.ao[1]);
    vertices.emplace_back(x2, y2, z2, quad.normal, quad.material, uv11, quad.ao[2]);
    vertices.emplace_back(x3, y3, z3, quad.normal, quad.material, uv01, quad.ao[3]);
    
    // Winding order
    // Determine winding order based on face normal
    // X- (nx < 0), Y+ (ny > 0), Z- (nz < 0) need CW winding
    bool reverseWinding = (quad.nx < 0) || (quad.ny > 0) || (quad.nz < 0);
    
    // Determine triangulation split based on AO
    // Connect vertices with highest AO (brightest) to avoid dark creases
    bool flipSplit = (quad.ao[1] + quad.ao[3]) > (quad.ao[0] + quad.ao[2]);
    
    if (reverseWinding) {
        if (flipSplit) {
            // Connect 1-3, CW winding
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 3);
            
            indices.push_back(baseIdx + 3);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 1);
        } else {
            // Connect 0-2, CW winding
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 1);
            
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 3);
            indices.push_back(baseIdx + 2);
        }
    } else {
        if (flipSplit) {
            // Connect 1-3, CCW winding
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 3);
            
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        } else {
            // Connect 0-2, CCW winding
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }
    }
}

u8 MeshBuilder::calculateVertexAO(std::shared_ptr<Chunk> chunk, int x, int y, int z, 
                                 const int* n_vec, const int* u_vec, const int* v_vec,
                                 std::shared_ptr<Chunk> neighbors[6]) {
    // Check 3 neighbors: Side1 (u), Side2 (v), Corner (u+v)
    // x,y,z is the block inside the quad.
    // u_vec, v_vec are directions to neighbors.
    
    bool s1 = isBlockSolid(chunk, x + u_vec[0], y + u_vec[1], z + u_vec[2], neighbors);
    bool s2 = isBlockSolid(chunk, x + v_vec[0], y + v_vec[1], z + v_vec[2], neighbors);
    bool c = isBlockSolid(chunk, x + u_vec[0] + v_vec[0], y + u_vec[1] + v_vec[1], z + u_vec[2] + v_vec[2], neighbors);
    
    if (s1 && s2) return 0;
    return 3 - (s1 + s2 + c);
}
