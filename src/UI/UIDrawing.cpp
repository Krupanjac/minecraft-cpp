#include "UIManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#endif

void UIManager::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    
    uiShader.setMat4("uModel", model);
    uiShader.setVec4("uColor", color);
    
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UIManager::drawTexturedRect(float x, float y, float w, float h, GLuint textureId) {
    if (textureId == 0) return;
    
    texturedShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    texturedShader.setMat4("uProjection", projection);
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    texturedShader.setMat4("uModel", model);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    texturedShader.setInt("uTexture", 0);
    
    glBindVertexArray(texturedVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // Switch back to UI shader for subsequent draws
    uiShader.use();
    uiShader.setMat4("uProjection", projection);
}

void UIManager::drawRoundedRect(float x, float y, float w, float h, float radius, const glm::vec4& color) {
    // For now, just draw a regular rect - true rounded corners would need more vertices
    // We can simulate with layered rects
    drawRect(x + radius, y, w - radius * 2, h, color);
    drawRect(x, y + radius, w, h - radius * 2, color);
    // Corner fills (approximation)
    drawRect(x, y, radius, radius, color);
    drawRect(x + w - radius, y, radius, radius, color);
    drawRect(x, y + h - radius, radius, radius, color);
    drawRect(x + w - radius, y + h - radius, radius, radius, color);
}

void UIManager::drawGradientRect(float x, float y, float w, float h, const glm::vec4& colorTop, const glm::vec4& colorBottom) {
    // Simulate gradient with multiple horizontal strips
    int strips = 8;
    float stripH = h / strips;
    for (int i = 0; i < strips; ++i) {
        float t = (float)i / (strips - 1);
        glm::vec4 c = glm::mix(colorTop, colorBottom, t);
        drawRect(x, y + i * stripH, w, stripH + 1, c);  // +1 to avoid gaps
    }
}

void UIManager::drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color) {
    float cursorX = x;
    
    // 5x7 pixel font style
    static const uint8_t font[][5] = {
        {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    };
    
    static const uint8_t nums[][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    };

    for (char c : text) {
        int idx = -1;
        bool isNum = false;
        
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a';
        else if (c >= '0' && c <= '9') { idx = c - '0'; isNum = true; }
        
        if (idx >= 0) {
            const uint8_t* glyph = isNum ? nums[idx] : font[idx];
            for (int col = 0; col < 5; col++) {
                uint8_t colData = glyph[col];
                for (int row = 0; row < 7; row++) {
                    if ((colData >> row) & 1) {
                        drawRect(cursorX + col*scale, y + row*scale, scale, scale, color);
                    }
                }
            }
        } else if (c == ':') {
             drawRect(cursorX + 2*scale, y + 1*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 3*scale, scale, scale, color);
        } else if (c == '.') {
             drawRect(cursorX + 2*scale, y + 4*scale, scale, scale, color);
        } else if (c == '-') {
             drawRect(cursorX + 1*scale, y + 3*scale, 3*scale, scale, color);
        } else if (c == '[') {
             drawRect(cursorX + 1*scale, y + 0*scale, scale, 7*scale, color);
             drawRect(cursorX + 2*scale, y + 0*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 6*scale, scale, scale, color);
        } else if (c == ']') {
             drawRect(cursorX + 3*scale, y + 0*scale, scale, 7*scale, color);
             drawRect(cursorX + 2*scale, y + 0*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 6*scale, scale, scale, color);
        }
        
        cursorX += 6 * scale;
    }
}

int UIManager::getBlockTextureIndex(BlockType type, int face) {
    // face: 0=top, 1=side, 2=bottom
    // Returns texture atlas index (0-255 for 16x16 atlas)
    // Use ResourcePackManager for actual texture lookup when available
    
    switch (type) {
        // Original blocks
        case BlockType::GRASS:
            if (face == 0) return 0;       // Top - grass top
            if (face == 2) return 2;       // Bottom - dirt
            return 3;                      // Side - grass side
        case BlockType::DIRT:
            return 2;                      // All faces - dirt
        case BlockType::STONE:
            return 1;                      // All faces - stone
        case BlockType::SAND:
            return 18;                     // All faces - sand
        case BlockType::GRAVEL:
            return 19;                     // All faces - gravel
        case BlockType::WOOD:
            return 4;                      // All faces - planks
        case BlockType::LEAVES:
            return 52;                     // All faces - leaves
        case BlockType::LOG:
            if (face == 0 || face == 2) return 21; // Top/Bottom - log top
            return 20;                     // Side - log side
        case BlockType::SNOW:
            return 66;                     // Snow texture
        case BlockType::SANDSTONE:
            if (face == 0 || face == 2) return 176; // sandstone top
            return 192;                    // sandstone side
        case BlockType::WATER:
            return 205;                    // Water texture
        case BlockType::ICE:
            return 67;                     // Ice texture
        case BlockType::BEDROCK:
            return 17;                     // Bedrock
        case BlockType::TALL_GRASS:
            return 39;                     // Tall grass
        case BlockType::ROSE:
            return 12;                     // Rose
            
        // Ores
        case BlockType::COBBLESTONE:
            return 16;                     // Cobblestone
        case BlockType::COAL_ORE:
            return 34;                     // Coal ore
        case BlockType::IRON_ORE:
            return 33;                     // Iron ore
        case BlockType::GOLD_ORE:
            return 32;                     // Gold ore
        case BlockType::DIAMOND_ORE:
            return 50;                     // Diamond ore
        case BlockType::EMERALD_ORE:
            return 171;                    // Emerald ore
        case BlockType::REDSTONE_ORE:
            return 51;                     // Redstone ore
        case BlockType::LAPIS_ORE:
            return 160;                    // Lapis ore
            
        // Stone variants
        case BlockType::MOSSY_COBBLESTONE:
            return 36;                     // Mossy cobblestone
        case BlockType::STONE_BRICKS:
            return 54;                     // Stone bricks
        case BlockType::MOSSY_STONE_BRICKS:
            return 100;                    // Mossy stone bricks
        case BlockType::CRACKED_STONE_BRICKS:
            return 118;                    // Cracked stone bricks
        case BlockType::CHISELED_STONE_BRICKS:
            return 98;                     // Chiseled stone bricks
            
        // Mineral blocks
        case BlockType::IRON_BLOCK:
            return 22;                     // Iron block
        case BlockType::GOLD_BLOCK:
            return 23;                     // Gold block
        case BlockType::DIAMOND_BLOCK:
            return 24;                     // Diamond block
        case BlockType::EMERALD_BLOCK:
            return 25;                     // Emerald block
        case BlockType::REDSTONE_BLOCK:
            return 215;                    // Redstone block
            
        // Building blocks
        case BlockType::BRICKS:
            return 7;                      // Bricks
        case BlockType::OBSIDIAN:
            return 37;                     // Obsidian
        case BlockType::GLASS:
            return 49;                     // Glass
        case BlockType::BOOKSHELF:
            if (face == 0 || face == 2) return 4; // Top/Bottom - planks
            return 35;                     // Side - bookshelf
        case BlockType::TNT:
            if (face == 0) return 9;       // Top
            if (face == 2) return 10;      // Bottom
            return 8;                      // Side
        case BlockType::GLOWSTONE:
            return 105;                    // Glowstone
        case BlockType::REDSTONE_LAMP:
            return 123;                    // Redstone lamp
            
        // Wood - Planks
        case BlockType::OAK_PLANKS:
            return 4;                      // Oak planks
        case BlockType::SPRUCE_PLANKS:
            return 198;                    // Spruce planks
        case BlockType::BIRCH_PLANKS:
            return 214;                    // Birch planks
        case BlockType::JUNGLE_PLANKS:
            return 199;                    // Jungle planks
            
        // Wood - Logs
        case BlockType::OAK_LOG:
            if (face == 0 || face == 2) return 21; // Top
            return 20;                     // Side
        case BlockType::SPRUCE_LOG:
            if (face == 0 || face == 2) return 117; // Spruce log top
            return 116;                    // Spruce log side
        case BlockType::BIRCH_LOG:
            if (face == 0 || face == 2) return 117; // Birch log top
            return 117;                    // Birch log side
        case BlockType::JUNGLE_LOG:
            if (face == 0 || face == 2) return 153; // Jungle log top
            return 153;                    // Jungle log side
            
        // Wood - Leaves
        case BlockType::OAK_LEAVES:
            return 52;                     // Oak leaves
        case BlockType::SPRUCE_LEAVES:
            return 132;                    // Spruce leaves
        case BlockType::BIRCH_LEAVES:
            return 52;                     // Use oak leaves
        case BlockType::JUNGLE_LEAVES:
            return 52;                     // Use oak leaves
            
        // Wool colors (using wool row starting at index 64)
        case BlockType::WHITE_WOOL:
            return 64;                     // White wool
        case BlockType::ORANGE_WOOL:
            return 210;                    // Orange wool
        case BlockType::MAGENTA_WOOL:
            return 194;                    // Magenta wool
        case BlockType::LIGHT_BLUE_WOOL:
            return 178;                    // Light blue wool
        case BlockType::YELLOW_WOOL:
            return 162;                    // Yellow wool
        case BlockType::LIME_WOOL:
            return 146;                    // Lime wool
        case BlockType::PINK_WOOL:
            return 130;                    // Pink wool
        case BlockType::GRAY_WOOL:
            return 114;                    // Gray wool
        case BlockType::LIGHT_GRAY_WOOL:
            return 225;                    // Light gray wool
        case BlockType::CYAN_WOOL:
            return 209;                    // Cyan wool
        case BlockType::PURPLE_WOOL:
            return 193;                    // Purple wool
        case BlockType::BLUE_WOOL:
            return 177;                    // Blue wool
        case BlockType::BROWN_WOOL:
            return 161;                    // Brown wool
        case BlockType::GREEN_WOOL:
            return 145;                    // Green wool
        case BlockType::RED_WOOL:
            return 129;                    // Red wool
        case BlockType::BLACK_WOOL:
            return 113;                    // Black wool
            
        case BlockType::CHISELED_SANDSTONE:
            if (face == 0 || face == 2) return 176;
            return 229;                    // Chiseled sandstone side
            
        // Misc blocks
        case BlockType::CLAY:
            return 53;                     // Clay
        case BlockType::SPONGE:
            return 48;                     // Sponge
        case BlockType::COBWEB:
            return 11;                     // Cobweb
        case BlockType::CRAFTING_TABLE:
            if (face == 0) return 43;      // Crafting table top
            if (face == 2) return 4;       // Bottom - planks
            return 59;                     // Side
        case BlockType::NOTE_BLOCK:
            return 74;                     // Note block
        case BlockType::JUKEBOX:
            if (face == 0) return 75;      // Jukebox top
            return 74;                     // Sides - note block
        case BlockType::FARMLAND:
            if (face == 0) return 86;      // Farmland top
            return 2;                      // Dirt sides
        case BlockType::SUGAR_CANE:
            return 73;                     // Sugar cane
        
        // Road blocks - use stone as atlas fallback (PBR has actual textures)
        case BlockType::ROAD_STRAIGHT:
        case BlockType::ROAD_LEFT:
        case BlockType::ROAD_RIGHT:
        case BlockType::ROAD_LEFT_RIGHT:
        case BlockType::ROAD_T_JUNCTION:
        case BlockType::ROAD_INTERSECTION_YELLOW:
        case BlockType::ROAD_MIDDLE_LINES:
        case BlockType::ROAD_MIDDLE_LINES_YELLOW:
        case BlockType::ROAD_MIDDLE_RIGHT:
        case BlockType::ROAD_MIDDLE_RIGHT_YELLOW:
        case BlockType::ROAD_LEFT_DIAG_45:
        case BlockType::ROAD_LEFT_DIAG_45_YELLOW:
        case BlockType::ROAD_LEFT_DIAG_60:
        case BlockType::ROAD_LEFT_DIAG_60_YELLOW:
        case BlockType::ROAD_RIGHT_DIAG_60:
        case BlockType::ROAD_RIGHT_DIAG_YELLOW:
        case BlockType::GLAZED_TERRACOTTA:
            return 1;                      // Stone placeholder for atlas
            
        default:
            return 1;                      // Default to stone
    }
}

void UIManager::drawBlockIcon(float x, float y, float size, BlockType type, float extraRotation) {
    if (type == BlockType::AIR || blockAtlasTexture == 0) return;
    
    blockIconShader.use();
    
    // Setup projection - orthographic with some depth
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -100.0f, 100.0f);
    blockIconShader.setMat4("uProjection", projection);
    
    // Setup model matrix - position, scale, and rotate for isometric view
    glm::mat4 model = glm::mat4(1.0f);
    // Move to center of the slot
    model = glm::translate(model, glm::vec3(x + size * 0.5f, y + size * 0.5f, 0.0f));
    // Scale to fit in slot
    model = glm::scale(model, glm::vec3(size * 0.6f, size * 0.6f, size * 0.6f));
    // Rotate for isometric view (like Minecraft inventory) - tilt FORWARD to show top
    model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Tilt forward (top visible)
    
    // User requested "rotate cube to left to face me on that edge"
    // Standard Minecraft view shows Front and Right faces.
    // +45 deg shows Front+Left.
    // -45 (315) deg shows Front+Right.
    // Add extraRotation for hover spin effect
    model = glm::rotate(model, glm::radians(315.0f + extraRotation), glm::vec3(0.0f, 1.0f, 0.0f));   // Rotate to show Front+Right + spin
    
    blockIconShader.setMat4("uModel", model);
    
    // Set atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    blockIconShader.setInt("uTexture", 0);
    
    // Set atlas cell size (16x16 atlas)
    float cellSize = 1.0f / 16.0f;
    blockIconShader.setFloat("uCellSize", cellSize);
    
    // Get texture indices for each face
    int topIdx = getBlockTextureIndex(type, 0);
    int sideIdx = getBlockTextureIndex(type, 1);
    int bottomIdx = getBlockTextureIndex(type, 2);
    
    // Calculate atlas offsets for each face
    auto idxToOffset = [cellSize](int idx) -> glm::vec2 {
        float col = float(idx % 16);
        float row = float(idx / 16);
        row = 15.0f - row; // Flip Y because texture is flipped
        return glm::vec2(col * cellSize, row * cellSize);
    };
    
    glm::vec2 offsets[3] = {
        idxToOffset(topIdx),    // top
        idxToOffset(sideIdx),   // side
        idxToOffset(bottomIdx)  // bottom
    };
    
    blockIconShader.setVec2("uAtlasOffset[0]", offsets[0]);
    blockIconShader.setVec2("uAtlasOffset[1]", offsets[1]);
    blockIconShader.setVec2("uAtlasOffset[2]", offsets[2]);
    
    // Enable depth test for proper face ordering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);  // Clear depth to avoid stale values
    
    // Disable culling - we want to see all faces of the block icon
    glDisable(GL_CULL_FACE);
    
    glBindVertexArray(blockIconVao);
    glDrawArrays(GL_TRIANGLES, 0, 36); // 6 faces * 6 vertices
    glBindVertexArray(0);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);  // Re-enable for other rendering
    
    // IMPORTANT: Reset texture binding to avoid affecting entity rendering
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Switch back to UI shader
    uiShader.use();
    glm::mat4 uiProjection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", uiProjection);
}

void UIManager::drawToolIcon(float x, float y, float size, ItemType type) {
    if (type == ItemType::NONE) return;
    
    // Draw a simplified tool icon using basic shapes
    // Color based on material
    glm::vec4 handleColor(0.5f, 0.35f, 0.2f, 1.0f);  // Wood handle
    glm::vec4 headColor;
    
    // ItemType enum is organized by TOOL TYPE first, then MATERIAL:
    // SWORD_WOOD=1, SWORD_STONE=2, SWORD_GOLD=3, SWORD_DIAMOND=4
    // PICKAXE_WOOD=5, PICKAXE_STONE=6, PICKAXE_GOLD=7, PICKAXE_DIAMOND=8
    // AXE_WOOD=9, AXE_STONE=10, AXE_GOLD=11, AXE_DIAMOND=12
    // SHOVEL_WOOD=13, SHOVEL_STONE=14, SHOVEL_GOLD=15, SHOVEL_DIAMOND=16
    
    int typeVal = static_cast<int>(type);
    int toolType = (typeVal - 1) / 4;   // 0=sword, 1=pickaxe, 2=axe, 3=shovel
    int material = (typeVal - 1) % 4;   // 0=wood, 1=stone, 2=gold, 3=diamond
    
    // Determine material color
    switch (material) {
        case 0: // Wood
            headColor = glm::vec4(0.6f, 0.45f, 0.25f, 1.0f);
            break;
        case 1: // Stone
            headColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            break;
        case 2: // Gold
            headColor = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
            break;
        case 3: // Diamond
            headColor = glm::vec4(0.3f, 0.9f, 0.9f, 1.0f);
            break;
        default:
            headColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            break;
    }
    
    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;
    float handleW = size * 0.12f;
    float handleH = size * 0.5f;
    
    if (toolType == 0) {
        // Sword - vertical blade with handle
        // Handle
        drawRect(cx - handleW * 0.5f, cy + size * 0.1f, handleW, handleH, handleColor);
        // Guard
        drawRect(cx - size * 0.15f, cy, size * 0.3f, handleW, headColor);
        // Blade
        drawRect(cx - handleW * 0.7f, cy - size * 0.35f, handleW * 1.4f, size * 0.4f, headColor);
        // Tip
        drawRect(cx - handleW * 0.5f, cy - size * 0.4f, handleW, size * 0.08f, headColor);
    } else if (toolType == 1) {
        // Pickaxe - diagonal with double head
        // Handle (diagonal)
        drawRect(cx - handleW * 0.5f, cy - size * 0.1f, handleW, handleH * 1.1f, handleColor);
        // Head (horizontal bar)
        drawRect(cx - size * 0.35f, cy - size * 0.25f, size * 0.7f, size * 0.15f, headColor);
        // Tips
        drawRect(cx - size * 0.38f, cy - size * 0.35f, size * 0.12f, size * 0.12f, headColor);
        drawRect(cx + size * 0.26f, cy - size * 0.35f, size * 0.12f, size * 0.12f, headColor);
    } else if (toolType == 2) {
        // Axe - handle with single head
        // Handle
        drawRect(cx - handleW * 0.5f, cy, handleW, handleH, handleColor);
        // Axe head
        drawRect(cx - size * 0.05f, cy - size * 0.35f, size * 0.3f, size * 0.35f, headColor);
        // Blade edge
        drawRect(cx + size * 0.18f, cy - size * 0.3f, size * 0.1f, size * 0.25f, headColor);
    } else {
        // Shovel - handle with flat head
        // Handle
        drawRect(cx - handleW * 0.5f, cy - size * 0.05f, handleW, handleH * 1.1f, handleColor);
        // Shovel head
        drawRect(cx - size * 0.15f, cy - size * 0.4f, size * 0.3f, size * 0.35f, headColor);
        // Round tip approximation
        drawRect(cx - size * 0.12f, cy - size * 0.45f, size * 0.24f, size * 0.08f, headColor);
    }
}

std::string UIManager::getBlockName(BlockType type) {
    switch (type) {
        case BlockType::AIR: return "Air";
        case BlockType::GRASS: return "Grass Block";
        case BlockType::DIRT: return "Dirt";
        case BlockType::STONE: return "Stone";
        case BlockType::SAND: return "Sand";
        case BlockType::WATER: return "Water";
        case BlockType::WOOD: return "Wood Planks";
        case BlockType::LEAVES: return "Leaves";
        case BlockType::SNOW: return "Snow Block";
        case BlockType::ICE: return "Ice";
        case BlockType::GRAVEL: return "Gravel";
        case BlockType::SANDSTONE: return "Sandstone";
        case BlockType::LOG: return "Oak Log";
        case BlockType::TALL_GRASS: return "Tall Grass";
        case BlockType::ROSE: return "Flower";
        case BlockType::BEDROCK: return "Bedrock";
        
        // Ores
        case BlockType::COBBLESTONE: return "Cobblestone";
        case BlockType::COAL_ORE: return "Coal Ore";
        case BlockType::IRON_ORE: return "Iron Ore";
        case BlockType::GOLD_ORE: return "Gold Ore";
        case BlockType::DIAMOND_ORE: return "Diamond Ore";
        case BlockType::EMERALD_ORE: return "Emerald Ore";
        case BlockType::REDSTONE_ORE: return "Redstone Ore";
        case BlockType::LAPIS_ORE: return "Lapis Lazuli Ore";
        
        // Stone variants
        case BlockType::MOSSY_COBBLESTONE: return "Mossy Cobblestone";
        case BlockType::STONE_BRICKS: return "Stone Bricks";
        case BlockType::MOSSY_STONE_BRICKS: return "Mossy Stone Bricks";
        case BlockType::CRACKED_STONE_BRICKS: return "Cracked Stone Bricks";
        case BlockType::CHISELED_STONE_BRICKS: return "Chiseled Stone Bricks";
        
        // Mineral blocks
        case BlockType::IRON_BLOCK: return "Block of Iron";
        case BlockType::GOLD_BLOCK: return "Block of Gold";
        case BlockType::DIAMOND_BLOCK: return "Block of Diamond";
        case BlockType::EMERALD_BLOCK: return "Block of Emerald";
        case BlockType::REDSTONE_BLOCK: return "Block of Redstone";
        
        // Building blocks
        case BlockType::BRICKS: return "Bricks";
        case BlockType::OBSIDIAN: return "Obsidian";
        case BlockType::GLASS: return "Glass";
        case BlockType::BOOKSHELF: return "Bookshelf";
        case BlockType::TNT: return "TNT";
        case BlockType::GLOWSTONE: return "Glowstone";
        case BlockType::REDSTONE_LAMP: return "Redstone Lamp";
        
        // Wood - Planks
        case BlockType::OAK_PLANKS: return "Oak Planks";
        case BlockType::SPRUCE_PLANKS: return "Spruce Planks";
        case BlockType::BIRCH_PLANKS: return "Birch Planks";
        case BlockType::JUNGLE_PLANKS: return "Jungle Planks";
        
        // Wood - Logs
        case BlockType::OAK_LOG: return "Oak Log";
        case BlockType::SPRUCE_LOG: return "Spruce Log";
        case BlockType::BIRCH_LOG: return "Birch Log";
        case BlockType::JUNGLE_LOG: return "Jungle Log";
        
        // Wood - Leaves
        case BlockType::OAK_LEAVES: return "Oak Leaves";
        case BlockType::SPRUCE_LEAVES: return "Spruce Leaves";
        case BlockType::BIRCH_LEAVES: return "Birch Leaves";
        case BlockType::JUNGLE_LEAVES: return "Jungle Leaves";
        
        // Wool
        case BlockType::WHITE_WOOL: return "White Wool";
        case BlockType::ORANGE_WOOL: return "Orange Wool";
        case BlockType::MAGENTA_WOOL: return "Magenta Wool";
        case BlockType::LIGHT_BLUE_WOOL: return "Light Blue Wool";
        case BlockType::YELLOW_WOOL: return "Yellow Wool";
        case BlockType::LIME_WOOL: return "Lime Wool";
        case BlockType::PINK_WOOL: return "Pink Wool";
        case BlockType::GRAY_WOOL: return "Gray Wool";
        case BlockType::LIGHT_GRAY_WOOL: return "Light Gray Wool";
        case BlockType::CYAN_WOOL: return "Cyan Wool";
        case BlockType::PURPLE_WOOL: return "Purple Wool";
        case BlockType::BLUE_WOOL: return "Blue Wool";
        case BlockType::BROWN_WOOL: return "Brown Wool";
        case BlockType::GREEN_WOOL: return "Green Wool";
        case BlockType::RED_WOOL: return "Red Wool";
        case BlockType::BLACK_WOOL: return "Black Wool";
        
        case BlockType::CHISELED_SANDSTONE: return "Chiseled Sandstone";
        
        // Misc blocks
        case BlockType::CLAY: return "Clay";
        case BlockType::SPONGE: return "Sponge";
        case BlockType::COBWEB: return "Cobweb";
        case BlockType::CRAFTING_TABLE: return "Crafting Table";
        case BlockType::NOTE_BLOCK: return "Note Block";
        case BlockType::JUKEBOX: return "Jukebox";
        case BlockType::FARMLAND: return "Farmland";
        case BlockType::SUGAR_CANE: return "Sugar Cane";
        
        // Road blocks
        case BlockType::GLAZED_TERRACOTTA: return "Glazed Terracotta";
        case BlockType::ROAD_STRAIGHT: return "Road Straight";
        case BlockType::ROAD_LEFT: return "Road Left";
        case BlockType::ROAD_RIGHT: return "Road Right";
        case BlockType::ROAD_LEFT_RIGHT: return "Road Left-Right";
        case BlockType::ROAD_T_JUNCTION: return "Road T-Junction";
        case BlockType::ROAD_INTERSECTION_YELLOW: return "Road Intersection";
        case BlockType::ROAD_MIDDLE_LINES: return "Road Middle Lines";
        case BlockType::ROAD_MIDDLE_LINES_YELLOW: return "Road Middle Lines Yellow";
        case BlockType::ROAD_MIDDLE_RIGHT: return "Road Middle Right";
        case BlockType::ROAD_MIDDLE_RIGHT_YELLOW: return "Road Middle Right Yellow";
        case BlockType::ROAD_LEFT_DIAG_45: return "Road Diag 45 Left";
        case BlockType::ROAD_LEFT_DIAG_45_YELLOW: return "Road Diag 45 Yellow";
        case BlockType::ROAD_LEFT_DIAG_60: return "Road Diag 60 Left";
        case BlockType::ROAD_LEFT_DIAG_60_YELLOW: return "Road Diag 60 Yellow";
        case BlockType::ROAD_RIGHT_DIAG_60: return "Road Diag 60 Right";
        case BlockType::ROAD_RIGHT_DIAG_YELLOW: return "Road Diag Right Yellow";
        
        default: return "Unknown";
    }
}

std::string UIManager::getItemName(ItemType type) {
    switch (type) {
        case ItemType::NONE: return "None";
        case ItemType::SWORD_WOOD: return "Wooden Sword";
        case ItemType::PICKAXE_WOOD: return "Wooden Pickaxe";
        case ItemType::AXE_WOOD: return "Wooden Axe";
        case ItemType::SHOVEL_WOOD: return "Wooden Shovel";
        case ItemType::SWORD_STONE: return "Stone Sword";
        case ItemType::PICKAXE_STONE: return "Stone Pickaxe";
        case ItemType::AXE_STONE: return "Stone Axe";
        case ItemType::SHOVEL_STONE: return "Stone Shovel";
        case ItemType::SWORD_GOLD: return "Golden Sword";
        case ItemType::PICKAXE_GOLD: return "Golden Pickaxe";
        case ItemType::AXE_GOLD: return "Golden Axe";
        case ItemType::SHOVEL_GOLD: return "Golden Shovel";
        case ItemType::SWORD_DIAMOND: return "Diamond Sword";
        case ItemType::PICKAXE_DIAMOND: return "Diamond Pickaxe";
        case ItemType::AXE_DIAMOND: return "Diamond Axe";
        case ItemType::SHOVEL_DIAMOND: return "Diamond Shovel";
        default: return "Unknown";
    }
}
