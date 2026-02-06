#pragma once

#include "Structure.h"
#include "WorldGenerator.h"  // For BiomeType enum
#include <memory>
#include <unordered_map>
#include <unordered_set>

// Forward declarations
class Chunk;
class ChunkManager;

// Unique ID for structure placements to avoid duplicates
struct StructurePlacementKey {
    int gridX;
    int gridZ;
    
    bool operator==(const StructurePlacementKey& other) const {
        return gridX == other.gridX && gridZ == other.gridZ;
    }
};

namespace std {
    template<>
    struct hash<StructurePlacementKey> {
        size_t operator()(const StructurePlacementKey& key) const {
            return hash<int>()(key.gridX) ^ (hash<int>()(key.gridZ) << 16);
        }
    };
}

// Tracks a placed structure instance
struct PlacedStructure {
    std::string structureName;
    int worldX, worldY, worldZ;    // World position
    int rotation;                   // 0, 90, 180, 270 degrees
    bool generated;                 // Has this been written to chunks?
};

// Manages structure placement during world generation
class StructurePlacer {
public:
    StructurePlacer(unsigned int seed = 12345);
    ~StructurePlacer() = default;
    
    // Initialize with structure registry
    void initialize(const std::string& structuresPath);
    
    // Set seed for reproducible generation
    void setSeed(unsigned int seed);
    
    // Generate structure placements for a region
    // Called when chunks are first generated to determine what structures exist
    void planStructuresForChunk(int chunkX, int chunkZ, BiomeType biome, 
                                 const WorldGenerator& worldGen);
    
    // Place structure blocks into a chunk during generation
    // Returns blocks that should be placed at given world coordinates
    void placeStructuresInChunk(std::shared_ptr<Chunk> chunk, ChunkManager& chunkManager);
    
    // Grid spacing for village/city layout (in blocks)
    static constexpr int VILLAGE_GRID_SIZE = 24;   // Village structures every 24 blocks
    static constexpr int CITY_GRID_SIZE = 12;      // City structures every 12 blocks (dense)
    
    // Check if a world position has a structure
    bool hasStructureAt(int worldX, int worldZ) const;
    
private:
    unsigned int seed;
    
    // Placed structures indexed by grid position
    std::unordered_map<StructurePlacementKey, PlacedStructure> placedStructures;
    
    // Track which chunks have been planned
    std::unordered_set<uint64_t> plannedChunks;
    
    // Simple hash for deterministic placement
    unsigned int hashPosition(int x, int z) const;
    
    // Pick a random structure from a category
    std::shared_ptr<Structure> pickStructure(StructureCategory category, unsigned int localSeed) const;
    
    // Check if terrain is suitable for structure placement
    bool isTerrainSuitable(int worldX, int worldZ, const Structure& structure,
                           const WorldGenerator& worldGen) const;
    
    // Get grid position for world coordinates
    void getGridPosition(int worldX, int worldZ, BiomeType biome, int& gridX, int& gridZ) const;
    
    // Pack chunk coords into single value for set
    static uint64_t packChunkCoords(int cx, int cz) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | 
               static_cast<uint64_t>(static_cast<uint32_t>(cz));
    }
};
