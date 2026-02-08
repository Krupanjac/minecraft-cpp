#include "Structure.h"
#include "../Core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper to convert BlockType to/from string
static std::string blockTypeToString(BlockType type) {
    switch (type) {
        case BlockType::AIR: return "air";
        case BlockType::GRASS: return "grass";
        case BlockType::DIRT: return "dirt";
        case BlockType::STONE: return "stone";
        case BlockType::COBBLESTONE: return "cobblestone";
        case BlockType::SAND: return "sand";
        case BlockType::GRAVEL: return "gravel";
        case BlockType::WATER: return "water";
        case BlockType::BEDROCK: return "bedrock";
        case BlockType::OAK_LOG: return "oak_log";
        case BlockType::TALL_GRASS: return "tall_grass";
        case BlockType::ROSE: return "rose";
        case BlockType::LEAVES: return "leaves";
        case BlockType::OAK_LEAVES: return "oak_leaves";
        case BlockType::OAK_PLANKS: return "oak_planks";
        case BlockType::BIRCH_LOG: return "birch_log";
        case BlockType::BIRCH_LEAVES: return "birch_leaves";
        case BlockType::BIRCH_PLANKS: return "birch_planks";
        case BlockType::SPRUCE_LOG: return "spruce_log";
        case BlockType::SPRUCE_LEAVES: return "spruce_leaves";
        case BlockType::SPRUCE_PLANKS: return "spruce_planks";
        case BlockType::JUNGLE_LOG: return "jungle_log";
        case BlockType::JUNGLE_LEAVES: return "jungle_leaves";
        case BlockType::JUNGLE_PLANKS: return "jungle_planks";
        case BlockType::GLASS: return "glass";
        case BlockType::COAL_ORE: return "coal_ore";
        case BlockType::IRON_ORE: return "iron_ore";
        case BlockType::GOLD_ORE: return "gold_ore";
        case BlockType::DIAMOND_ORE: return "diamond_ore";
        case BlockType::REDSTONE_ORE: return "redstone_ore";
        case BlockType::EMERALD_ORE: return "emerald_ore";
        case BlockType::LAPIS_ORE: return "lapis_ore";
        case BlockType::SNOW: return "snow";
        case BlockType::ICE: return "ice";
        case BlockType::CLAY: return "clay";
        case BlockType::BRICKS: return "bricks";
        case BlockType::STONE_BRICKS: return "stone_bricks";
        case BlockType::COBWEB: return "cobweb";
        case BlockType::TNT: return "tnt";
        case BlockType::BOOKSHELF: return "bookshelf";
        case BlockType::MOSSY_COBBLESTONE: return "mossy_cobblestone";
        case BlockType::OBSIDIAN: return "obsidian";
        case BlockType::CRAFTING_TABLE: return "crafting_table";
        case BlockType::GLOWSTONE: return "glowstone";
        case BlockType::SANDSTONE: return "sandstone";
        case BlockType::SUGAR_CANE: return "sugar_cane";
        case BlockType::SPONGE: return "sponge";
        case BlockType::WHITE_WOOL: return "white_wool";
        case BlockType::ORANGE_WOOL: return "orange_wool";
        case BlockType::MAGENTA_WOOL: return "magenta_wool";
        case BlockType::LIGHT_BLUE_WOOL: return "light_blue_wool";
        case BlockType::YELLOW_WOOL: return "yellow_wool";
        case BlockType::LIME_WOOL: return "lime_wool";
        case BlockType::PINK_WOOL: return "pink_wool";
        case BlockType::GRAY_WOOL: return "gray_wool";
        case BlockType::LIGHT_GRAY_WOOL: return "light_gray_wool";
        case BlockType::CYAN_WOOL: return "cyan_wool";
        case BlockType::PURPLE_WOOL: return "purple_wool";
        case BlockType::BLUE_WOOL: return "blue_wool";
        case BlockType::BROWN_WOOL: return "brown_wool";
        case BlockType::GREEN_WOOL: return "green_wool";
        case BlockType::RED_WOOL: return "red_wool";
        case BlockType::BLACK_WOOL: return "black_wool";
        case BlockType::IRON_BLOCK: return "iron_block";
        case BlockType::GOLD_BLOCK: return "gold_block";
        case BlockType::DIAMOND_BLOCK: return "diamond_block";
        case BlockType::EMERALD_BLOCK: return "emerald_block";
        case BlockType::REDSTONE_BLOCK: return "redstone_block";
        case BlockType::MOSSY_STONE_BRICKS: return "mossy_stone_bricks";
        case BlockType::CRACKED_STONE_BRICKS: return "cracked_stone_bricks";
        case BlockType::CHISELED_STONE_BRICKS: return "chiseled_stone_bricks";
        case BlockType::REDSTONE_LAMP: return "redstone_lamp";
        case BlockType::FARMLAND: return "farmland";
        case BlockType::CHISELED_SANDSTONE: return "chiseled_sandstone";
        case BlockType::ROAD_STRAIGHT: return "road_straight";
        case BlockType::ROAD_LEFT: return "road_left";
        case BlockType::ROAD_RIGHT: return "road_right";
        case BlockType::ROAD_LEFT_RIGHT: return "road_left_right";
        case BlockType::ROAD_T_JUNCTION: return "road_t_junction";
        case BlockType::ROAD_INTERSECTION_YELLOW: return "road_intersection_yellow";
        case BlockType::ROAD_MIDDLE_LINES: return "road_middle_lines";
        case BlockType::ROAD_MIDDLE_LINES_YELLOW: return "road_middle_lines_yellow";
        case BlockType::ROAD_MIDDLE_RIGHT: return "road_middle_right";
        case BlockType::ROAD_MIDDLE_RIGHT_YELLOW: return "road_middle_right_yellow";
        case BlockType::ROAD_LEFT_DIAG_45: return "road_left_diag_45";
        case BlockType::ROAD_LEFT_DIAG_45_YELLOW: return "road_left_diag_45_yellow";
        case BlockType::ROAD_LEFT_DIAG_60: return "road_left_diag_60";
        case BlockType::ROAD_LEFT_DIAG_60_YELLOW: return "road_left_diag_60_yellow";
        case BlockType::ROAD_RIGHT_DIAG_60: return "road_right_diag_60";
        case BlockType::ROAD_RIGHT_DIAG_YELLOW: return "road_right_diag_yellow";
        case BlockType::GLAZED_TERRACOTTA: return "glazed_terracotta";
        default: return "unknown";
    }
}

static BlockType stringToBlockType(const std::string& str) {
    static std::unordered_map<std::string, BlockType> lookup = {
        {"air", BlockType::AIR},
        {"grass", BlockType::GRASS},
        {"dirt", BlockType::DIRT},
        {"stone", BlockType::STONE},
        {"cobblestone", BlockType::COBBLESTONE},
        {"sand", BlockType::SAND},
        {"gravel", BlockType::GRAVEL},
        {"water", BlockType::WATER},
        {"bedrock", BlockType::BEDROCK},
        {"oak_log", BlockType::OAK_LOG},
        {"tall_grass", BlockType::TALL_GRASS},
        {"rose", BlockType::ROSE},
        {"leaves", BlockType::LEAVES},
        {"oak_leaves", BlockType::OAK_LEAVES},
        {"oak_planks", BlockType::OAK_PLANKS},
        {"birch_log", BlockType::BIRCH_LOG},
        {"birch_leaves", BlockType::BIRCH_LEAVES},
        {"birch_planks", BlockType::BIRCH_PLANKS},
        {"spruce_log", BlockType::SPRUCE_LOG},
        {"spruce_leaves", BlockType::SPRUCE_LEAVES},
        {"spruce_planks", BlockType::SPRUCE_PLANKS},
        {"jungle_log", BlockType::JUNGLE_LOG},
        {"jungle_leaves", BlockType::JUNGLE_LEAVES},
        {"jungle_planks", BlockType::JUNGLE_PLANKS},
        {"glass", BlockType::GLASS},
        {"coal_ore", BlockType::COAL_ORE},
        {"iron_ore", BlockType::IRON_ORE},
        {"gold_ore", BlockType::GOLD_ORE},
        {"diamond_ore", BlockType::DIAMOND_ORE},
        {"redstone_ore", BlockType::REDSTONE_ORE},
        {"emerald_ore", BlockType::EMERALD_ORE},
        {"lapis_ore", BlockType::LAPIS_ORE},
        {"snow", BlockType::SNOW},
        {"ice", BlockType::ICE},
        {"clay", BlockType::CLAY},
        {"bricks", BlockType::BRICKS},
        {"stone_bricks", BlockType::STONE_BRICKS},
        {"cobweb", BlockType::COBWEB},
        {"tnt", BlockType::TNT},
        {"bookshelf", BlockType::BOOKSHELF},
        {"mossy_cobblestone", BlockType::MOSSY_COBBLESTONE},
        {"obsidian", BlockType::OBSIDIAN},
        {"crafting_table", BlockType::CRAFTING_TABLE},
        {"glowstone", BlockType::GLOWSTONE},
        {"sandstone", BlockType::SANDSTONE},
        {"sugar_cane", BlockType::SUGAR_CANE},
        {"sponge", BlockType::SPONGE},
        {"white_wool", BlockType::WHITE_WOOL},
        {"orange_wool", BlockType::ORANGE_WOOL},
        {"magenta_wool", BlockType::MAGENTA_WOOL},
        {"light_blue_wool", BlockType::LIGHT_BLUE_WOOL},
        {"yellow_wool", BlockType::YELLOW_WOOL},
        {"lime_wool", BlockType::LIME_WOOL},
        {"pink_wool", BlockType::PINK_WOOL},
        {"gray_wool", BlockType::GRAY_WOOL},
        {"light_gray_wool", BlockType::LIGHT_GRAY_WOOL},
        {"cyan_wool", BlockType::CYAN_WOOL},
        {"purple_wool", BlockType::PURPLE_WOOL},
        {"blue_wool", BlockType::BLUE_WOOL},
        {"brown_wool", BlockType::BROWN_WOOL},
        {"green_wool", BlockType::GREEN_WOOL},
        {"red_wool", BlockType::RED_WOOL},
        {"black_wool", BlockType::BLACK_WOOL},
        {"iron_block", BlockType::IRON_BLOCK},
        {"gold_block", BlockType::GOLD_BLOCK},
        {"diamond_block", BlockType::DIAMOND_BLOCK},
        {"emerald_block", BlockType::EMERALD_BLOCK},
        {"redstone_block", BlockType::REDSTONE_BLOCK},
        {"mossy_stone_bricks", BlockType::MOSSY_STONE_BRICKS},
        {"cracked_stone_bricks", BlockType::CRACKED_STONE_BRICKS},
        {"chiseled_stone_bricks", BlockType::CHISELED_STONE_BRICKS},
        {"redstone_lamp", BlockType::REDSTONE_LAMP},
        {"farmland", BlockType::FARMLAND},
        {"note_block", BlockType::NOTE_BLOCK},
        {"jukebox", BlockType::JUKEBOX},
        {"chiseled_sandstone", BlockType::CHISELED_SANDSTONE},
        {"wood", BlockType::WOOD},
        {"log", BlockType::LOG},
        {"road_straight", BlockType::ROAD_STRAIGHT},
        {"road_left", BlockType::ROAD_LEFT},
        {"road_right", BlockType::ROAD_RIGHT},
        {"road_left_right", BlockType::ROAD_LEFT_RIGHT},
        {"road_t_junction", BlockType::ROAD_T_JUNCTION},
        {"road_intersection_yellow", BlockType::ROAD_INTERSECTION_YELLOW},
        {"road_middle_lines", BlockType::ROAD_MIDDLE_LINES},
        {"road_middle_lines_yellow", BlockType::ROAD_MIDDLE_LINES_YELLOW},
        {"road_middle_right", BlockType::ROAD_MIDDLE_RIGHT},
        {"road_middle_right_yellow", BlockType::ROAD_MIDDLE_RIGHT_YELLOW},
        {"road_left_diag_45", BlockType::ROAD_LEFT_DIAG_45},
        {"road_left_diag_45_yellow", BlockType::ROAD_LEFT_DIAG_45_YELLOW},
        {"road_left_diag_60", BlockType::ROAD_LEFT_DIAG_60},
        {"road_left_diag_60_yellow", BlockType::ROAD_LEFT_DIAG_60_YELLOW},
        {"road_right_diag_60", BlockType::ROAD_RIGHT_DIAG_60},
        {"road_right_diag_yellow", BlockType::ROAD_RIGHT_DIAG_YELLOW},
        {"glazed_terracotta", BlockType::GLAZED_TERRACOTTA}
    };
    
    auto it = lookup.find(str);
    return (it != lookup.end()) ? it->second : BlockType::AIR;
}

static std::string categoryToString(StructureCategory cat) {
    switch (cat) {
        case StructureCategory::VILLAGE_HOUSE: return "village_house";
        case StructureCategory::VILLAGE_BUILDING: return "village_building";
        case StructureCategory::VILLAGE_FARM: return "village_farm";
        case StructureCategory::VILLAGE_WELL: return "village_well";
        case StructureCategory::VILLAGE_PATH: return "village_path";
        case StructureCategory::VILLAGE_DECORATION: return "village_decoration";
        case StructureCategory::CITY_BUILDING: return "city_building";
        case StructureCategory::CITY_SKYSCRAPER: return "city_skyscraper";
        case StructureCategory::CITY_ROAD: return "city_road";
        case StructureCategory::CITY_PARK: return "city_park";
        case StructureCategory::CITY_DECORATION: return "city_decoration";
        default: return "misc";
    }
}

static StructureCategory stringToCategory(const std::string& str) {
    static std::unordered_map<std::string, StructureCategory> lookup = {
        {"village_house", StructureCategory::VILLAGE_HOUSE},
        {"village_building", StructureCategory::VILLAGE_BUILDING},
        {"village_farm", StructureCategory::VILLAGE_FARM},
        {"village_well", StructureCategory::VILLAGE_WELL},
        {"village_path", StructureCategory::VILLAGE_PATH},
        {"village_decoration", StructureCategory::VILLAGE_DECORATION},
        {"city_building", StructureCategory::CITY_BUILDING},
        {"city_skyscraper", StructureCategory::CITY_SKYSCRAPER},
        {"city_road", StructureCategory::CITY_ROAD},
        {"city_park", StructureCategory::CITY_PARK},
        {"city_decoration", StructureCategory::CITY_DECORATION},
        {"misc", StructureCategory::MISC}
    };
    auto it = lookup.find(str);
    return (it != lookup.end()) ? it->second : StructureCategory::MISC;
}

Structure::Structure(const std::string& name) : m_name(name) {}

bool Structure::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open structure file: " + filepath);
            return false;
        }
        
        json data = json::parse(file);
        
        // Load metadata
        m_name = data.value("name", "unnamed");
        m_author = data.value("author", "unknown");
        m_category = stringToCategory(data.value("category", "misc"));
        m_requiresFlat = data.value("requires_flat", true);
        m_minGroundCoverage = data.value("min_ground_coverage", 0.7f);
        
        // Load tags
        m_tags.clear();
        if (data.contains("tags") && data["tags"].is_array()) {
            for (const auto& tag : data["tags"]) {
                m_tags.push_back(tag.get<std::string>());
            }
        }
        
        // Load blocks
        m_blocks.clear();
        if (data.contains("blocks") && data["blocks"].is_array()) {
            for (const auto& blockData : data["blocks"]) {
                StructureBlock block;
                block.position = glm::ivec3(
                    blockData["x"].get<int>(),
                    blockData["y"].get<int>(),
                    blockData["z"].get<int>()
                );
                block.type = stringToBlockType(blockData["type"].get<std::string>());
                block.metadata = blockData.value("metadata", 0);
                m_blocks.push_back(block);
            }
        }
        
        // Load markers
        m_markers.clear();
        if (data.contains("markers") && data["markers"].is_array()) {
            for (const auto& markerData : data["markers"]) {
                StructureMarker marker;
                marker.position = glm::ivec3(
                    markerData["x"].get<int>(),
                    markerData["y"].get<int>(),
                    markerData["z"].get<int>()
                );
                marker.type = markerData["type"].get<std::string>();
                marker.data = markerData.value("data", "");
                m_markers.push_back(marker);
            }
        }
        
        recalculateBounds();
        LOG_INFO("Loaded structure: " + m_name + " (" + std::to_string(m_blocks.size()) + " blocks)");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing structure file " + filepath + ": " + e.what());
        return false;
    }
}

bool Structure::saveToFile(const std::string& filepath) const {
    try {
        json data;
        
        // Save metadata
        data["name"] = m_name;
        data["author"] = m_author;
        data["category"] = categoryToString(m_category);
        data["requires_flat"] = m_requiresFlat;
        data["min_ground_coverage"] = m_minGroundCoverage;
        data["tags"] = m_tags;
        
        // Save bounds info
        data["size"] = {
            {"x", m_maxBounds.x - m_minBounds.x + 1},
            {"y", m_maxBounds.y - m_minBounds.y + 1},
            {"z", m_maxBounds.z - m_minBounds.z + 1}
        };
        
        // Save blocks
        json blocksArray = json::array();
        for (const auto& block : m_blocks) {
            json blockData;
            blockData["x"] = block.position.x;
            blockData["y"] = block.position.y;
            blockData["z"] = block.position.z;
            blockData["type"] = blockTypeToString(block.type);
            if (block.metadata != 0) {
                blockData["metadata"] = block.metadata;
            }
            blocksArray.push_back(blockData);
        }
        data["blocks"] = blocksArray;
        
        // Save markers
        json markersArray = json::array();
        for (const auto& marker : m_markers) {
            json markerData;
            markerData["x"] = marker.position.x;
            markerData["y"] = marker.position.y;
            markerData["z"] = marker.position.z;
            markerData["type"] = marker.type;
            if (!marker.data.empty()) {
                markerData["data"] = marker.data;
            }
            markersArray.push_back(markerData);
        }
        data["markers"] = markersArray;
        
        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create structure file: " + filepath);
            return false;
        }
        
        file << data.dump(2);  // Pretty print with 2-space indent
        LOG_INFO("Saved structure: " + m_name + " to " + filepath);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving structure: " + std::string(e.what()));
        return false;
    }
}

void Structure::setBlock(const glm::ivec3& pos, BlockType type, uint8_t metadata) {
    // Check if block already exists at position
    for (auto& block : m_blocks) {
        if (block.position == pos) {
            block.type = type;
            block.metadata = metadata;
            return;
        }
    }
    
    // Add new block
    StructureBlock block;
    block.position = pos;
    block.type = type;
    block.metadata = metadata;
    m_blocks.push_back(block);
    recalculateBounds();
}

void Structure::removeBlock(const glm::ivec3& pos) {
    m_blocks.erase(
        std::remove_if(m_blocks.begin(), m_blocks.end(),
            [&pos](const StructureBlock& b) { return b.position == pos; }),
        m_blocks.end()
    );
    recalculateBounds();
}

void Structure::addMarker(const glm::ivec3& pos, const std::string& type, const std::string& data) {
    // Remove existing marker at position
    removeMarker(pos);
    
    StructureMarker marker;
    marker.position = pos;
    marker.type = type;
    marker.data = data;
    m_markers.push_back(marker);
}

void Structure::removeMarker(const glm::ivec3& pos) {
    m_markers.erase(
        std::remove_if(m_markers.begin(), m_markers.end(),
            [&pos](const StructureMarker& m) { return m.position == pos; }),
        m_markers.end()
    );
}

void Structure::clear() {
    m_blocks.clear();
    m_markers.clear();
    m_minBounds = glm::ivec3(0);
    m_maxBounds = glm::ivec3(0);
}

BlockType Structure::getBlock(const glm::ivec3& pos) const {
    for (const auto& block : m_blocks) {
        if (block.position == pos) {
            return block.type;
        }
    }
    return BlockType::AIR;
}

uint8_t Structure::getBlockMetadata(const glm::ivec3& pos) const {
    for (const auto& block : m_blocks) {
        if (block.position == pos) {
            return block.metadata;
        }
    }
    return 0;
}

bool Structure::hasBlock(const glm::ivec3& pos) const {
    for (const auto& block : m_blocks) {
        if (block.position == pos) {
            return true;
        }
    }
    return false;
}

bool Structure::hasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

std::vector<StructureBlock> Structure::getRotatedBlocks(int rotation) const {
    std::vector<StructureBlock> rotated = m_blocks;
    
    if (rotation == 0) return rotated;
    
    // Calculate center for rotation
    glm::vec3 center = glm::vec3(m_maxBounds + m_minBounds) * 0.5f;
    
    for (auto& block : rotated) {
        glm::vec3 pos = glm::vec3(block.position) - center;
        
        for (int r = 0; r < rotation; ++r) {
            // Rotate 90 degrees around Y axis
            float newX = -pos.z;
            float newZ = pos.x;
            pos.x = newX;
            pos.z = newZ;
        }
        
        block.position = glm::ivec3(glm::round(pos + center));
        
        // Also rotate face rotation metadata to match the spatial rotation
        // Face rotation is stored in bits 0-1 of metadata (0-3 = 0/90/180/270 deg)
        uint8_t faceRot = block.metadata & 0x03;
        uint8_t otherBits = block.metadata & ~0x03;
        faceRot = (faceRot + rotation) & 0x03;
        block.metadata = otherBits | faceRot;
    }
    
    return rotated;
}

void Structure::recalculateBounds() {
    if (m_blocks.empty()) {
        m_minBounds = glm::ivec3(0);
        m_maxBounds = glm::ivec3(0);
        return;
    }
    
    m_minBounds = m_blocks[0].position;
    m_maxBounds = m_blocks[0].position;
    
    for (const auto& block : m_blocks) {
        m_minBounds = glm::min(m_minBounds, block.position);
        m_maxBounds = glm::max(m_maxBounds, block.position);
    }
}

// ============== StructureRegistry ==============

StructureRegistry& StructureRegistry::instance() {
    static StructureRegistry registry;
    return registry;
}

void StructureRegistry::loadStructuresFromDirectory(const std::string& directory) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(directory)) {
        LOG_WARNING("Structure directory does not exist: " + directory);
        fs::create_directories(directory);
        return;
    }
    
    int loaded = 0;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".vxstruct" || ext == ".json") {
                auto structure = std::make_shared<Structure>();
                if (structure->loadFromFile(entry.path().string())) {
                    std::string id = entry.path().stem().string();
                    registerStructure(id, structure);
                    loaded++;
                }
            }
        }
    }
    
    LOG_INFO("Loaded " + std::to_string(loaded) + " structures from " + directory);
}

void StructureRegistry::registerStructure(const std::string& id, std::shared_ptr<Structure> structure) {
    m_structures[id] = structure;
}

std::shared_ptr<Structure> StructureRegistry::getStructure(const std::string& id) const {
    auto it = m_structures.find(id);
    return (it != m_structures.end()) ? it->second : nullptr;
}

bool StructureRegistry::hasStructure(const std::string& id) const {
    return m_structures.find(id) != m_structures.end();
}

std::vector<std::string> StructureRegistry::getStructuresByCategory(StructureCategory category) const {
    std::vector<std::string> result;
    for (const auto& [id, structure] : m_structures) {
        if (structure->getCategory() == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> StructureRegistry::getStructuresByTag(const std::string& tag) const {
    std::vector<std::string> result;
    for (const auto& [id, structure] : m_structures) {
        if (structure->hasTag(tag)) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> StructureRegistry::getAllStructureIds() const {
    std::vector<std::string> result;
    result.reserve(m_structures.size());
    for (const auto& [id, _] : m_structures) {
        result.push_back(id);
    }
    return result;
}

std::shared_ptr<Structure> StructureRegistry::getRandomStructure(StructureCategory category, unsigned int seed) const {
    auto ids = getStructuresByCategory(category);
    if (ids.empty()) return nullptr;
    
    // Simple deterministic random
    size_t index = seed % ids.size();
    return getStructure(ids[index]);
}
