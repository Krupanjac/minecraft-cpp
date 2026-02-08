#pragma once

#include "Block.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// A single block placement in a structure
struct StructureBlock {
    glm::ivec3 position;      // Relative position within structure (origin at 0,0,0)
    BlockType type;
    uint8_t metadata = 0;     // For rotation, variant, etc.
};

// Marker for special positions in structures (doors, spawns, etc.)
struct StructureMarker {
    glm::ivec3 position;
    std::string type;         // "door", "spawn", "chest", "bed", etc.
    std::string data;         // Additional data (JSON string)
};

// Structure categories for organization
enum class StructureCategory {
    VILLAGE_HOUSE,
    VILLAGE_BUILDING,     // Generic village building (like blacksmith)
    VILLAGE_FARM,
    VILLAGE_WELL,
    VILLAGE_PATH,
    VILLAGE_DECORATION,
    CITY_BUILDING,
    CITY_SKYSCRAPER,
    CITY_ROAD,
    CITY_PARK,
    CITY_DECORATION,
    MISC
};

// Main structure class - represents a placeable multi-block structure
class Structure {
public:
    Structure() = default;
    Structure(const std::string& name);
    
    // Load/Save
    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;
    
    // Building the structure (for editor)
    void setBlock(const glm::ivec3& pos, BlockType type, uint8_t metadata = 0);
    void removeBlock(const glm::ivec3& pos);
    void addMarker(const glm::ivec3& pos, const std::string& type, const std::string& data = "");
    void removeMarker(const glm::ivec3& pos);
    void clear();
    
    // Querying
    BlockType getBlock(const glm::ivec3& pos) const;
    uint8_t getBlockMetadata(const glm::ivec3& pos) const;
    bool hasBlock(const glm::ivec3& pos) const;
    const std::vector<StructureBlock>& getBlocks() const { return m_blocks; }
    const std::vector<StructureMarker>& getMarkers() const { return m_markers; }
    
    // Properties
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    const std::string& getAuthor() const { return m_author; }
    void setAuthor(const std::string& author) { m_author = author; }
    
    StructureCategory getCategory() const { return m_category; }
    void setCategory(StructureCategory cat) { m_category = cat; }
    
    // Bounding box
    glm::ivec3 getMinBounds() const { return m_minBounds; }
    glm::ivec3 getMaxBounds() const { return m_maxBounds; }
    glm::ivec3 getSize() const { return m_maxBounds - m_minBounds + glm::ivec3(1); }
    
    // Rotation helpers (0=0°, 1=90°, 2=180°, 3=270°)
    std::vector<StructureBlock> getRotatedBlocks(int rotation) const;
    
    // Tags for filtering
    void addTag(const std::string& tag) { m_tags.push_back(tag); }
    const std::vector<std::string>& getTags() const { return m_tags; }
    bool hasTag(const std::string& tag) const;
    
    // Placement requirements
    void setRequiresFlat(bool flat) { m_requiresFlat = flat; }
    bool requiresFlat() const { return m_requiresFlat; }
    
    void setMinGroundCoverage(float coverage) { m_minGroundCoverage = coverage; }
    float getMinGroundCoverage() const { return m_minGroundCoverage; }

private:
    void recalculateBounds();
    
    std::string m_name = "unnamed";
    std::string m_author = "unknown";
    StructureCategory m_category = StructureCategory::MISC;
    std::vector<std::string> m_tags;
    
    std::vector<StructureBlock> m_blocks;
    std::vector<StructureMarker> m_markers;
    
    glm::ivec3 m_minBounds = glm::ivec3(0);
    glm::ivec3 m_maxBounds = glm::ivec3(0);
    
    // Placement requirements
    bool m_requiresFlat = true;
    float m_minGroundCoverage = 0.7f;  // 70% of footprint must be solid ground
};

// Registry to manage all loaded structures
class StructureRegistry {
public:
    static StructureRegistry& instance();
    
    // Load all structures from directory
    void loadStructuresFromDirectory(const std::string& directory);
    
    // Individual structure management
    void registerStructure(const std::string& id, std::shared_ptr<Structure> structure);
    std::shared_ptr<Structure> getStructure(const std::string& id) const;
    bool hasStructure(const std::string& id) const;
    
    // Querying
    std::vector<std::string> getStructuresByCategory(StructureCategory category) const;
    std::vector<std::string> getStructuresByTag(const std::string& tag) const;
    std::vector<std::string> getAllStructureIds() const;
    
    // For random selection during generation
    std::shared_ptr<Structure> getRandomStructure(StructureCategory category, unsigned int seed) const;

private:
    StructureRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<Structure>> m_structures;
};
