#include "Item.h"

ItemProperties ItemRegistry::emptyProperties;

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry inst;
    return inst;
}

ItemRegistry::ItemRegistry() {
    // Base path for tool models
    const std::string basePath = "assets/models/Tools/";
    
    // === SWORDS ===
    // Swords deal bonus damage but don't mine effectively
    registerItem(ItemType::SWORD_WOOD, {
        "Wooden Sword",
        basePath + "Sword_Wood.gltf",
        ToolCategory::SWORD,
        MaterialTier::WOOD,
        4.0f,   // attackDamage (MC: 4)
        1.6f,   // attackSpeed (MC: 1.6)
        0.4f,   // knockback
        1.0f,   // miningSpeed (not a mining tool)
        60      // durability (MC: 60)
    });
    
    registerItem(ItemType::SWORD_STONE, {
        "Stone Sword",
        basePath + "Sword_Stone.gltf",
        ToolCategory::SWORD,
        MaterialTier::STONE,
        5.0f,   // attackDamage (MC: 5)
        1.6f,   // attackSpeed
        0.4f,   // knockback
        1.0f,   // miningSpeed
        132     // durability (MC: 132)
    });
    
    registerItem(ItemType::SWORD_GOLD, {
        "Golden Sword",
        basePath + "Sword_Gold.gltf",
        ToolCategory::SWORD,
        MaterialTier::GOLD,
        4.0f,   // attackDamage (MC: 4, same as wood)
        1.6f,   // attackSpeed
        0.4f,   // knockback
        1.0f,   // miningSpeed
        33      // durability (MC: 33, very low)
    });
    
    registerItem(ItemType::SWORD_DIAMOND, {
        "Diamond Sword",
        basePath + "Sword_Diamond.gltf",
        ToolCategory::SWORD,
        MaterialTier::DIAMOND,
        7.0f,   // attackDamage (MC: 7)
        1.6f,   // attackSpeed
        0.4f,   // knockback
        1.0f,   // miningSpeed
        1562    // durability (MC: 1562)
    });
    
    // === PICKAXES ===
    // Pickaxes are effective against stone, ores, metal blocks
    registerItem(ItemType::PICKAXE_WOOD, {
        "Wooden Pickaxe",
        basePath + "Pickaxe_Wood.gltf",
        ToolCategory::PICKAXE,
        MaterialTier::WOOD,
        2.0f,   // attackDamage (MC: 2)
        1.2f,   // attackSpeed (MC: 1.2)
        0.4f,   // knockback
        2.0f,   // miningSpeed (MC: 2x on stone)
        60      // durability
    });
    
    registerItem(ItemType::PICKAXE_STONE, {
        "Stone Pickaxe",
        basePath + "Pickaxe_Stone.gltf",
        ToolCategory::PICKAXE,
        MaterialTier::STONE,
        3.0f,   // attackDamage (MC: 3)
        1.2f,   // attackSpeed
        0.4f,   // knockback
        4.0f,   // miningSpeed (MC: 4x on stone)
        132     // durability
    });
    
    registerItem(ItemType::PICKAXE_GOLD, {
        "Golden Pickaxe",
        basePath + "Pickaxe_Gold.gltf",
        ToolCategory::PICKAXE,
        MaterialTier::GOLD,
        2.0f,   // attackDamage (MC: 2)
        1.2f,   // attackSpeed
        0.4f,   // knockback
        12.0f,  // miningSpeed (MC: 12x, fastest!)
        33      // durability
    });
    
    registerItem(ItemType::PICKAXE_DIAMOND, {
        "Diamond Pickaxe",
        basePath + "Pickaxe_Diamond.gltf",
        ToolCategory::PICKAXE,
        MaterialTier::DIAMOND,
        5.0f,   // attackDamage (MC: 5)
        1.2f,   // attackSpeed
        0.4f,   // knockback
        8.0f,   // miningSpeed (MC: 8x)
        1562    // durability
    });
    
    // === AXES ===
    // Axes are effective against wood, deal high damage but slow
    registerItem(ItemType::AXE_WOOD, {
        "Wooden Axe",
        basePath + "Axe_Wood.gltf",
        ToolCategory::AXE,
        MaterialTier::WOOD,
        7.0f,   // attackDamage (MC: 7, high!)
        0.8f,   // attackSpeed (MC: 0.8, slow!)
        0.4f,   // knockback
        2.0f,   // miningSpeed
        60      // durability
    });
    
    registerItem(ItemType::AXE_STONE, {
        "Stone Axe",
        basePath + "Axe_Stone.gltf",
        ToolCategory::AXE,
        MaterialTier::STONE,
        9.0f,   // attackDamage (MC: 9)
        0.8f,   // attackSpeed
        0.4f,   // knockback
        4.0f,   // miningSpeed
        132     // durability
    });
    
    registerItem(ItemType::AXE_GOLD, {
        "Golden Axe",
        basePath + "Axe_Gold.gltf",
        ToolCategory::AXE,
        MaterialTier::GOLD,
        7.0f,   // attackDamage (MC: 7)
        1.0f,   // attackSpeed (MC: 1.0, slightly faster)
        0.4f,   // knockback
        12.0f,  // miningSpeed
        33      // durability
    });
    
    registerItem(ItemType::AXE_DIAMOND, {
        "Diamond Axe",
        basePath + "Axe_Diamond.gltf",
        ToolCategory::AXE,
        MaterialTier::DIAMOND,
        9.0f,   // attackDamage (MC: 9)
        1.0f,   // attackSpeed
        0.4f,   // knockback
        8.0f,   // miningSpeed
        1562    // durability
    });
    
    // === SHOVELS ===
    // Shovels are effective against dirt, sand, gravel, snow
    registerItem(ItemType::SHOVEL_WOOD, {
        "Wooden Shovel",
        basePath + "Shovel_Wood.gltf",
        ToolCategory::SHOVEL,
        MaterialTier::WOOD,
        2.5f,   // attackDamage (MC: 2.5)
        1.0f,   // attackSpeed (MC: 1.0)
        0.4f,   // knockback
        2.0f,   // miningSpeed
        60      // durability
    });
    
    registerItem(ItemType::SHOVEL_STONE, {
        "Stone Shovel",
        basePath + "Shovel_Stone.gltf",
        ToolCategory::SHOVEL,
        MaterialTier::STONE,
        3.5f,   // attackDamage (MC: 3.5)
        1.0f,   // attackSpeed
        0.4f,   // knockback
        4.0f,   // miningSpeed
        132     // durability
    });
    
    registerItem(ItemType::SHOVEL_GOLD, {
        "Golden Shovel",
        basePath + "Shovel_Gold.gltf",
        ToolCategory::SHOVEL,
        MaterialTier::GOLD,
        2.5f,   // attackDamage (MC: 2.5)
        1.0f,   // attackSpeed
        0.4f,   // knockback
        12.0f,  // miningSpeed
        33      // durability
    });
    
    registerItem(ItemType::SHOVEL_DIAMOND, {
        "Diamond Shovel",
        basePath + "Shovel_Diamond.gltf",
        ToolCategory::SHOVEL,
        MaterialTier::DIAMOND,
        5.5f,   // attackDamage (MC: 5.5)
        1.0f,   // attackSpeed
        0.4f,   // knockback
        8.0f,   // miningSpeed
        1562    // durability
    });
}

void ItemRegistry::registerItem(ItemType type, const ItemProperties& props) {
    items[type] = props;
}

const ItemProperties& ItemRegistry::getProperties(ItemType type) const {
    auto it = items.find(type);
    if (it != items.end()) {
        return it->second;
    }
    return emptyProperties;
}

ItemType ItemRegistry::getItemFromIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(ItemType::COUNT) - 1) {
        return ItemType::NONE;
    }
    return static_cast<ItemType>(index + 1); // +1 because NONE is 0
}

ToolCategory ItemRegistry::getCategory(ItemType type) {
    if (type >= ItemType::SWORD_WOOD && type <= ItemType::SWORD_DIAMOND) {
        return ToolCategory::SWORD;
    }
    if (type >= ItemType::PICKAXE_WOOD && type <= ItemType::PICKAXE_DIAMOND) {
        return ToolCategory::PICKAXE;
    }
    if (type >= ItemType::AXE_WOOD && type <= ItemType::AXE_DIAMOND) {
        return ToolCategory::AXE;
    }
    if (type >= ItemType::SHOVEL_WOOD && type <= ItemType::SHOVEL_DIAMOND) {
        return ToolCategory::SHOVEL;
    }
    return ToolCategory::NONE;
}

MaterialTier ItemRegistry::getTier(ItemType type) {
    int typeVal = static_cast<int>(type);
    if (typeVal == 0) return MaterialTier::NONE;
    
    // Each tool category has 4 tiers: wood, stone, gold, diamond
    int tierIndex = (typeVal - 1) % 4;
    switch (tierIndex) {
        case 0: return MaterialTier::WOOD;
        case 1: return MaterialTier::STONE;
        case 2: return MaterialTier::GOLD;
        case 3: return MaterialTier::DIAMOND;
        default: return MaterialTier::NONE;
    }
}

std::string ItemRegistry::getModelPath(ItemType type) {
    return instance().getProperties(type).modelPath;
}

std::string ItemRegistry::getDisplayName(ItemType type) {
    return instance().getProperties(type).name;
}

float ItemRegistry::getMiningMultiplier(ItemType tool, BlockType block) const {
    if (tool == ItemType::NONE) return 1.0f;
    
    const auto& props = getProperties(tool);
    
    // Check if this tool is effective on this block type
    if (!props.isEffectiveOn(block)) {
        return 1.0f; // No bonus
    }
    
    return props.miningSpeed;
}

float ItemRegistry::getAttackDamage(ItemType weapon) const {
    if (weapon == ItemType::NONE) return 1.0f; // Fist damage
    return getProperties(weapon).attackDamage;
}

float ItemRegistry::getKnockback(ItemType weapon) const {
    if (weapon == ItemType::NONE) return 0.3f; // Base knockback
    return getProperties(weapon).knockback;
}

bool ItemProperties::isEffectiveOn(BlockType block) const {
    switch (category) {
        case ToolCategory::PICKAXE:
            // Effective on stone-like blocks
            return block == BlockType::STONE ||
                   block == BlockType::SANDSTONE ||
                   block == BlockType::GRAVEL ||
                   block == BlockType::ICE ||
                   block == BlockType::BEDROCK;
                   
        case ToolCategory::AXE:
            // Effective on wood-like blocks
            return block == BlockType::WOOD ||
                   block == BlockType::LOG ||
                   block == BlockType::LEAVES;
                   
        case ToolCategory::SHOVEL:
            // Effective on soft blocks
            return block == BlockType::DIRT ||
                   block == BlockType::GRASS ||
                   block == BlockType::SAND ||
                   block == BlockType::GRAVEL ||
                   block == BlockType::SNOW;
                   
        case ToolCategory::SWORD:
            // Swords are not mining tools, but can cut some things
            return block == BlockType::LEAVES ||
                   block == BlockType::TALL_GRASS ||
                   block == BlockType::ROSE;
                   
        default:
            return false;
    }
}
