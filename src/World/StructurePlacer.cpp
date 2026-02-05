#include "StructurePlacer.h"
#include "Chunk.h"
#include "WorldGenerator.h"
#include "ChunkManager.h"
#include "../Util/Config.h"
#include <random>
#include <iostream>
#include <cmath>

StructurePlacer::StructurePlacer(unsigned int seed) : seed(seed) {}

void StructurePlacer::initialize(const std::string& structuresPath) {
    StructureRegistry::instance().loadStructuresFromDirectory(structuresPath);
    std::cout << "StructurePlacer initialized with " << StructureRegistry::instance().getAllStructureIds().size() 
              << " structures" << std::endl;
}

void StructurePlacer::setSeed(unsigned int s) {
    seed = s;
    placedStructures.clear();
    plannedChunks.clear();
}

unsigned int StructurePlacer::hashPosition(int x, int z) const {
    // Simple hash combining seed and position for deterministic results
    unsigned int h = seed;
    h ^= static_cast<unsigned int>(x) * 374761393u;
    h = ((h << 17) | (h >> 15)) * 3266489917u;
    h ^= static_cast<unsigned int>(z) * 668265263u;
    h = ((h << 13) | (h >> 19)) * 2654435761u;
    return h;
}

void StructurePlacer::getGridPosition(int worldX, int worldZ, BiomeType biome, 
                                       int& gridX, int& gridZ) const {
    int gridSize = (biome == BiomeType::CITY) ? CITY_GRID_SIZE : VILLAGE_GRID_SIZE;
    
    // Floor division to get grid cell
    gridX = (worldX >= 0) ? (worldX / gridSize) : ((worldX - gridSize + 1) / gridSize);
    gridZ = (worldZ >= 0) ? (worldZ / gridSize) : ((worldZ - gridSize + 1) / gridSize);
}

std::shared_ptr<Structure> StructurePlacer::pickStructure(StructureCategory category, 
                                                           unsigned int localSeed) const {
    auto structureIds = StructureRegistry::instance().getStructuresByCategory(category);
    if (structureIds.empty()) return nullptr;
    
    // Use local seed to deterministically pick a structure
    size_t index = localSeed % structureIds.size();
    return StructureRegistry::instance().getStructure(structureIds[index]);
}

bool StructurePlacer::isTerrainSuitable(int worldX, int worldZ, const Structure& structure,
                                         const WorldGenerator& worldGen) const {
    if (!structure.requiresFlat()) return true;
    
    // Check terrain flatness across structure footprint
    glm::ivec3 size = structure.getSize();
    int sizeX = size.x;
    int sizeZ = size.z;
    
    // Sample heights at corners and center
    std::vector<int> heights;
    heights.push_back(worldGen.getSurfaceHeight(worldX, worldZ));
    heights.push_back(worldGen.getSurfaceHeight(worldX + sizeX - 1, worldZ));
    heights.push_back(worldGen.getSurfaceHeight(worldX, worldZ + sizeZ - 1));
    heights.push_back(worldGen.getSurfaceHeight(worldX + sizeX - 1, worldZ + sizeZ - 1));
    heights.push_back(worldGen.getSurfaceHeight(worldX + sizeX / 2, worldZ + sizeZ / 2));
    
    // Find height variation
    int minH = heights[0], maxH = heights[0];
    for (int h : heights) {
        minH = std::min(minH, h);
        maxH = std::max(maxH, h);
    }
    
    // Allow up to 3 blocks of variation for "flat" terrain
    return (maxH - minH) <= 3;
}

void StructurePlacer::planStructuresForChunk(int chunkX, int chunkZ, BiomeType biome,
                                              const WorldGenerator& worldGen) {
    // Skip if not a settlement biome
    if (biome != BiomeType::VILLAGE && biome != BiomeType::CITY) return;
    
    uint64_t chunkKey = packChunkCoords(chunkX, chunkZ);
    if (plannedChunks.count(chunkKey)) return;
    plannedChunks.insert(chunkKey);
    
    int gridSize = (biome == BiomeType::CITY) ? CITY_GRID_SIZE : VILLAGE_GRID_SIZE;
    
    // Calculate which grid cells this chunk overlaps
    int chunkMinX = chunkX * CHUNK_SIZE;
    int chunkMinZ = chunkZ * CHUNK_SIZE;
    int chunkMaxX = chunkMinX + CHUNK_SIZE - 1;
    int chunkMaxZ = chunkMinZ + CHUNK_SIZE - 1;
    
    int gridMinX, gridMinZ, gridMaxX, gridMaxZ;
    getGridPosition(chunkMinX, chunkMinZ, biome, gridMinX, gridMinZ);
    getGridPosition(chunkMaxX, chunkMaxZ, biome, gridMaxX, gridMaxZ);
    
    // Plan structures for each grid cell in chunk range
    for (int gx = gridMinX; gx <= gridMaxX; gx++) {
        for (int gz = gridMinZ; gz <= gridMaxZ; gz++) {
            StructurePlacementKey key{gx, gz};
            
            // Already planned?
            if (placedStructures.count(key)) continue;
            
            // Get placement position from grid
            int worldX = gx * gridSize;
            int worldZ = gz * gridSize;
            
            // Hash to determine if structure spawns and which type
            unsigned int localSeed = hashPosition(gx, gz);
            
            // Probability of structure spawning (higher in city)
            float spawnChance = (biome == BiomeType::CITY) ? 0.85f : 0.65f;
            float roll = static_cast<float>(localSeed % 1000) / 1000.0f;
            
            if (roll > spawnChance) continue;
            
            // Pick structure category based on biome and random variation
            StructureCategory category;
            if (biome == BiomeType::CITY) {
                // City structure distribution
                unsigned int typeRoll = (localSeed >> 8) % 100;
                if (typeRoll < 50) category = StructureCategory::CITY_BUILDING;
                else if (typeRoll < 80) category = StructureCategory::CITY_ROAD;
                else category = StructureCategory::CITY_DECORATION;
            } else {
                // Village structure distribution
                unsigned int typeRoll = (localSeed >> 8) % 100;
                if (typeRoll < 45) category = StructureCategory::VILLAGE_HOUSE;
                else if (typeRoll < 60) category = StructureCategory::VILLAGE_FARM;
                else if (typeRoll < 75) category = StructureCategory::VILLAGE_WELL;
                else category = StructureCategory::VILLAGE_PATH;
            }
            
            // Pick specific structure
            std::shared_ptr<Structure> structure = pickStructure(category, localSeed >> 16);
            if (!structure) continue;
            
            // Get surface height at placement point
            glm::ivec3 size = structure->getSize();
            int surfaceY = worldGen.getSurfaceHeight(worldX + size.x / 2,
                                                      worldZ + size.z / 2);
            
            // Check terrain suitability
            if (!isTerrainSuitable(worldX, worldZ, *structure, worldGen)) continue;
            
            // Random rotation (0, 90, 180, 270)
            int rotation = ((localSeed >> 24) % 4) * 90;
            
            // Store placement
            PlacedStructure placed;
            placed.structureName = structure->getName();
            placed.worldX = worldX;
            placed.worldY = surfaceY;
            placed.worldZ = worldZ;
            placed.rotation = rotation;
            placed.generated = false;
            
            placedStructures[key] = placed;
        }
    }
}

void StructurePlacer::placeStructuresInChunk(std::shared_ptr<Chunk> chunk, 
                                              ChunkManager& chunkManager) {
    int chunkX = chunk->getPosition().x;
    int chunkZ = chunk->getPosition().z;
    int chunkMinX = chunkX * CHUNK_SIZE;
    int chunkMinZ = chunkZ * CHUNK_SIZE;
    int chunkMaxX = chunkMinX + CHUNK_SIZE - 1;
    int chunkMaxZ = chunkMinZ + CHUNK_SIZE - 1;
    
    // Check all placed structures
    for (auto& pair : placedStructures) {
        PlacedStructure& placed = pair.second;
        
        // Get structure definition
        std::shared_ptr<Structure> structure = StructureRegistry::instance().getStructure(placed.structureName);
        if (!structure) continue;
        
        // Check if structure overlaps this chunk
        glm::ivec3 structSize = structure->getSize();
        int structMinX = placed.worldX;
        int structMinZ = placed.worldZ;
        int structMaxX = placed.worldX + structSize.x - 1;
        int structMaxZ = placed.worldZ + structSize.z - 1;
        
        // No overlap with this chunk?
        if (structMaxX < chunkMinX || structMinX > chunkMaxX ||
            structMaxZ < chunkMinZ || structMinZ > chunkMaxZ) {
            continue;
        }
        
        // Get rotated blocks
        auto blocks = structure->getRotatedBlocks(placed.rotation);
        
        // Place blocks that fall within this chunk
        for (const auto& block : blocks) {
            int worldX = placed.worldX + block.position.x;
            int worldY = placed.worldY + block.position.y;
            int worldZ = placed.worldZ + block.position.z;
            
            // Within chunk bounds?
            if (worldX < chunkMinX || worldX > chunkMaxX ||
                worldZ < chunkMinZ || worldZ > chunkMaxZ) {
                continue;
            }
            
            // Within world height bounds?
            if (worldY < 0 || worldY >= CHUNK_HEIGHT) continue;
            
            // Convert to local chunk coordinates
            int localX = worldX - chunkMinX;
            int localZ = worldZ - chunkMinZ;
            
            // Set block in chunk
            chunk->setBlock(localX, worldY, localZ, block.type);
        }
    }
}

bool StructurePlacer::hasStructureAt(int worldX, int worldZ) const {
    // Check a nearby radius for structures
    for (const auto& pair : placedStructures) {
        const PlacedStructure& placed = pair.second;
        std::shared_ptr<Structure> structure = StructureRegistry::instance().getStructure(placed.structureName);
        if (!structure) continue;
        
        glm::ivec3 structSize = structure->getSize();
        if (worldX >= placed.worldX && worldX < placed.worldX + structSize.x &&
            worldZ >= placed.worldZ && worldZ < placed.worldZ + structSize.z) {
            return true;
        }
    }
    return false;
}
