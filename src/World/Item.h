#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "Block.h"

// Item types - tools and weapons
enum class ItemType : uint8_t {
    NONE = 0,
    
    // Swords
    SWORD_WOOD = 1,
    SWORD_STONE = 2,
    SWORD_GOLD = 3,
    SWORD_DIAMOND = 4,
    
    // Pickaxes
    PICKAXE_WOOD = 5,
    PICKAXE_STONE = 6,
    PICKAXE_GOLD = 7,
    PICKAXE_DIAMOND = 8,
    
    // Axes
    AXE_WOOD = 9,
    AXE_STONE = 10,
    AXE_GOLD = 11,
    AXE_DIAMOND = 12,
    
    // Shovels
    SHOVEL_WOOD = 13,
    SHOVEL_STONE = 14,
    SHOVEL_GOLD = 15,
    SHOVEL_DIAMOND = 16,
    
    COUNT
};

// Tool category for determining effectiveness
enum class ToolCategory : uint8_t {
    NONE = 0,
    SWORD,
    PICKAXE,
    AXE,
    SHOVEL
};

// Material tier affects damage, mining speed, and durability
enum class MaterialTier : uint8_t {
    NONE = 0,
    WOOD = 1,
    STONE = 2,
    GOLD = 3,     // Gold is fast but weak
    DIAMOND = 4
};

// Properties for each item type
struct ItemProperties {
    std::string name;
    std::string modelPath;           // Path to GLTF model
    ToolCategory category = ToolCategory::NONE;
    MaterialTier tier = MaterialTier::NONE;
    
    // Combat stats
    float attackDamage = 1.0f;       // Damage dealt to entities
    float attackSpeed = 4.0f;        // Attacks per second (Minecraft default is 4)
    float knockback = 0.4f;          // Knockback strength
    
    // Mining stats
    float miningSpeed = 1.0f;        // Multiplier for block breaking speed
    int durability = 0;              // 0 = infinite (for creative mode items)
    
    // Can this tool harvest specific block types efficiently?
    bool isEffectiveOn(BlockType block) const;
};

// Singleton item registry
class ItemRegistry {
public:
    static ItemRegistry& instance();
    
    const ItemProperties& getProperties(ItemType type) const;
    ItemType getItemFromIndex(int index) const;
    int getItemCount() const { return static_cast<int>(ItemType::COUNT) - 1; } // Exclude NONE
    
    // Get tool category from item type
    static ToolCategory getCategory(ItemType type);
    static MaterialTier getTier(ItemType type);
    static std::string getModelPath(ItemType type);
    static std::string getDisplayName(ItemType type);
    
    // Mining speed calculation
    float getMiningMultiplier(ItemType tool, BlockType block) const;
    
    // Combat damage calculation
    float getAttackDamage(ItemType weapon) const;
    float getKnockback(ItemType weapon) const;

private:
    ItemRegistry();
    void registerItem(ItemType type, const ItemProperties& props);
    
    std::unordered_map<ItemType, ItemProperties> items;
    static ItemProperties emptyProperties;
};

// An item stack (for inventory)
struct ItemStack {
    ItemType type = ItemType::NONE;
    int count = 0;
    int durability = 0;  // Current durability remaining
    
    ItemStack() = default;
    ItemStack(ItemType t, int c = 1) : type(t), count(c) {
        durability = ItemRegistry::instance().getProperties(t).durability;
    }
    
    bool isEmpty() const { return type == ItemType::NONE || count <= 0; }
    bool isBlock() const { return false; } // Items are not blocks
    bool isTool() const { return type != ItemType::NONE; }
    
    const ItemProperties& getProperties() const {
        return ItemRegistry::instance().getProperties(type);
    }
};

// Hotbar slot can hold either a block or an item
struct HotbarSlot {
    bool isItem = false;
    BlockType blockType = BlockType::AIR;
    ItemStack itemStack;
    
    HotbarSlot() = default;
    HotbarSlot(BlockType block) : isItem(false), blockType(block) {}
    HotbarSlot(ItemType item) : isItem(true), itemStack(item, 1) {}
    
    bool isEmpty() const {
        return isItem ? itemStack.isEmpty() : blockType == BlockType::AIR;
    }
};

