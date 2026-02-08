#include "MeshBuilder.h"
#include "../Util/Config.h"
#include <algorithm>
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
    
    // Pass 1: Standard Greedy Meshing for solid blocks
    greedyMesh(chunk, neighbors, meshData, lod);
    
    // Pass 2: Special models (Vegetation) - Only at LOD 0 for now to save perf
    if (lod == 0) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    Block block = chunk->getBlock(x, y, z);
                    if (block.isCrossModel()) {
                        // Simple AO for plant: check block below
                        u8 ao = 3; // Default bright
                        // If block below is solid, maybe darken slightly at bottom? 
                        // For now, just flat lighting
                        
                        // Calculate sky light for vegetation
                        u8 skyLight = calculateSkyLight(chunk, x, y, z, neighbors);
                        addCross(x, y, z, block.getMaterialID(), ao, skyLight, meshData);
                    }
                }
            }
        }
    }
    
    return meshData;
}

void MeshBuilder::addCross(int x, int y, int z, u8 material, u8 ao, u8 skyLight, MeshData& meshData) {
    // Two intersecting quads forming an X shape
    // Quad 1: diagonal from (0,0,0) to (1,1,1)
    // Quad 2: diagonal from (0,0,1) to (1,1,0)
    
    // UV represents block dimensions for the face (1x1 block)
    // The shader unpacks these as the size, giving coords 0..1 across the face
    // fract() is used in fragment shader for texture tiling
    u16 uv00 = Vertex::packUV(0, 0);
    u16 uv10 = Vertex::packUV(1, 0);
    u16 uv11 = Vertex::packUV(1, 1);
    u16 uv01 = Vertex::packUV(0, 1);
    
    u8 normalUp = Vertex::packNormal(0, 1, 0); // Fake normal up for lighting
    
    // Quad 1 - Store sky light in data field
    meshData.vertices.emplace_back(static_cast<i16>(x), static_cast<i16>(y), static_cast<i16>(z), normalUp, material, uv00, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x + 1), static_cast<i16>(y), static_cast<i16>(z + 1), normalUp, material, uv10, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x + 1), static_cast<i16>(y + 1), static_cast<i16>(z + 1), normalUp, material, uv11, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x), static_cast<i16>(y + 1), static_cast<i16>(z), normalUp, material, uv01, ao, skyLight);
    
    u32 baseIdx = static_cast<u32>(meshData.vertices.size()) - 4;
    // Double sided
    meshData.indices.push_back(baseIdx + 0); meshData.indices.push_back(baseIdx + 1); meshData.indices.push_back(baseIdx + 2);
    meshData.indices.push_back(baseIdx + 0); meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 3);
    meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 1); meshData.indices.push_back(baseIdx + 0);
    meshData.indices.push_back(baseIdx + 3); meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 0);

    // Quad 2 - Store sky light in data field
    meshData.vertices.emplace_back(static_cast<i16>(x), static_cast<i16>(y), static_cast<i16>(z + 1), normalUp, material, uv00, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x + 1), static_cast<i16>(y), static_cast<i16>(z), normalUp, material, uv10, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x + 1), static_cast<i16>(y + 1), static_cast<i16>(z), normalUp, material, uv11, ao, skyLight);
    meshData.vertices.emplace_back(static_cast<i16>(x), static_cast<i16>(y + 1), static_cast<i16>(z + 1), normalUp, material, uv01, ao, skyLight);
    
    baseIdx = static_cast<u32>(meshData.vertices.size()) - 4;
    // Double sided
    meshData.indices.push_back(baseIdx + 0); meshData.indices.push_back(baseIdx + 1); meshData.indices.push_back(baseIdx + 2);
    meshData.indices.push_back(baseIdx + 0); meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 3);
    meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 1); meshData.indices.push_back(baseIdx + 0);
    meshData.indices.push_back(baseIdx + 3); meshData.indices.push_back(baseIdx + 2); meshData.indices.push_back(baseIdx + 0);
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
            // Neighbor chunk not loaded - assume water below sea level to prevent ugly side faces
            // SEA_LEVEL = 32, chunk world Y is chunk->getPosition().y * CHUNK_HEIGHT
            int worldY = static_cast<int>(chunk->getPosition().y) * CHUNK_HEIGHT + gy;
            if (worldY < 32) {
                return Block(BlockType::WATER);
            }
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
        std::vector<u16> mask(size * size);
        
        for (int d = 0; d < size; ++d) {
            std::fill(mask.begin(), mask.end(), static_cast<u16>(0));
            
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
                        if (adjBlock.isWater()) {
                            // If neighbor is water, only render if we are "higher" (smaller data value)
                            // and only for side faces (not top/bottom)
                            if (ny != 0) {
                                shouldRender = false;
                            } else {
                                // Only render if there is a significant drop
                                // And ensure we don't render internal faces for flat water
                                shouldRender = block.getData() < adjBlock.getData();
                            }
                        } else {
                            // Render water face if neighbor is NOT water and NOT opaque (so Air or Glass)
                            // Also don't render against ICE to avoid Z-fighting
                            shouldRender = !adjBlock.isOpaque() && (adjBlock.getType() != BlockType::ICE);
                        }
                    } else if (block.isTransparent()) {
                        // ICE and other transparent blocks
                        // Don't render if neighbor is opaque OR if neighbor is the same type (e.g. Ice next to Ice)
                        shouldRender = !adjBlock.isOpaque() && (adjBlock.getType() != block.getType());
                    } else {
                        // Solid block: Render if neighbor is NOT opaque
                        shouldRender = !adjBlock.isOpaque();
                    }
                    
                    if (shouldRender) {
                        u8 data = block.getData();
                        
                        // Check for water above to prevent "drop" logic for waterfalls
                        if (block.isWater()) {
                            Block above = sampleBlock(x, y + step, z);
                            if (above.isWater()) {
                                // Set bit 5 (0x20) to signal "hasWaterAbove"
                                data |= 0x20;
                            }
                        }
                        
                        mask[v * size + u] = (static_cast<u16>(data) << 8) | block.getMaterialID();
                    }
                }
            }
            
            // Generate mesh from mask using greedy algorithm
            for (int v = 0; v < size; ++v) {
                for (int u = 0; u < size; ) {
                    u16 val = mask[v * size + u];
                    if (val == 0) {
                        ++u;
                        continue;
                    }
                    
                    u8 material = static_cast<u8>(val & 0xFF);
                    u8 data = static_cast<u8>((val >> 8) & 0xFF);
                    
                    // Compute width
                    int w = 1;
                    // Disable greedy meshing for water and ice to prevent gaps with vertex displacement
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (u + w < size && mask[v * size + u + w] == val) {
                            ++w;
                        }
                    }
                    
                    // Compute height
                    int h = 1;
                    bool done = false;
                    if (material != static_cast<u8>(BlockType::WATER) && material != static_cast<u8>(BlockType::ICE)) {
                        while (v + h < size && !done) {
                            for (int k = 0; k < w; ++k) {
                                if (mask[(v + h) * size + u + k] != val) {
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
                    quad.data = data;
                    
                    // Calculate AO - skip for water and ice (transparent blocks shouldn't have AO)
                    bool skipAO = (material == static_cast<u8>(BlockType::WATER) || material == static_cast<u8>(BlockType::ICE));
                    
                    if (skipAO) {
                        // Full brightness for water/ice - no ambient occlusion
                        quad.ao[0] = quad.ao[1] = quad.ao[2] = quad.ao[3] = 3;
                    } else {
                        // For LOD > 0, we can simplify AO or just sample at corners
                        // We'll use the same logic but with 'step' for neighbor checks
                        
                        int u_vec[3] = {0}; u_vec[u_axis] = step;
                        int v_vec[3] = {0}; v_vec[v_axis] = step;
                        int n_vec[3] = {nx * step, ny * step, nz * step};
                        int neg_u[3] = {-u_vec[0], -u_vec[1], -u_vec[2]};
                        int neg_v[3] = {-v_vec[0], -v_vec[1], -v_vec[2]};
                        
                        // The face is at the boundary of the block. For AO, we need to check
                        // blocks that are ADJACENT to the exposed face, which means checking
                        // in the direction of the face normal from the face position.
                        //
                        // quad.x/y/z gives the corner of the face (already offset for positive normals).
                        // For negative normals (nx<0, ny<0, nz<0), the face is at the block's min boundary.
                        // For positive normals, the face is at the block's max boundary.
                        //
                        // AO neighbors should be checked from the block position, looking outward.
                        // The key insight: for a face with normal N, we check blocks at positions
                        // that would cast shadows onto this face - those are blocks that are
                        // in the N direction from the edge, but also adjacent in u/v directions.
                        
                        // Get the position of the block that OWNS this face (block behind the face)
                        int blockX = quad.x - (nx > 0 ? step : 0);
                        int blockY = quad.y - (ny > 0 ? step : 0);
                        int blockZ = quad.z - (nz > 0 ? step : 0);
                        
                        // Now check AO from the face's perspective
                        // V0: corner at (0,0) of the quad - check neighbors in -u and -v from face
                        quad.ao[0] = calculateVertexAOWithNormal(chunk, blockX, blockY, blockZ, 
                                                                  neg_u, neg_v, n_vec, neighbors);
                        
                        // V1: corner at (w,0) - block at far end in u direction
                        int bx = blockX + (quad.w - step)*u_vec[0]/step;
                        int by = blockY + (quad.w - step)*u_vec[1]/step;
                        int bz = blockZ + (quad.w - step)*u_vec[2]/step;
                        quad.ao[1] = calculateVertexAOWithNormal(chunk, bx, by, bz, 
                                                                  u_vec, neg_v, n_vec, neighbors);
                        
                        // V2: corner at (w,h) - block at far end in both u and v
                        bx = blockX + (quad.w - step)*u_vec[0]/step + (quad.h - step)*v_vec[0]/step;
                        by = blockY + (quad.w - step)*u_vec[1]/step + (quad.h - step)*v_vec[1]/step;
                        bz = blockZ + (quad.w - step)*u_vec[2]/step + (quad.h - step)*v_vec[2]/step;
                        quad.ao[2] = calculateVertexAOWithNormal(chunk, bx, by, bz, 
                                                                  u_vec, v_vec, n_vec, neighbors);
                        
                        // V3: corner at (0,h) - block at far end in v direction
                        bx = blockX + (quad.h - step)*v_vec[0]/step;
                        by = blockY + (quad.h - step)*v_vec[1]/step;
                        bz = blockZ + (quad.h - step)*v_vec[2]/step;
                        quad.ao[3] = calculateVertexAOWithNormal(chunk, bx, by, bz, 
                                                                  neg_u, v_vec, n_vec, neighbors);
                    }
                    
                    // Calculate sky light for this quad
                    // IMPORTANT: Check sky light from the ADJACENT air position, not the block itself.
                    // This ensures a face exposed to air above gets proper sky light, even if the
                    // block itself is underground. We check from where the face is visible (the air space).
                    int blockX = quad.x - (nx > 0 ? step : 0);
                    int blockY = quad.y - (ny > 0 ? step : 0);
                    int blockZ = quad.z - (nz > 0 ? step : 0);
                    
                    // Adjacent position (the air space in front of this face)
                    int adjX = blockX + nx * step;
                    int adjY = blockY + ny * step;
                    int adjZ = blockZ + nz * step;
                    
                    // For water, don't override sky light (it's stored in data for water level)
                    if (material == static_cast<u8>(BlockType::WATER)) {
                        quad.skyLight = 15; // Water always gets full sky light (handled differently)
                    } else {
                        // Check sky light from the adjacent air position
                        quad.skyLight = calculateSkyLight(chunk, adjX, adjY, adjZ, neighbors);
                    }
                    
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
    
    // Determine which vertices are "top" vertices for water height adjustment
    u8 data0 = quad.data;
    u8 data1 = quad.data;
    u8 data2 = quad.data;
    u8 data3 = quad.data;

    if (isWater) {
        // Check if this is a waterfall block (has water above)
        // Bit 5 (0x20) is set in greedyMesh if so
        bool isWaterfall = (quad.data & 0x20) != 0;
        
        if (!isWaterfall) {
            // Find min/max Y to identify top vertices
            i16 minY = std::min({y0, y1, y2, y3});
            i16 maxY = std::max({y0, y1, y2, y3});
            
            auto isTop = [&](i16 y) {
                if (minY == maxY) return quad.ny > 0; // Horizontal face: Top if normal is Up
                return y == maxY; // Vertical face: Top if Y is max
            };
            
            // Set bit 4 (0x10) if top vertex
            if (isTop(y0)) data0 |= 0x10;
            if (isTop(y1)) data1 |= 0x10;
            if (isTop(y2)) data2 |= 0x10;
            if (isTop(y3)) data3 |= 0x10;
        }
    } else {
        // For non-water blocks: bits 0-3 = sky light, bits 4-5 = face rotation
        // Sky light is 0-15, where 15 = full sky access, 0 = underground
        // Face rotation (from metadata) is in bits 0-1 of quad.data
        u8 faceRot = (quad.data & 0x03);
        u8 packedData = quad.skyLight | (faceRot << 4);
        data0 = packedData;
        data1 = packedData;
        data2 = packedData;
        data3 = packedData;
    }
    
    // Pass dimensions (w, h) as UVs for tiling
    u16 uv00 = Vertex::packUV(0, 0);
    u16 uv10 = Vertex::packUV(quad.w, 0);
    u16 uv11 = Vertex::packUV(quad.w, quad.h);
    u16 uv01 = Vertex::packUV(0, quad.h);
    
    vertices.emplace_back(x0, y0, z0, quad.normal, quad.material, uv00, quad.ao[0], data0);
    vertices.emplace_back(x1, y1, z1, quad.normal, quad.material, uv10, quad.ao[1], data1);
    vertices.emplace_back(x2, y2, z2, quad.normal, quad.material, uv11, quad.ao[2], data2);
    vertices.emplace_back(x3, y3, z3, quad.normal, quad.material, uv01, quad.ao[3], data3);
    
    // Winding order
    // Determine winding order based on face normal
    // X- (nx < 0), Y+ (ny > 0), Z- (nz < 0) need CW winding
    bool reverseWinding = (quad.nx < 0) || (quad.ny > 0) || (quad.nz < 0);
    
    // Determine triangulation split based on AO
    // Connect vertices with highest AO (brightest) to avoid dark creases
    // AO values: 0 (darkest) to 3 (brightest)
    // We want to split along the diagonal that connects the two vertices with the most similar AO?
    // Or the diagonal that avoids cutting through a shadow?
    // Standard voxel AO trick: if (ao0 + ao2 < ao1 + ao3) flip
    bool flipSplit = (quad.ao[0] + quad.ao[2]) < (quad.ao[1] + quad.ao[3]);
    
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
    // Legacy function - forwards to new implementation with zero normal
    int n_vec[3] = {0, 0, 0};
    return calculateVertexAOWithNormal(chunk, x, y, z, u_vec, v_vec, n_vec, neighbors);
}

u8 MeshBuilder::calculateVertexAOWithNormal(std::shared_ptr<Chunk> chunk, int x, int y, int z,
                                            const int* u_vec, const int* v_vec, const int* n_vec,
                                            std::shared_ptr<Chunk> neighbors[6]) {
    // Minecraft-style ambient occlusion for block face vertices
    //
    // For each vertex of a face, we check 3 neighbor blocks that could occlude it:
    // - Side1: block adjacent in u direction (along one edge)
    // - Side2: block adjacent in v direction (along other edge)
    // - Corner: block diagonal in u+v direction
    //
    // CRITICAL: These neighbors must be checked in the space IN FRONT of the face,
    // i.e., offset by the face normal. A block behind the face doesn't occlude it.
    //
    // x,y,z = position of the block that owns this face
    // u_vec = direction along one edge of the face
    // v_vec = direction along other edge of the face  
    // n_vec = face normal direction (points outward from the block)
    
    // Check neighbors at positions offset by the normal (in front of the face)
    // These are the blocks that would actually cast shadows onto this vertex
    int nx = x + n_vec[0];
    int ny = y + n_vec[1];
    int nz = z + n_vec[2];
    
    // Side1: adjacent in u direction, in front of face
    bool s1 = isBlockSolid(chunk, nx + u_vec[0], ny + u_vec[1], nz + u_vec[2], neighbors);
    
    // Side2: adjacent in v direction, in front of face
    bool s2 = isBlockSolid(chunk, nx + v_vec[0], ny + v_vec[1], nz + v_vec[2], neighbors);
    
    // Corner: diagonal in u+v direction, in front of face
    bool c = isBlockSolid(chunk, nx + u_vec[0] + v_vec[0], ny + u_vec[1] + v_vec[1], nz + u_vec[2] + v_vec[2], neighbors);
    
    // Standard AO formula:
    // - If both sides are solid, the corner is fully occluded (AO = 0)
    //   This prevents light leaking through diagonal cracks
    // - Otherwise, count solid neighbors and subtract from 3
    if (s1 && s2) return 0;
    return 3 - (s1 + s2 + c);
}

u8 MeshBuilder::calculateSkyLight(std::shared_ptr<Chunk> chunk, int x, int y, int z,
                                  std::shared_ptr<Chunk> neighbors[6]) {
    // Calculate sky light by checking if there's any opaque block above this position.
    // Sky light is 15 if the block can see the sky, 0 if it's completely underground.
    // This provides a Minecraft-style light propagation that blocks sunlight in caves.
    
    // Handle out-of-bounds X/Z by checking neighbor chunks
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        // Position is outside current chunk - assume sky light for simplicity
        // (edge cases at chunk boundaries)
        return 15;
    }
    
    ChunkPos chunkPos = chunk->getPosition();
    
    // Check all blocks above within this chunk first
    for (int checkY = y + 1; checkY < CHUNK_HEIGHT; ++checkY) {
        if (chunk->getBlock(x, checkY, z).isOpaque()) {
            // Found an opaque block above in same chunk - underground
            return 0;
        }
    }
    
    // Now check the chunk above if available
    if (neighbors[2]) {
        for (int checkY = 0; checkY < CHUNK_HEIGHT; ++checkY) {
            if (neighbors[2]->getBlock(x, checkY, z).isOpaque()) {
                // Found an opaque block in chunk above - underground  
                return 0;
            }
        }
    }
    
    // We've checked current chunk and one above - no obstructions found
    // If we can reach above typical terrain height, we have sky access
    int maxCheckedWorldY = static_cast<int>(chunkPos.y + (neighbors[2] ? 2 : 1)) * CHUNK_HEIGHT;
    
    // If we've checked up to or past typical terrain surface, we have clear sky
    // TERRAIN_HEIGHT is 64, so if we've checked past ~70-80 we're good
    if (maxCheckedWorldY >= TERRAIN_HEIGHT + 16) {
        return 15; // Full sky access - no blocks found above
    }
    
    // We couldn't check high enough to be certain, but found no obstructions
    // in everything we could check. Be optimistic - if there were blocks above,
    // they would likely have been within our check range for normal terrain.
    // This handles open pits, ravines, and low terrain correctly.
    return 15;
}
