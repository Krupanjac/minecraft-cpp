#include "UIManager.h"
#include "Console.h"
#include "../World/WorldGenerator.h"
#include "../Core/Settings.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#endif

void UIManager::render() {
    // Menu and Debug rendering
    if (isMenuOpen() || showDebug) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        uiShader.use();
        glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
        uiShader.setMat4("uProjection", projection);

        if (isMenuOpen()) {
            // Draw semi-transparent background
            drawRect(0, 0, (float)width, (float)height, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
            
            // Draw game title for main menu
            if (currentMenuState == MenuState::MAIN_MENU) {
                // Title: C++craft with stylish colors
                std::string title = "Bettercraft";
                float titleScale = 6.0f;
                float titleW = title.length() * 6.0f * titleScale;
                float titleX = (width - titleW) / 2.0f;
                float titleY = height * 0.15f;
                
                // Draw shadow
                drawText(titleX + 4, titleY + 4, titleScale, title, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
                // Draw main title with gradient-like effect (green for C++, white for craft)
                drawText(titleX, titleY, titleScale, title, glm::vec4(0.4f, 0.9f, 0.4f, 1.0f));
                
                // Subtitle
                std::string subtitle = "A CPP Voxel Engine Game";
                float subScale = 2.0f;
                float subW = subtitle.length() * 6.0f * subScale;
                float subX = (width - subW) / 2.0f;
                drawText(subX, titleY + titleScale * 12.0f, subScale, subtitle, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

                // Main menu tip (persists for session)
                if (!mainMenuTip.empty()) {
                    float tipScale = 1.6f;
                    float tipW = mainMenuTip.length() * 6.0f * tipScale;
                    float tipX = (width - tipW) / 2.0f;
                    float tipY = titleY + titleScale * 16.5f;
                    drawText(tipX, tipY, tipScale, mainMenuTip, glm::vec4(0.85f, 0.85f, 0.85f, 1.0f));
                }
            }
            
            // Special handling for MAP - render biomes directly
            if (currentMenuState == MenuState::MAP && !elements.empty()) {
                const auto& mapEl = elements[0];
                
                // Draw the map background/border
                drawRect(mapEl.x - 4, mapEl.y - 4, mapEl.w + 8, mapEl.h + 8, glm::vec4(0.3f, 0.3f, 0.35f, 1.0f));
                drawRect(mapEl.x - 2, mapEl.y - 2, mapEl.w + 4, mapEl.h + 4, glm::vec4(0.1f, 0.1f, 0.12f, 1.0f));
                
                // Draw map directly - sample biomes at pixel resolution
                if (worldGenerator) {
                    int pixelRes = 96; // Lower res for better performance
                    float pixelSize = mapEl.w / pixelRes;
                    float worldPerPixel = mapScale * 4.0f; // Simpler calculation
                    float halfMapWorld = (pixelRes / 2.0f) * worldPerPixel;
                    
                    // Snap sampling center to worldPerPixel grid to prevent pixel shimmer during pan
                    float snappedCenterX = std::floor(mapCenterX / worldPerPixel) * worldPerPixel;
                    float snappedCenterZ = std::floor(mapCenterZ / worldPerPixel) * worldPerPixel;
                    float subPixelX = (mapCenterX - snappedCenterX) / worldPerPixel * pixelSize;
                    float subPixelZ = (mapCenterZ - snappedCenterZ) / worldPerPixel * pixelSize;
                    
                    // Enable scissor to clip pixels that overflow due to sub-pixel offset
                    glEnable(GL_SCISSOR_TEST);
                    glScissor((GLint)mapEl.x, (GLint)(height - mapEl.y - mapEl.h), (GLsizei)mapEl.w, (GLsizei)mapEl.h);
                    
                    // Render with 1 extra pixel border on each side to cover sub-pixel offset
                    for (int py = -1; py <= pixelRes; py++) {
                        for (int px = -1; px <= pixelRes; px++) {
                            // Convert pixel coords to world coords using snapped center
                            float worldX = snappedCenterX + (px - pixelRes / 2) * worldPerPixel;
                            float worldZ = snappedCenterZ + (py - pixelRes / 2) * worldPerPixel;
                            
                            // Get height and biome
                            float h = worldGenerator->getHeight(worldX, worldZ);
                            BiomeType biome = worldGenerator->getBiome(worldX, worldZ);
                            
                            // Determine color
                            glm::vec4 color;
                            
                            if (h < 32) { // SEA_LEVEL - water
                                float depth = (32 - h) / 32.0f;
                                color = glm::vec4(0.1f + 0.1f * (1.0f - depth), 
                                                  0.3f + 0.2f * (1.0f - depth), 
                                                  0.6f + 0.2f * (1.0f - depth), 1.0f);
                            } else {
                                BiomeInfo biomeInfo = worldGenerator->getBiomeInfo(biome);
                                float heightFactor = std::min(h / 100.0f, 1.0f);
                                
                                if (biome == BiomeType::CITY) {
                                    // City - gray stone color
                                    color = glm::vec4(0.55f + 0.1f * heightFactor, 0.55f + 0.1f * heightFactor, 0.6f + 0.08f * heightFactor, 1.0f);
                                } else if (biome == BiomeType::VILLAGE) {
                                    // Village - warm brown
                                    color = glm::vec4(0.65f + 0.1f * heightFactor, 0.52f + 0.08f * heightFactor, 0.35f, 1.0f);
                                } else if (biome == BiomeType::OCEAN) {
                                    color = glm::vec4(0.15f, 0.35f, 0.7f, 1.0f);
                                } else if (biome == BiomeType::MOUNTAINS && h > 115) {
                                    color = glm::vec4(0.92f, 0.94f, 0.96f, 1.0f); // Snow
                                } else {
                                    float r = biomeInfo.mapColorR * (0.75f + 0.25f * heightFactor);
                                    float g = biomeInfo.mapColorG * (0.75f + 0.25f * heightFactor);
                                    float b = biomeInfo.mapColorB;
                                    color = glm::vec4(r, g, b, 1.0f);
                                }
                            }
                            
                            // Apply sub-pixel offset for smooth, stable panning
                            float rx = mapEl.x + px * pixelSize - subPixelX;
                            float ry = mapEl.y + py * pixelSize - subPixelZ;
                            drawRect(rx, ry, pixelSize + 1, pixelSize + 1, color);
                        }
                    }
                    
                    glDisable(GL_SCISSOR_TEST);
                    
                    // Second pass: draw settlement icons at actual world-space centers
                    // Compute settlement centers from jittered grid to avoid icons moving during pan/zoom
                    {
                        const float CITY_GRID_M = 500.0f, CITY_JITTER_M = 120.0f;
                        const float VILLAGE_GRID_M = 180.0f, VILLAGE_JITTER_M = 50.0f;
                        unsigned int wSeed = worldGenerator->getSeed();
                        
                        auto jitterCenter = [&](float gridCX, float gridCZ, float jitterAmt, float gridSize) -> std::pair<float, float> {
                            unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
                            unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
                            unsigned int h1 = (wSeed ^ hx ^ hz) * 2654435761u;
                            unsigned int h2 = (wSeed ^ hz ^ (hx * 2246822519u)) * 3266489917u;
                            float offX = ((h1 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmt;
                            float offZ = ((h2 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmt;
                            return {gridCX + offX, gridCZ + offZ};
                        };
                        
                        // Visible world bounds
                        float visMinX = mapCenterX - halfMapWorld;
                        float visMaxX = mapCenterX + halfMapWorld;
                        float visMinZ = mapCenterZ - halfMapWorld;
                        float visMaxZ = mapCenterZ + halfMapWorld;
                        
                        // City icons - iterate grid cells in visible area
                        int cgMinX = (int)std::floor((visMinX - CITY_JITTER_M) / CITY_GRID_M);
                        int cgMaxX = (int)std::floor((visMaxX + CITY_JITTER_M) / CITY_GRID_M);
                        int cgMinZ = (int)std::floor((visMinZ - CITY_JITTER_M) / CITY_GRID_M);
                        int cgMaxZ = (int)std::floor((visMaxZ + CITY_JITTER_M) / CITY_GRID_M);
                        
                        for (int gx = cgMinX; gx <= cgMaxX; gx++) {
                            for (int gz = cgMinZ; gz <= cgMaxZ; gz++) {
                                float gridCX = gx * CITY_GRID_M + CITY_GRID_M / 2.0f;
                                float gridCZ = gz * CITY_GRID_M + CITY_GRID_M / 2.0f;
                                auto [cx, cz] = jitterCenter(gridCX, gridCZ, CITY_JITTER_M, CITY_GRID_M);
                                
                                if (worldGenerator->getBiome(cx, cz) != BiomeType::CITY) continue;
                                
                                float screenX = mapEl.x + ((cx - mapCenterX) / halfMapWorld + 1.0f) * 0.5f * mapEl.w;
                                float screenY = mapEl.y + ((cz - mapCenterZ) / halfMapWorld + 1.0f) * 0.5f * mapEl.h;
                                if (screenX < mapEl.x - 20 || screenX > mapEl.x + mapEl.w + 20 ||
                                    screenY < mapEl.y - 20 || screenY > mapEl.y + mapEl.h + 20) continue;
                                
                                float iconSize = std::max(12.0f, 30.0f / mapScale);
                                float ix = screenX - iconSize/2;
                                float iy = screenY - iconSize/2;
                                drawRect(ix-2, iy-2, iconSize+4, iconSize+4, glm::vec4(0.0f, 0.0f, 0.0f, 0.9f));
                                drawRect(ix, iy, iconSize, iconSize, glm::vec4(0.6f, 0.6f, 0.65f, 1.0f));
                                float textScale = std::max(0.6f, 1.2f / mapScale);
                                drawText(ix + iconSize/2 - 3*textScale, iy + iconSize/2 - 4*textScale, textScale, "C", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                            }
                        }
                        
                        // Village icons
                        int vgMinX = (int)std::floor((visMinX - VILLAGE_JITTER_M) / VILLAGE_GRID_M);
                        int vgMaxX = (int)std::floor((visMaxX + VILLAGE_JITTER_M) / VILLAGE_GRID_M);
                        int vgMinZ = (int)std::floor((visMinZ - VILLAGE_JITTER_M) / VILLAGE_GRID_M);
                        int vgMaxZ = (int)std::floor((visMaxZ + VILLAGE_JITTER_M) / VILLAGE_GRID_M);
                        
                        for (int gx = vgMinX; gx <= vgMaxX; gx++) {
                            for (int gz = vgMinZ; gz <= vgMaxZ; gz++) {
                                float gridCX = gx * VILLAGE_GRID_M + VILLAGE_GRID_M / 2.0f;
                                float gridCZ = gz * VILLAGE_GRID_M + VILLAGE_GRID_M / 2.0f;
                                auto [vx, vz] = jitterCenter(gridCX, gridCZ, VILLAGE_JITTER_M, VILLAGE_GRID_M);
                                
                                if (worldGenerator->getBiome(vx, vz) != BiomeType::VILLAGE) continue;
                                
                                float screenX = mapEl.x + ((vx - mapCenterX) / halfMapWorld + 1.0f) * 0.5f * mapEl.w;
                                float screenY = mapEl.y + ((vz - mapCenterZ) / halfMapWorld + 1.0f) * 0.5f * mapEl.h;
                                if (screenX < mapEl.x - 20 || screenX > mapEl.x + mapEl.w + 20 ||
                                    screenY < mapEl.y - 20 || screenY > mapEl.y + mapEl.h + 20) continue;
                                
                                float iconSize = std::max(8.0f, 22.0f / mapScale);
                                float ix = screenX - iconSize/2;
                                float iy = screenY - iconSize/2;
                                drawRect(ix-2, iy-2, iconSize+4, iconSize+4, glm::vec4(0.0f, 0.0f, 0.0f, 0.9f));
                                drawRect(ix, iy, iconSize, iconSize, glm::vec4(0.7f, 0.55f, 0.35f, 1.0f));
                                float textScale = std::max(0.6f, 1.2f / mapScale);
                                drawText(ix + iconSize/2 - 3*textScale, iy + iconSize/2 - 4*textScale, textScale, "V", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                            }
                        }
                    }
                    
                    // Draw player marker (uses actual center, mathematically equivalent)
                    float playerScreenX = mapEl.x + ((currentPlayerPos.x - snappedCenterX) / halfMapWorld + 1.0f) * 0.5f * mapEl.w - subPixelX;
                    float playerScreenZ = mapEl.y + ((currentPlayerPos.z - snappedCenterZ) / halfMapWorld + 1.0f) * 0.5f * mapEl.h - subPixelZ;
                    
                    // Check if player is on map
                    if (playerScreenX >= mapEl.x && playerScreenX <= mapEl.x + mapEl.w &&
                        playerScreenZ >= mapEl.y && playerScreenZ <= mapEl.y + mapEl.h) {
                        float ms = 16.0f;
                        // White outline
                        drawRect(playerScreenX - ms/2 - 3, playerScreenZ - ms/2 - 3, ms + 6, ms + 6, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
                        // Red fill
                        drawRect(playerScreenX - ms/2, playerScreenZ - ms/2, ms, ms, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                        // Inner highlight
                        drawRect(playerScreenX - ms/4, playerScreenZ - ms/4, ms/2, ms/2, glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
                        
                        // "YOU" label
                        drawRect(playerScreenX - 14, playerScreenZ - ms/2 - 18, 28, 14, glm::vec4(0.0f, 0.0f, 0.0f, 0.9f));
                        drawText(playerScreenX - 10, playerScreenZ - ms/2 - 16, 1.0f, "YOU", glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
                    }
                    
                    // Compass "N" at top
                    drawRect(mapEl.x + mapEl.w/2 - 10, mapEl.y - 24, 20, 18, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f));
                    drawText(mapEl.x + mapEl.w/2 - 5, mapEl.y - 22, 1.4f, "N", glm::vec4(1.0f, 0.95f, 0.95f, 1.0f));
                    
                    // Coordinates display
                    char coordBuf[128];
                    snprintf(coordBuf, sizeof(coordBuf), "X:%d Z:%d  Scale:%d", 
                            (int)mapCenterX, (int)mapCenterZ, (int)mapScale);
                    float coordLen = strlen(coordBuf) * 7.0f;
                    drawRect(mapEl.x, mapEl.y + mapEl.h + 6, coordLen + 10, 20, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f));
                    drawText(mapEl.x + 5, mapEl.y + mapEl.h + 10, 1.1f, coordBuf, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
                    
                    // Legend on right
                    float lx = mapEl.x + mapEl.w + 12;
                    float ly = mapEl.y;
                    
                    drawRect(lx - 4, ly - 4, 90, 130, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f));
                    
                    drawText(lx, ly, 1.0f, "LEGEND", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ly += 18;
                    
                    // City swatch - gray
                    drawRect(lx, ly, 12, 12, glm::vec4(0.6f, 0.6f, 0.65f, 1.0f));
                    drawText(lx + 16, ly + 1, 0.9f, "City", glm::vec4(0.7f, 0.7f, 0.75f, 1.0f));
                    ly += 16;
                    
                    // Village swatch - brown
                    drawRect(lx, ly, 12, 12, glm::vec4(0.7f, 0.55f, 0.35f, 1.0f));
                    drawText(lx + 16, ly + 1, 0.9f, "Village", glm::vec4(0.75f, 0.6f, 0.4f, 1.0f));
                    ly += 18;
                    
                    // Player swatch
                    drawRect(lx, ly, 12, 12, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                    drawText(lx + 16, ly + 1, 0.9f, "You", glm::vec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ly += 22;
                    
                    drawText(lx, ly, 0.8f, "LClick:TP", glm::vec4(0.5f, 0.8f, 0.5f, 1.0f));
                    ly += 12;
                    drawText(lx, ly, 0.8f, "RDrag:Pan", glm::vec4(0.5f, 0.8f, 0.5f, 1.0f));
                }
            }

            for (const auto& el : elements) {
                glm::vec4 color = el.isHovered ? glm::vec4(0.5f, 0.7f, 0.5f, 0.95f) : glm::vec4(0.25f, 0.25f, 0.28f, 0.9f);
                glm::vec4 borderColor = el.isHovered ? glm::vec4(0.6f, 0.9f, 0.6f, 1.0f) : glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
                
                // Skip the map element itself (we rendered it above)
                if (currentMenuState == MenuState::MAP && &el == &elements[0]) {
                    continue;
                }
                
                // Handle labels (just text, no background)
                if (el.isLabel) {
                    float textScale = 1.8f;
                    glm::vec4 textColor = el.customColor.a > 0 ? el.customColor : glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
                    // Left-align labels
                    drawText(el.x, el.y + (el.h - 7.0f * textScale) / 2.0f, textScale, el.text, textColor);
                    continue;
                }
                
                // Handle headers (larger text with underline)
                if (el.isHeader) {
                    float textScale = 2.2f;
                    glm::vec4 textColor = el.customColor.a > 0 ? el.customColor : glm::vec4(0.5f, 0.9f, 0.5f, 1.0f);
                    float textW = el.text.length() * 6.0f * textScale;
                    float textX = el.x + (el.w - textW) / 2.0f;
                    drawText(textX, el.y, textScale, el.text, textColor);
                    // Draw underline
                    drawRect(el.x, el.y + textScale * 10.0f, el.w, 2, glm::vec4(textColor.r, textColor.g, textColor.b, 0.5f));
                    continue;
                }
                
                // Handle card-style elements (larger, rounded-look buttons)
                if (el.isCard) {
                    glm::vec4 cardColor = el.isHovered ? glm::vec4(0.35f, 0.45f, 0.35f, 0.95f) : glm::vec4(0.18f, 0.2f, 0.22f, 0.9f);
                    glm::vec4 cardBorder = el.isHovered ? glm::vec4(0.5f, 0.8f, 0.5f, 1.0f) : glm::vec4(0.3f, 0.35f, 0.38f, 1.0f);
                    
                    // Draw card background with thicker border
                    float borderWidth = 3.0f;
                    drawRect(el.x - borderWidth, el.y - borderWidth, el.w + borderWidth*2, el.h + borderWidth*2, cardBorder);
                    drawRect(el.x, el.y, el.w, el.h, cardColor);
                    
                    // Draw thumbnail if available
                    float textOffsetX = 0.0f;
                    if (el.textureId != 0) {
                        // Draw thumbnail border
                        float thumbBorder = 2.0f;
                        drawRect(el.thumbnailX - thumbBorder, el.thumbnailY - thumbBorder, 
                                 el.thumbnailW + thumbBorder*2, el.thumbnailH + thumbBorder*2, 
                                 glm::vec4(0.1f, 0.1f, 0.12f, 1.0f));
                        
                        // Draw the thumbnail image
                        drawTexturedRect(el.thumbnailX, el.thumbnailY, el.thumbnailW, el.thumbnailH, el.textureId);
                        
                        // Offset text to the right of thumbnail
                        textOffsetX = el.thumbnailW + 24.0f;
                    }
                    
                    // Draw text (left-aligned if thumbnail, centered otherwise)
                    float textScale = 2.5f;
                    float textW = el.text.length() * 6.0f * textScale;
                    float textX, textY;
                    
                    if (el.textureId != 0) {
                        // Left-align after thumbnail
                        textX = el.x + textOffsetX + 12.0f;
                        textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                    } else {
                        // Center if no thumbnail
                        textX = el.x + (el.w - textW) / 2.0f;
                        textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                    }
                    drawText(textX, textY, textScale, el.text, glm::vec4(1.0f));
                    continue;
                }
                
                if (el.isInventoryItem) {
                    // Draw slot background with border effect
                    glm::vec4 bgColor = el.isHovered ? glm::vec4(0.4f, 0.4f, 0.5f, 0.95f) : glm::vec4(0.15f, 0.15f, 0.15f, 0.9f);
                    glm::vec4 innerColor = el.isHovered ? glm::vec4(0.3f, 0.3f, 0.35f, 0.95f) : glm::vec4(0.22f, 0.22f, 0.22f, 0.9f);
                    
                    // Outer border
                    drawRect(el.x - 2, el.y - 2, el.w + 4, el.h + 4, glm::vec4(0.1f, 0.1f, 0.1f, 0.9f));
                    drawRect(el.x, el.y, el.w, el.h, bgColor);
                    drawRect(el.x + 2, el.y + 2, el.w - 4, el.h - 4, innerColor);
                    
                    // Get rotation for this inventory item
                    float rotation = 0.0f;
                    if (el.inventoryIndex >= 0 && el.inventoryIndex < 256) {
                        rotation = inventoryItemRotations[el.inventoryIndex];
                    }
                    
                    // Draw 3D isometric block or tool
                    if (el.isInventoryTool && el.itemType != ItemType::NONE) {
                        drawToolIcon(el.x + 4, el.y + 4, el.w - 8, el.itemType);
                    } else if (el.blockType != BlockType::AIR) {
                        drawBlockIcon(el.x + 4, el.y + 4, el.w - 8, el.blockType, rotation);
                    }
                    
                    // Draw selection highlight if this block is in current hotbar slot
                    if (!el.isInventoryTool && el.blockType == hotbar[selectedSlot]) {
                        glm::vec4 hl(0.9f, 0.8f, 0.3f, 1.0f); // Golden highlight
                        float t = 3.0f;
                        drawRect(el.x - 2, el.y - 2, el.w + 4, t, hl); // Top
                        drawRect(el.x - 2, el.y + el.h - t + 2, el.w + 4, t, hl); // Bottom
                        drawRect(el.x - 2, el.y - 2, t, el.h + 4, hl); // Left
                        drawRect(el.x + el.w - t + 2, el.y - 2, t, el.h + 4, hl); // Right
                    }
                    continue;
                }

                // Draw button with border for better look
                float borderWidth = 2.0f;
                drawRect(el.x - borderWidth, el.y - borderWidth, el.w + borderWidth*2, el.h + borderWidth*2, borderColor);
                drawRect(el.x, el.y, el.w, el.h, color);
                
                // Draw slider indicator
                if (el.isSlider) {
                    float val = 0.0f;
                    if (el.intValueRef) val = (float)*el.intValueRef;
                    else if (el.valueRef) val = *el.valueRef;
                    
                    float pct = (val - el.minVal) / (el.maxVal - el.minVal);
                    drawRect(el.x, el.y, el.w * pct, el.h, glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
                }

                // Draw text centered
                float textScale = 2.0f;
                float textW = el.text.length() * 6.0f * textScale; // Approx width
                float textX = el.x + (el.w - textW) / 2.0f;
                float textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                drawText(textX, textY, textScale, el.text, glm::vec4(1.0f));
            }
            
            // Render 3D model preview for Player Settings - AFTER all UI elements
            if (currentMenuState == MenuState::PLAYER_SETTINGS && previewModel) {
                // Use full screen for rendering - no clipping!
                // Render the 3D model directly to screen
                uiShader.unuse();
                
                // Use full screen viewport - no scissor clipping
                glViewport(0, 0, width, height);
                glDisable(GL_SCISSOR_TEST);  // Don't clip!
                
                // Clear just depth
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glClear(GL_DEPTH_BUFFER_BIT);
                glDisable(GL_BLEND);
                
                // Setup camera for 3D preview
                float aspectRatio = (float)width / (float)height;
                glm::mat4 proj3D = glm::perspective(glm::radians(30.0f), aspectRatio, 0.1f, 100.0f);
                
                // Camera looking at model - pull back so models fit nicely
                // Half-Life (index 0) needs more distance
                float camDist = (previewModelIndex == 0) ? 20.0f : 6.0f;
                float camHeight = (previewModelIndex == 0) ? 4.0f : 1.5f;
                float targetHeight = (previewModelIndex == 0) ? 2.5f : 0.7f;
                
                glm::vec3 camPos = glm::vec3(0.0f, camHeight, camDist);
                glm::vec3 target = glm::vec3(0.0f, targetHeight, 0.0f);
                glm::mat4 view3D = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
                
                // Model transform - rotate based on user drag
                glm::mat4 modelMat = glm::mat4(1.0f);
                modelMat = glm::rotate(modelMat, glm::radians(previewRotation), glm::vec3(0.0f, 1.0f, 0.0f));
                
                // Scale down all models - Half-Life much smaller, others also scaled
                float modelScale = (previewModelIndex == 0) ? 0.1f : 0.6f;
                modelMat = glm::scale(modelMat, glm::vec3(modelScale));
                
                // Use the model shader
                modelPreviewShader.use();
                modelPreviewShader.setMat4("uProjection", proj3D);
                modelPreviewShader.setMat4("uView", view3D);
                modelPreviewShader.setMat4("uPrevView", view3D);
                modelPreviewShader.setMat4("uPrevProjection", proj3D);
                modelPreviewShader.setVec3("uLightDir", glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)));
                modelPreviewShader.setVec3("uCameraPos", camPos);
                modelPreviewShader.setVec4("uBaseColor", glm::vec4(1.0f));
                modelPreviewShader.setInt("uDebugNoTexture", 0);
                modelPreviewShader.setInt("uDebugShowNormals", 0);
                modelPreviewShader.setInt("uUseShadows", 0);  // No shadows in preview
                modelPreviewShader.setFloat("uAlphaMultiplier", 1.0f);
                modelPreviewShader.setMat4("uLightSpaceMatrix", glm::mat4(1.0f));  // Identity for preview
                
                // Draw the model
                previewModel->draw(modelPreviewShader, modelMat, modelMat);
                
                modelPreviewShader.unuse();
                
                // Restore viewport to full screen
                glViewport(0, 0, width, height);
                glDisable(GL_SCISSOR_TEST);
                
                // Back to UI shader state
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                uiShader.use();
                uiShader.setMat4("uProjection", glm::ortho(0.0f, (float)width, (float)height, 0.0f));
            }
        }

        if (showDebug) {
            std::string fpsText = "FPS: " + std::to_string((int)currentFPS);
            std::string blockText = "Block: " + currentBlockName;
            
            drawText(10.0f, 30.0f, 2.0f, fpsText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            drawText(10.0f, 60.0f, 2.0f, blockText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            
            
            std::string posText = "XYZ: " + std::to_string((int)currentPlayerPos.x) + " " + 
                                  std::to_string((int)currentPlayerPos.y) + " " + 
                                  std::to_string((int)currentPlayerPos.z);
            drawText(10.0f, 90.0f, 2.0f, posText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            float speed = glm::length(currentPlayerVel);
            float hSpeed = glm::length(glm::vec2(currentPlayerVel.x, currentPlayerVel.z));
            std::string velText = "SPEED: " + std::to_string(speed).substr(0,4) + " (H: " + std::to_string(hSpeed).substr(0,4) + ")";
            drawText(10.0f, 120.0f, 2.0f, velText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            std::string timeText = "TIME: " + std::to_string(static_cast<int>(timeOfDay));
            drawText(10.0f, 150.0f, 2.0f, timeText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            
            // Debug Controls
            drawText(10.0f, 180.0f, 2.0f, "[F1] TOGGLE DEBUG", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 210.0f, 2.0f, "[F2] PAUSE TIME: " + std::string(isDayNightPaused ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 240.0f, 2.0f, "[F3] SHADOWS: " + std::string(Settings::instance().enableShadows ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 270.0f, 2.0f, "[ARROWS] CHANGE TIME", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

            // New debug toggles
            drawText(10.0f, 300.0f, 2.0f, "[F4] SHOW TAA METRICS: " + std::string(Settings::instance().debugShowTAA ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 330.0f, 2.0f, "[F8] NO TEXTURES: " + std::string(Settings::instance().debugNoTexture ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 360.0f, 2.0f, "[F6] WIREFRAME: " + std::string(Settings::instance().debugWireframe ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 390.0f, 2.0f, "[F7] SHOW NORMALS: " + std::string(Settings::instance().debugShowNormals ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

            // Show TAA metrics if enabled
            if (Settings::instance().debugShowTAA) {
                std::string taaMotion = "TAA Motion: " + std::to_string(lastTaaMotion).substr(0,6);
                std::string taaHistory = "TAA HistWeight: " + std::to_string(lastTaaHistoryWeight).substr(0,6);
                drawText(10.0f, 420.0f, 2.0f, taaMotion, glm::vec4(0.9f, 0.6f, 0.2f, 1.0f));
                drawText(10.0f, 450.0f, 2.0f, taaHistory, glm::vec4(0.9f, 0.6f, 0.2f, 1.0f));
            }
        }
        
        // Render tooltip if visible and timer exceeded delay
        if (!currentTooltip.empty() && tooltipTimer >= tooltipDelay && Settings::instance().enableTooltips) {
            float padding = 8.0f;
            float textScale = 1.5f;
            float tooltipW = currentTooltip.length() * 6.0f * textScale + padding * 2;
            float tooltipH = 7.0f * textScale + padding * 2;
            
            // Keep tooltip on screen
            float drawX = tooltipX;
            float drawY = tooltipY;
            if (drawX + tooltipW > width) drawX = width - tooltipW - 5;
            if (drawY + tooltipH > height) drawY = height - tooltipH - 5;
            
            // Draw tooltip background
            drawRect(drawX, drawY, tooltipW, tooltipH, glm::vec4(0.1f, 0.1f, 0.12f, 0.95f));
            // Draw border
            drawRect(drawX, drawY, tooltipW, 1, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX, drawY + tooltipH - 1, tooltipW, 1, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX, drawY, 1, tooltipH, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX + tooltipW - 1, drawY, 1, tooltipH, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            // Draw text
            drawText(drawX + padding, drawY + padding, textScale, currentTooltip, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
        }

        uiShader.unuse();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    
    // Always render HUD unless in Main Menu / Settings / Load Game (basically if we are 'in game' or 'inventory')
    // Specifically: NONE (playing) or INVENTORY or CHAT (chat overlay). Not IN_GAME_MENU (pause).
    if (currentMenuState == MenuState::NONE || currentMenuState == MenuState::INVENTORY || currentMenuState == MenuState::CHAT) {
        renderHUD();
        renderChat();
    }
}

void UIManager::renderHUD() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    // ========================================
    // HOTBAR - Modern Glass-like Design
    // ========================================
    float slotSize = 48.0f;
    float slotPadding = 4.0f;
    float slotGap = 3.0f;
    int slots = 9;
    float innerSize = slotSize - slotPadding * 2;
    float totalW = slots * slotSize + (slots - 1) * slotGap;
    float hotbarPadding = 6.0f;
    float hotbarW = totalW + hotbarPadding * 2;
    float hotbarH = slotSize + hotbarPadding * 2;
    float startX = (width - hotbarW) / 2.0f;
    float startY = height - hotbarH - 8.0f;
    
    // Hotbar background - dark glass effect
    drawRect(startX, startY, hotbarW, hotbarH, glm::vec4(0.05f, 0.05f, 0.08f, 0.85f));
    // Top highlight
    drawRect(startX, startY, hotbarW, 1.0f, glm::vec4(0.3f, 0.3f, 0.35f, 0.6f));
    // Bottom shadow
    drawRect(startX, startY + hotbarH - 1, hotbarW, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    // Left/right borders
    drawRect(startX, startY, 1.0f, hotbarH, glm::vec4(0.2f, 0.2f, 0.25f, 0.5f));
    drawRect(startX + hotbarW - 1, startY, 1.0f, hotbarH, glm::vec4(0.2f, 0.2f, 0.25f, 0.5f));
    
    float slotStartX = startX + hotbarPadding;
    float slotStartY = startY + hotbarPadding;
    
    for (int i = 0; i < slots; ++i) {
        float x = slotStartX + i * (slotSize + slotGap);
        float y = slotStartY;
        
        // Apply hover animation (slots bob up slightly when hovered)
        float hoverOffset = hotbarSlotHover[i] * 6.0f;
        float bounceOffset = hotbarSlotBounce[i] * 4.0f;
        y -= hoverOffset + bounceOffset;
        
        // Selection highlight
        if (i == selectedSlot) {
            // Outer glow
            float glowSize = 4.0f + selectionBounce * 2.0f;
            drawRect(x - glowSize, y - glowSize, slotSize + glowSize * 2, slotSize + glowSize * 2, 
                     glm::vec4(1.0f, 0.85f, 0.3f, 0.3f + selectionBounce * 0.2f));
            // Golden border
            drawRect(x - 2, y - 2, slotSize + 4, slotSize + 4, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
            drawRect(x - 1, y - 1, slotSize + 2, slotSize + 2, glm::vec4(0.9f, 0.75f, 0.2f, 1.0f));
        }
        
        // Slot background with subtle gradient effect
        glm::vec4 slotBg = (i == selectedSlot) ? 
            glm::vec4(0.2f, 0.2f, 0.22f, 0.95f) : 
            glm::vec4(0.12f, 0.12f, 0.14f, 0.9f);
        
        // Brighten on hover
        if (hotbarSlotHover[i] > 0.0f) {
            float h = hotbarSlotHover[i];
            slotBg = glm::vec4(slotBg.r + h * 0.1f, slotBg.g + h * 0.1f, slotBg.b + h * 0.12f, slotBg.a);
        }
        
        drawRect(x, y, slotSize, slotSize, slotBg);
        
        // Inner border (3D inset effect)
        drawRect(x, y, slotSize, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));  // top shadow
        drawRect(x, y, 1.0f, slotSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));  // left shadow
        drawRect(x, y + slotSize - 1, slotSize, 1.0f, glm::vec4(0.3f, 0.3f, 0.35f, 0.3f));  // bottom highlight
        drawRect(x + slotSize - 1, y, 1.0f, slotSize, glm::vec4(0.3f, 0.3f, 0.35f, 0.3f));  // right highlight
        
        // Draw item/block from hotbarSlots (supports both blocks and items)
        const HotbarSlot& slot = hotbarSlots[i];
        float iconX = x + slotPadding;
        float iconY = y + slotPadding;
        
        if (!slot.isEmpty()) {
            if (slot.isItem) {
                // Draw tool icon
                drawToolIcon(iconX, iconY, innerSize, slot.itemStack.type);
            } else if (slot.blockType != BlockType::AIR) {
                // Draw block with rotation on hover for spinning effect
                drawBlockIcon(iconX, iconY, innerSize, slot.blockType, hotbarSlotRotation[i]);
            }
        }
        
        // Slot number (subtle, bottom-right corner)
        float numX = x + slotSize - 12;
        float numY = y + slotSize - 14;
        glm::vec4 numColor = (i == selectedSlot) ? 
            glm::vec4(1.0f, 0.9f, 0.6f, 0.9f) : 
            glm::vec4(0.6f, 0.6f, 0.65f, 0.7f);
        drawText(numX, numY, 0.4f, std::to_string(i + 1), numColor);
    }
    
    // ========================================
    // STATS BARS - Above Hotbar
    // ========================================
    float statsY = startY - 32.0f;
    float barHeight = 10.0f;
    float barWidth = (totalW - 20.0f) / 2.0f;
    float iconSize = 14.0f;
    
    // Health Bar (left side)
    float healthX = startX + hotbarPadding;
    
    // Health icon (heart shape approximation)
    glm::vec4 heartColor = glm::vec4(0.9f, 0.15f, 0.2f, 1.0f);
    if (healthFlash > 0.0f) {
        heartColor = glm::mix(heartColor, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), healthFlash);
    }
    // Simple heart icon made of squares
    drawRect(healthX + 2, statsY + 2, iconSize - 4, iconSize - 4, heartColor);
    drawRect(healthX, statsY + 4, iconSize, iconSize - 6, heartColor);
    drawRect(healthX + 4, statsY, iconSize - 8, iconSize - 2, heartColor);
    
    // Health bar background
    float hBarX = healthX + iconSize + 6;
    drawRect(hBarX, statsY + 1, barWidth, barHeight, glm::vec4(0.15f, 0.05f, 0.05f, 0.9f));
    // Health bar border
    drawRect(hBarX, statsY + 1, barWidth, 1.0f, glm::vec4(0.4f, 0.1f, 0.1f, 1.0f));
    drawRect(hBarX, statsY + barHeight, barWidth, 1.0f, glm::vec4(0.2f, 0.05f, 0.05f, 1.0f));
    
    // Health fill with gradient
    float healthPct = playerHealth / 20.0f;
    float healthFillW = (barWidth - 4) * healthPct;
    if (healthFillW > 0) {
        glm::vec4 healthColorMain = glm::vec4(0.85f, 0.15f, 0.2f, 1.0f);
        glm::vec4 healthColorBright = glm::vec4(1.0f, 0.3f, 0.35f, 1.0f);
        if (healthFlash > 0.0f) {
            healthColorMain = glm::mix(healthColorMain, glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), healthFlash);
        }
        drawRect(hBarX + 2, statsY + 3, healthFillW, barHeight - 4, healthColorMain);
        // Top shine
        drawRect(hBarX + 2, statsY + 3, healthFillW, 2.0f, healthColorBright);
    }
    
    // Health text
    std::string healthText = std::to_string(playerHealth) + "/20";
    drawText(hBarX + barWidth / 2 - healthText.length() * 3, statsY + 2, 0.4f, healthText, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    
    // Hunger/Food Bar (right side)
    float foodX = startX + hotbarW - hotbarPadding - barWidth - iconSize - 6;
    
    // Food icon (drumstick shape approximation)
    glm::vec4 foodColor = glm::vec4(0.7f, 0.45f, 0.2f, 1.0f);
    drawRect(foodX + 2, statsY + 3, iconSize - 4, iconSize - 6, foodColor);
    drawRect(foodX + iconSize - 6, statsY + 5, 4, iconSize - 8, glm::vec4(0.9f, 0.85f, 0.7f, 1.0f)); // bone
    
    // Food bar
    float fBarX = foodX + iconSize + 6;
    drawRect(fBarX, statsY + 1, barWidth, barHeight, glm::vec4(0.1f, 0.08f, 0.03f, 0.9f));
    drawRect(fBarX, statsY + 1, barWidth, 1.0f, glm::vec4(0.3f, 0.2f, 0.1f, 1.0f));
    drawRect(fBarX, statsY + barHeight, barWidth, 1.0f, glm::vec4(0.15f, 0.1f, 0.05f, 1.0f));
    
    // Food fill
    float foodPct = playerFood / 20.0f;
    float foodFillW = (barWidth - 4) * foodPct;
    if (foodFillW > 0) {
        drawRect(fBarX + 2, statsY + 3, foodFillW, barHeight - 4, glm::vec4(0.65f, 0.4f, 0.15f, 1.0f));
        drawRect(fBarX + 2, statsY + 3, foodFillW, 2.0f, glm::vec4(0.8f, 0.55f, 0.25f, 1.0f));
    }
    
    // Food text
    std::string foodText = std::to_string(playerFood) + "/20";
    drawText(fBarX + barWidth / 2 - foodText.length() * 3, statsY + 2, 0.4f, foodText, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    
    // ========================================
    // XP BAR - Below stats, above hotbar
    // ========================================
    float xpY = startY - 14.0f;
    float xpH = 6.0f;
    float xpW = totalW;
    float xpX = startX + hotbarPadding;
    
    // XP bar background
    drawRect(xpX, xpY, xpW, xpH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    drawRect(xpX, xpY, xpW, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    
    // XP fill with glow effect
    float xpFillW = xpW * playerXP;
    if (xpFillW > 0) {
        glm::vec4 xpColor = glm::vec4(0.3f, 0.9f, 0.3f, 1.0f);
        if (xpBarGlow > 0.0f) {
            xpColor = glm::mix(xpColor, glm::vec4(0.6f, 1.0f, 0.6f, 1.0f), xpBarGlow);
        }
        drawRect(xpX + 1, xpY + 1, xpFillW - 2, xpH - 2, xpColor);
        // Shine
        drawRect(xpX + 1, xpY + 1, xpFillW - 2, 2.0f, glm::vec4(0.5f, 1.0f, 0.5f, 0.8f));
    }
    
    // Level indicator (centered above XP bar)
    if (playerLevel > 0) {
        std::string levelText = std::to_string(playerLevel);
        float levelX = width / 2.0f - levelText.length() * 4;
        float levelY = xpY - 14.0f;
        // Shadow
        drawText(levelX + 1, levelY + 1, 0.5f, levelText, glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
        // Text with glow
        glm::vec4 levelColor = glm::vec4(0.5f, 1.0f, 0.5f, 1.0f);
        if (xpBarGlow > 0.0f) {
            levelColor = glm::mix(levelColor, glm::vec4(1.0f, 1.0f, 0.5f, 1.0f), xpBarGlow);
        }
        drawText(levelX, levelY, 0.5f, levelText, levelColor);
    }
    
    // ========================================
    // GAME MODE INDICATOR
    // ========================================
    if (isCreativeMode) {
        std::string modeText = "Creative Mode";
        float modeTextW = modeText.length() * 7.0f;
        float modeX = 12.0f;
        float modeY = 12.0f;
        
        // Background pill
        drawRect(modeX, modeY, modeTextW + 16.0f, 24.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
        drawRect(modeX, modeY, modeTextW + 16.0f, 1.0f, glm::vec4(0.3f, 0.7f, 1.0f, 0.5f));
        
        // Icon (star-like)
        drawRect(modeX + 6, modeY + 8, 8, 8, glm::vec4(0.3f, 0.8f, 1.0f, 1.0f));
        
        // Text
        drawText(modeX + 18, modeY + 6, 0.45f, modeText, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
    }
    
    uiShader.unuse();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void UIManager::renderChat() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    float chatX = 10.0f;
    float chatY = height - 150.0f; // Above hotbar
    float lineHeight = 20.0f;
    float textScale = 1.5f;
    
    // Use settings for fade delay
    float fadeDelay = Settings::instance().chatFadeDelay;
    
    // Update timestamps and render recent messages
    int visibleCount = 0;
    int maxVisible = 10;
    
    // Render messages from bottom to top (newest at bottom)
    for (int i = static_cast<int>(chatMessages.size()) - 1; i >= 0 && visibleCount < maxVisible; i--) {
        auto& msg = chatMessages[i];
        msg.timestamp += 0.016f; // Approximate frame time
        
        // Only show recent messages when chat is closed, show all when open
        bool shouldShow = (currentMenuState == MenuState::CHAT) || (msg.timestamp < fadeDelay);
        
        if (shouldShow) {
            float alpha = 1.0f;
            if (currentMenuState != MenuState::CHAT && msg.timestamp > fadeDelay - 2.0f) {
                alpha = (fadeDelay - msg.timestamp) / 2.0f;
            }
            
            float y = chatY - (visibleCount * lineHeight);
            
            // Draw background for readability
            std::string fullMsg = "<" + msg.playerName + "> " + msg.message;
            float msgWidth = fullMsg.length() * 6.0f * textScale + 10.0f;
            drawRect(chatX - 5, y - 2, msgWidth, lineHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f * alpha));
            
            // Draw player name in yellow, message in white
            std::string nameStr = "<" + msg.playerName + "> ";
            drawText(chatX, y, textScale, nameStr, glm::vec4(1.0f, 1.0f, 0.3f, alpha));
            
            float nameWidth = nameStr.length() * 6.0f * textScale;
            drawText(chatX + nameWidth, y, textScale, msg.message, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
            
            visibleCount++;
        }
    }
    
    // Render chat input when chat is open
    if (currentMenuState == MenuState::CHAT) {
        float inputY = chatY + lineHeight + 5.0f;
        float inputW = width * 0.4f;
        float inputH = 25.0f;
        
        // Input background
        drawRect(chatX - 5, inputY, inputW, inputH, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
        
        // Input text with cursor
        std::string displayText = chatInput + "_";
        drawText(chatX, inputY + 4, textScale, displayText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        
        // Help text
        drawText(chatX, inputY + inputH + 5, 1.2f, "Press ENTER to send, ESC to cancel", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    }
    
    uiShader.unuse();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void UIManager::renderModelPreview() {
    if (!previewModel) return;
    if (previewFBO == 0 || previewTexture == 0) return;
    
    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Bind preview FBO
    glBindFramebuffer(GL_FRAMEBUFFER, previewFBO);
    
    // Set draw buffers
    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);
    
    glViewport(0, 0, previewWidth, previewHeight);
    
    // Clear with semi-transparent dark background
    glClearColor(0.1f, 0.1f, 0.15f, 0.9f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    // Setup camera matrices for preview
    float aspectRatio = (float)previewWidth / (float)previewHeight;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    
    // Camera position - looking at model from front, positioned to see full character
    // Character models are typically 1.8 units tall, centered at origin
    glm::vec3 cameraPos = glm::vec3(0.0f, 1.2f, 2.5f);
    glm::vec3 target = glm::vec3(0.0f, 0.9f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Model transform with rotation - rotate around Y axis and face camera
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(previewRotation + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(1.0f));
    
    // Setup shader
    modelPreviewShader.use();
    modelPreviewShader.setMat4("uProjection", projection);
    modelPreviewShader.setMat4("uView", view);
    modelPreviewShader.setMat4("uPrevView", view);
    modelPreviewShader.setMat4("uPrevProjection", projection);
    
    // Simple lighting from above-front
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.5f));
    modelPreviewShader.setVec3("uLightDir", lightDir);
    modelPreviewShader.setVec3("uCameraPos", cameraPos);
    modelPreviewShader.setVec4("uBaseColor", glm::vec4(1.0f));
    modelPreviewShader.setInt("uDebugNoTexture", 0);
    modelPreviewShader.setInt("uDebugShowNormals", 0);
    
    // Draw the model
    previewModel->draw(modelPreviewShader, modelMatrix, modelMatrix);
    
    modelPreviewShader.unuse();
    
    // Unbind FBO and restore viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void UIManager::renderLoadingTip(const std::string& text) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);

    // Draw "Loading..." text above the progress bar
    float loadingScale = 2.0f;
    std::string loadingText = "Loading...";
    float loadingW = loadingText.length() * 6.0f * loadingScale;
    float loadingX = (width - loadingW) / 2.0f;
    float loadingY = height * 0.40f;
    drawText(loadingX, loadingY, loadingScale, loadingText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Draw tip text below the progress bar
    if (!text.empty()) {
        float tipScale = 1.4f;
        float textW = text.length() * 6.0f * tipScale;
        float x = (width - textW) / 2.0f;
        float y = height * 0.65f;
        drawText(x, y, tipScale, text, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    }

    uiShader.unuse();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UIManager::renderConsole() {
    auto& console = Console::instance();
    if (!console.isVisible()) return;
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    // Console dimensions
    float consoleHeight = height * 0.4f;  // 40% of screen height
    float padding = 8.0f;
    float lineHeight = 16.0f;
    float fontScale = 1.5f;
    int visibleLines = static_cast<int>((consoleHeight - padding * 3 - lineHeight) / lineHeight);
    
    // Background
    drawRect(0, 0, (float)width, consoleHeight, glm::vec4(0.05f, 0.05f, 0.1f, 0.92f));
    
    // Border at bottom
    drawRect(0, consoleHeight - 2, (float)width, 2, glm::vec4(0.3f, 0.3f, 0.5f, 1.0f));
    
    // Input area background
    float inputY = consoleHeight - lineHeight - padding;
    drawRect(padding, inputY, width - padding * 2, lineHeight, glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
    
    // Get messages from console
    const auto& messages = console.getMessages();
    int scrollOffset = console.getScrollOffset();
    
    int startIdx = std::max(0, (int)messages.size() - visibleLines - scrollOffset);
    int endIdx = std::min((int)messages.size(), startIdx + visibleLines);
    
    float y = padding;
    for (int i = startIdx; i < endIdx; i++) {
        const auto& entry = messages[i];
        drawText(padding, y, fontScale, entry.text, entry.color);
        y += lineHeight;
    }
    
    // Scroll indicator
    if (messages.size() > (size_t)visibleLines) {
        float scrollPercent = 1.0f - (float)scrollOffset / (float)(messages.size() - visibleLines);
        float indicatorH = std::max(20.0f, (consoleHeight - lineHeight - padding * 3) * visibleLines / messages.size());
        float indicatorY = padding + (consoleHeight - lineHeight - padding * 3 - indicatorH) * scrollPercent;
        drawRect(width - 8, indicatorY, 4, indicatorH, glm::vec4(0.5f, 0.5f, 0.6f, 0.7f));
    }
    
    // Render input prompt with cursor
    std::string prompt = "> " + console.getInputText();
    
    // Cursor blink
    static float lastTime = 0.0f;
    float currentTime = static_cast<float>(glfwGetTime());
    if (fmod(currentTime, 1.0f) < 0.5f) {
        prompt.insert(2 + console.getCursorPos(), "|");
    }
    
    drawText(padding + 4, inputY + 2, fontScale, prompt, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
    
    uiShader.unuse();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
