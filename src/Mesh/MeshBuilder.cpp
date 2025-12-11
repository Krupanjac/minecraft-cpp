#include "MeshBuilder.h"
#include "../Util/Config.h"
#include <array>
#include <cstring>
#include <tuple>
#include <unordered_map>

MeshData MeshBuilder::buildChunkMesh(std::shared_ptr<Chunk> chunk,
                                     std::shared_ptr<Chunk> chunkXPos,
                                     std::shared_ptr<Chunk> chunkXNeg,
                                     std::shared_ptr<Chunk> chunkYPos,
                                     std::shared_ptr<Chunk> chunkYNeg,
                                     std::shared_ptr<Chunk> chunkZPos,
                                     std::shared_ptr<Chunk> chunkZNeg,
                                     int lod) {
    MeshData meshData;
    
    std::shared_ptr<Chunk> neighbors[6] = {
        chunkXPos, chunkXNeg, chunkYPos, chunkYNeg, chunkZPos, chunkZNeg
    };
    
    greedyMesh(chunk, neighbors, meshData, lod);
    
    return meshData;
}

void MeshBuilder::greedyMesh(std::shared_ptr<Chunk> chunk,
                             std::shared_ptr<Chunk> neighbors[6],
                             MeshData& meshData,
                             int lod) {
    // Greedy meshing for each axis and direction
    const int dirs[6][3] = {
        {1, 0, 0}, {-1, 0, 0},  // X+, X-
        {0, 1, 0}, {0, -1, 0},  // Y+, Y-
        {0, 0, 1}, {0, 0, -1}   // Z+, Z-
    };
    
    int step = 1 << lod;
    int size = CHUNK_SIZE >> lod;
    // int height = CHUNK_HEIGHT >> lod; // Unused
    
    auto getBlockGlobal = [&](int gx, int gy, int gz) -> Block {
        if (gx < 0 || gx >= CHUNK_SIZE || gy < 0 || gy >= CHUNK_HEIGHT || gz < 0 || gz >= CHUNK_SIZE) {
            if (gx < 0 && neighbors[1]) return neighbors[1]->getBlock(gx + CHUNK_SIZE, gy, gz);
            if (gx >= CHUNK_SIZE && neighbors[0]) return neighbors[0]->getBlock(gx - CHUNK_SIZE, gy, gz);
            if (gy < 0 && neighbors[3]) return neighbors[3]->getBlock(gx, gy + CHUNK_HEIGHT, gz);
            if (gy >= CHUNK_HEIGHT && neighbors[2]) return neighbors[2]->getBlock(gx, gy - CHUNK_HEIGHT, gz);
            if (gz < 0 && neighbors[5]) return neighbors[5]->getBlock(gx, gy, gz + CHUNK_SIZE);
            if (gz >= CHUNK_SIZE && neighbors[4]) return neighbors[4]->getBlock(gx, gy, gz - CHUNK_SIZE);
            return Block(BlockType::AIR);
        }
        return chunk->getBlock(gx, gy, gz);
    };

    // Simple sampling for performance (O(1))
    // We rely on increased LOD distance to hide the block alignment artifacts.
    auto sampleBlock = [&](int baseX, int baseY, int baseZ) -> Block {
        return getBlockGlobal(baseX, baseY, baseZ);
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
        std::vector<u8> mask(size * size);
        
        for (int d = 0; d < size; ++d) {
            std::fill(mask.begin(), mask.end(), static_cast<u8>(0));
            
            // Build mask
            for (int v = 0; v < size; ++v) {
                for (int u = 0; u < size; ++u) {
                    int x = ((w_axis == 0) ? d : (u_axis == 0) ? u : v) * step;
                    int y = ((w_axis == 1) ? d : (u_axis == 1) ? u : v) * step;
                    int z = ((w_axis == 2) ? d : (u_axis == 2) ? u : v) * step;
                    
                    if (x >= CHUNK_SIZE || y >= CHUNK_HEIGHT || z >= CHUNK_SIZE) continue;
                    
                    Block block = sampleBlock(x, y, z);
                    if (!block.isSolid() && !block.isWater()) continue;
                    
                    // Check if face should be rendered
                    int adjX = x + nx * step;
                    int adjY = y + ny * step;
                    int adjZ = z + nz * step;
                    
                    bool shouldRender = false;
                    Block adjBlock = sampleBlock(adjX, adjY, adjZ);
                    
                    if (block.isWater()) {
                        // Render water face if neighbor is NOT water and NOT opaque (so Air or Glass)
                        // Also don't render against ICE to avoid Z-fighting
                        shouldRender = !adjBlock.isWater() && !adjBlock.isOpaque() && (adjBlock.getType() != BlockType::ICE);
                    } else if (block.isTransparent()) {
                        // ICE and other transparent blocks
                        // Don't render if neighbor is opaque OR if neighbor is the same type (e.g. Ice next to Ice)
                        shouldRender = !adjBlock.isOpaque() && (adjBlock.getType() != block.getType());
                    } else {
                        // Solid block: Render if neighbor is NOT opaque
                        shouldRender = !adjBlock.isOpaque();
                    }
                    
                    if (shouldRender) {
                        mask[v * size + u] = block.getMaterialID();
                    }
                }
            }
            
            // Generate mesh from mask using greedy algorithm
            for (int v = 0; v < size; ++v) {
                for (int u = 0; u < size; ) {
                    u8 material = mask[v * size + u];
                    if (material == 0) {
                        ++u;
                        continue;
                    }
                    
                    // Compute width
                    int w = 1;
                    // Disable greedy meshing for water and ice to prevent gaps with vertex displacement
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (u + w < size && mask[v * size + u + w] == material) {
                            ++w;
                        }
                    }
                    
                    // Compute height
                    int h = 1;
                    bool done = false;
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (v + h < size && !done) {
                            for (int k = 0; k < w; ++k) {
                                if (mask[(v + h) * size + u + k] != material) {
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
                            mask[(v + l) * size + u + k] = 0;
                        }
                    }
                    
                    // Add quad
                    Quad quad;
                    if (w_axis == 0) {
                        quad.x = ((nx > 0) ? d + 1 : d) * step; 
                        quad.y = u * step; 
                        quad.z = v * step;
                    } else if (w_axis == 1) {
                        quad.x = u * step; 
                        quad.y = ((ny > 0) ? d + 1 : d) * step; 
                        quad.z = v * step;
                    } else {
                        quad.x = u * step; 
                        quad.y = v * step; 
                        quad.z = ((nz > 0) ? d + 1 : d) * step;
                    }
                    
                    quad.w = w * step; 
                    quad.h = h * step;
                    
                    quad.u_axis = u_axis;
                    quad.v_axis = v_axis;
                    
                    quad.nx = nx;
                    quad.ny = ny;
                    quad.nz = nz;
                    
                    quad.normal = Vertex::packNormal(nx, ny, nz);
                    quad.material = material;
                    
                    // Calculate AO
                    // For LOD > 0, we can simplify AO or just sample at corners
                    // We'll use the same logic but with 'step' for neighbor checks
                    
                    int u_vec[3] = {0}; u_vec[u_axis] = step;
                    int v_vec[3] = {0}; v_vec[v_axis] = step;
                    int n_vec[3] = {nx * step, ny * step, nz * step};
                    int neg_u[3] = {-u_vec[0], -u_vec[1], -u_vec[2]};
                    int neg_v[3] = {-v_vec[0], -v_vec[1], -v_vec[2]};
                    
                    // V0: (0, 0) -> Block (0, 0), check -u, -v
                    quad.ao[0] = calculateVertexAO(chunk, quad.x, quad.y, quad.z, neg_u, neg_v, neighbors);
                    
                    // V1: (w, 0) -> Block (w-1, 0), check +u, -v
                    int bx = quad.x + (quad.w - step)*u_vec[0]/step;
                    int by = quad.y + (quad.w - step)*u_vec[1]/step;
                    int bz = quad.z + (quad.w - step)*u_vec[2]/step;
                    quad.ao[1] = calculateVertexAO(chunk, bx, by, bz, u_vec, neg_v, neighbors);
                    
                    // V2: (w, h) -> Block (w-1, h-1), check +u, +v
                    bx = quad.x + (quad.w - step)*u_vec[0]/step + (quad.h - step)*v_vec[0]/step;
                    by = quad.y + (quad.w - step)*u_vec[1]/step + (quad.h - step)*v_vec[1]/step;
                    bz = quad.z + (quad.w - step)*u_vec[2]/step + (quad.h - step)*v_vec[2]/step;
                    quad.ao[2] = calculateVertexAO(chunk, bx, by, bz, u_vec, v_vec, neighbors);
                    
                    // V3: (0, h) -> Block (0, h-1), check -u, +v
                    bx = quad.x + (quad.h - step)*v_vec[0]/step;
                    by = quad.y + (quad.h - step)*v_vec[1]/step;
                    bz = quad.z + (quad.h - step)*v_vec[2]/step;
                    quad.ao[3] = calculateVertexAO(chunk, bx, by, bz, neg_u, v_vec, neighbors);
                    
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
    
    if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE) {
        return chunk->getBlock(x, y, z).isOpaque();
    }

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
                                 const int* u_vec, const int* v_vec,
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
