#include "UIManager.h"
#include "../Audio/AudioManager.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#endif

glm::vec4 UIManager::getBlockColor(BlockType type) {
    switch (type) {
        case BlockType::GRASS: return glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);
        case BlockType::DIRT: return glm::vec4(0.5f, 0.3f, 0.1f, 1.0f);
        case BlockType::STONE: return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        case BlockType::SAND: return glm::vec4(0.9f, 0.8f, 0.5f, 1.0f);
        case BlockType::WOOD: return glm::vec4(0.6f, 0.4f, 0.2f, 1.0f);
        case BlockType::LEAVES: return glm::vec4(0.1f, 0.5f, 0.1f, 1.0f);
        case BlockType::SNOW: return glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        case BlockType::ICE: return glm::vec4(0.6f, 0.8f, 1.0f, 0.8f);
        case BlockType::WATER: return glm::vec4(0.2f, 0.4f, 0.8f, 0.6f);
        case BlockType::GRAVEL: return glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        case BlockType::SANDSTONE: return glm::vec4(0.8f, 0.7f, 0.5f, 1.0f);
        case BlockType::LOG: return glm::vec4(0.4f, 0.3f, 0.1f, 1.0f);
        case BlockType::TALL_GRASS: return glm::vec4(0.2f, 0.6f, 0.2f, 1.0f);
        case BlockType::ROSE: return glm::vec4(0.9f, 0.1f, 0.1f, 1.0f);
        case BlockType::BEDROCK: return glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        default: return glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); 
    }
}

void UIManager::update(float deltaTime, double mouseX, double mouseY, bool mousePressed, bool rightMousePressed) {
    // Update preview model animation and rotation when in player settings
    if (currentMenuState == MenuState::PLAYER_SETTINGS && previewModel) {
        previewModel->updateAnimation(deltaTime);
        
        // Handle drag rotation with momentum - use LEFT mouse but only if not over UI
        // Check if mouse is over any UI element
        bool overUI = false;
        for (const auto& el : elements) {
            if (mouseX >= el.x && mouseX <= el.x + el.w &&
                mouseY >= el.y && mouseY <= el.y + el.h) {
                overUI = true;
                break;
            }
        }
        
        if (mousePressed && !overUI) {
            if (!isDraggingModel) {
                // Start dragging
                isDraggingModel = true;
                lastDragX = (float)mouseX;
                previewRotationVelocity = 0.0f;  // Stop any existing momentum
            } else {
                // Continue dragging - calculate velocity from mouse movement
                float dragDelta = (float)mouseX - lastDragX;
                previewRotationVelocity = dragDelta * 2.0f;  // Sensitivity multiplier
                previewRotation += previewRotationVelocity * deltaTime * 60.0f;
                lastDragX = (float)mouseX;
            }
        } else {
            if (isDraggingModel && mousePressed) {
                // Still holding mouse but moved over UI - keep momentum
                isDraggingModel = false;
            } else if (!mousePressed) {
                isDraggingModel = false;
            }
            
            // Apply deceleration (momentum decay)
            previewRotation += previewRotationVelocity * deltaTime * 60.0f;
            previewRotationVelocity *= 0.95f;  // Decay factor - adjust for faster/slower stop
            
            // Stop when velocity is very small
            if (std::abs(previewRotationVelocity) < 0.01f) {
                previewRotationVelocity = 0.0f;
            }
        }
        
        // Keep rotation in reasonable range
        while (previewRotation > 360.0f) previewRotation -= 360.0f;
        while (previewRotation < 0.0f) previewRotation += 360.0f;
    }
    
    // Always update HUD animations (hover, selection bounce, etc.)
    updateHUDAnimations(deltaTime, mouseX, mouseY);
    
    if (!isMenuOpen()) {
        lastMousePressed = mousePressed;
        currentTooltip.clear();
        tooltipTimer = 0.0f;
        return;
    }

    std::function<void()> pendingClick = nullptr;

    // Block clicks if waiting for keybind
    if (waitingForKeyBind) return;
    
    // Track tooltip
    std::string hoveredTooltip;
    bool anyHovered = false;

    for (auto& el : elements) {
        // Hit test
        if (mouseX >= el.x && mouseX <= el.x + el.w &&
            mouseY >= el.y && mouseY <= el.y + el.h) {
            
            el.isHovered = true;
            anyHovered = true;
            
            // Track tooltip for hovered element
            if (!el.tooltip.empty() && Settings::instance().enableTooltips) {
                if (hoveredTooltip.empty()) {
                    hoveredTooltip = el.tooltip;
                    tooltipX = static_cast<float>(mouseX) + 15.0f;
                    tooltipY = static_cast<float>(mouseY) + 15.0f;
                }
            }
            
            if (mousePressed) {
                if (el.isSlider) {
                    // Calculate slider value
                    float pct = (float)(mouseX - el.x) / el.w;
                    float val = el.minVal + pct * (el.maxVal - el.minVal);
                    val = std::max(el.minVal, std::min(el.maxVal, val));
                    
                    if (el.intValueRef) {
                        *el.intValueRef = (int)val;
                        // Update text based on which slider it is
                        if (el.text.find("Render Distance") != std::string::npos)
                            el.text = "Render Distance: " + std::to_string(*el.intValueRef);
                        else if (el.text.find("Master") != std::string::npos)
                            el.text = "Master: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("Music") != std::string::npos)
                            el.text = "Music: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("SFX") != std::string::npos)
                            el.text = "SFX: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("Ambient") != std::string::npos)
                            el.text = "Ambient: " + std::to_string(*el.intValueRef) + "%";
                    } else if (el.valueRef) {
                        *el.valueRef = val;
                        // Update text
                        if (el.text.find("FOV") != std::string::npos)
                            el.text = "FOV: " + std::to_string((int)*el.valueRef);
                        else if (el.text.find("SENSITIVITY") != std::string::npos)
                            el.text = "SENSITIVITY: " + std::to_string(*el.valueRef).substr(0, 4);
                        else if (el.text.find("AO") != std::string::npos)
                            el.text = "AO STRENGTH: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("GAMMA") != std::string::npos)
                            el.text = "GAMMA: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("BRIGHTNESS") != std::string::npos)
                            el.text = "BRIGHTNESS: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("Sharpness") != std::string::npos)
                            el.text = "Sharpness: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("Shadow Distance") != std::string::npos)
                            el.text = "Shadow Distance: " + std::to_string((int)*el.valueRef);
                        else if (el.text.find("Master Volume") != std::string::npos) {
                            el.text = "Master Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setMasterVolume(*el.valueRef);
                        }
                        else if (el.text.find("Music Volume") != std::string::npos) {
                            el.text = "Music Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, *el.valueRef);
                        }
                        else if (el.text.find("Sound Effects") != std::string::npos) {
                            el.text = "Sound Effects: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, *el.valueRef);
                        }
                        else if (el.text.find("Ambient Volume") != std::string::npos) {
                            el.text = "Ambient Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::WEATHER, *el.valueRef);
                        }
                    }
                    
                    if (onSettingsChanged) onSettingsChanged();
                } else if (!lastMousePressed) {
                    // Button clicks (Rising Edge)
                    // Play UI click sound for all button interactions
                    Audio::AudioManager::instance().playSound(Audio::SoundType::UI_CLICK, 0.5f);
                    
                    if (el.isKeybind) {
                        waitingForKeyBind = true;
                        keyBindPtr = el.keyBindRef;
                        el.text = "PRESS ANY KEY...";
                    } else if (el.onClick) {
                        if (el.boolValueRef) {
                            *el.boolValueRef = !(*el.boolValueRef);
                            // Update text for toggle
                            size_t colonPos = el.text.find(":");
                            if (colonPos != std::string::npos) {
                                std::string prefix = el.text.substr(0, colonPos + 1);
                                el.text = prefix + (*el.boolValueRef ? " ON" : " OFF");
                            }
                            if (onSettingsChanged) onSettingsChanged();
                        } else if (el.intValueRef) {
                            // Cycle integer value
                            *el.intValueRef = (*el.intValueRef + 1);
                            if (*el.intValueRef > (int)el.maxVal) *el.intValueRef = (int)el.minVal;
                            
                            if (el.text.find("WINDOW MODE") != std::string::npos) {
                                std::string modeStr;
                                if (*el.intValueRef == 0) modeStr = "WINDOWED";
                                else if (*el.intValueRef == 1) modeStr = "FULLSCREEN";
                                else if (*el.intValueRef == 2) modeStr = "BORDERLESS";
                                el.text = "WINDOW MODE: " + modeStr;
                            }
                            
                            if (el.text.find("AA METHOD") != std::string::npos) {
                                el.text = "AA METHOD: " + std::string(Settings::AA_METHOD_NAMES[*el.intValueRef]);
                            }
                            
                            if (el.text.find("RT Quality") != std::string::npos) {
                                el.text = "RT Quality: " + std::string(Settings::RT_QUALITY_NAMES[*el.intValueRef]);
                            }

                            if (el.text.find("Color Template") != std::string::npos) {
                                Settings::instance().applyColorTemplate(*el.intValueRef);
                                el.text = "Color Template: " + std::string(Settings::COLOR_TEMPLATE_NAMES[*el.intValueRef]);
                                pendingClick = [this]() {
                                    this->setupVideoSettingsMenu();
                                    if (onSettingsChanged) onSettingsChanged();
                                };
                                break;
                            }
                            
                            if (el.text.find("Shadows:") != std::string::npos && el.text.find("RT") == std::string::npos) {
                                el.text = "Shadows: " + std::string(Settings::SHADOW_METHOD_NAMES[*el.intValueRef]);
                            }
                            
                            // Handle preset selector - apply the preset after cycling and refresh UI
                            if (el.intValueRef == &Settings::instance().graphicsPreset) {
                                Settings::instance().applyPreset(*el.intValueRef);
                                // Refresh video settings UI to show updated values
                                pendingClick = [this]() {
                                    this->setupVideoSettingsMenu();
                                    if (onSettingsChanged) onSettingsChanged();
                                };
                                break; // Exit early since we rebuild after loop
                            }
                            
                            if (onSettingsChanged) onSettingsChanged();
                        } else {
                            pendingClick = el.onClick;
                            break; // Stop processing to avoid issues with vector modification
                        }
                    }
                }
            }
        } else {
            el.isHovered = false;
        }
    }


    
    // Right Click Handling
    if (rightMousePressed && !lastRightMousePressed) {
        for (auto& el : elements) {
            if (el.isHovered && el.isInventoryItem && el.onRightClick) {
                el.onRightClick();
            }
        }
    }
    
    // Map interaction handling
    if (currentMenuState == MenuState::MAP && !elements.empty()) {
        const auto& mapEl = elements[0];
        bool mouseOnMap = (mouseX >= mapEl.x && mouseX <= mapEl.x + mapEl.w &&
                          mouseY >= mapEl.y && mouseY <= mapEl.y + mapEl.h);
        
        // Must match the rendering calculation exactly!
        int pixelRes = 96;
        float worldPerPixel = mapScale * 4.0f;
        float halfMapWorld = (pixelRes / 2.0f) * worldPerPixel;
        float visibleWorldSize = halfMapWorld * 2.0f;
        
        // Right-click drag for panning
        if (mouseOnMap && rightMousePressed && !lastRightMousePressed) {
            // Start dragging
            mapDragging = true;
            mapDragStartX = static_cast<float>(mouseX);
            mapDragStartZ = static_cast<float>(mouseY);
            mapDragCenterX = mapCenterX;
            mapDragCenterZ = mapCenterZ;
            mapPanVelocityX = 0.0f;
            mapPanVelocityZ = 0.0f;
        }
        
        if (mapDragging && rightMousePressed) {
            // Calculate drag delta in world units
            float dragPixelsX = static_cast<float>(mouseX) - mapDragStartX;
            float dragPixelsZ = static_cast<float>(mouseY) - mapDragStartZ;
            float dragWorldX = (dragPixelsX / mapEl.w) * visibleWorldSize;
            float dragWorldZ = (dragPixelsZ / mapEl.h) * visibleWorldSize;
            
            // Directly update center (inverted because dragging moves the view)
            mapCenterX = mapDragCenterX - dragWorldX;
            mapCenterZ = mapDragCenterZ - dragWorldZ;
            mapTargetCenterX = mapCenterX;
            mapTargetCenterZ = mapCenterZ;
            
            // Track velocity for momentum
            mapPanVelocityX = -dragWorldX * 0.5f;
            mapPanVelocityZ = -dragWorldZ * 0.5f;
        }
        
        if (!rightMousePressed && lastRightMousePressed && mapDragging) {
            // Stop dragging - momentum will continue
            mapDragging = false;
        }
        
        // Left-click teleport (only if not dragging)
        if (mouseOnMap && mousePressed && !lastMousePressed && !mapDragging) {
            // Convert screen coords to world coords
            float relX = (float)(mouseX - mapEl.x) / (float)mapEl.w - 0.5f; // -0.5 to 0.5
            float relY = (float)(mouseY - mapEl.y) / (float)mapEl.h - 0.5f;
            
            float worldX = mapCenterX + relX * visibleWorldSize;
            float worldZ = mapCenterZ + relY * visibleWorldSize;
            
            // Teleport
            if (onTeleport) {
                onTeleport(worldX, worldZ);
                setMenuState(MenuState::NONE); // Close map after teleport
            }
        }
        
        // Apply momentum when not dragging
        if (!mapDragging) {
            mapCenterX += mapPanVelocityX * deltaTime;
            mapCenterZ += mapPanVelocityZ * deltaTime;
            mapTargetCenterX = mapCenterX;
            mapTargetCenterZ = mapCenterZ;
            mapPanVelocityX *= 0.90f;  // Decay
            mapPanVelocityZ *= 0.90f;
            if (std::abs(mapPanVelocityX) < 1.0f) mapPanVelocityX = 0.0f;
            if (std::abs(mapPanVelocityZ) < 1.0f) mapPanVelocityZ = 0.0f;
        }
        
        // Snap zoom to target to prevent pixel shimmer from fractional scale
        mapScale = mapTargetScale;
    }

    if (pendingClick) {
        pendingClick();
    }
    
    // Update tooltip state
    if (!hoveredTooltip.empty()) {
        if (currentTooltip == hoveredTooltip) {
            tooltipTimer += deltaTime;
        } else {
            currentTooltip = hoveredTooltip;
            tooltipTimer = 0.0f;
        }
    } else {
        currentTooltip.clear();
        tooltipTimer = 0.0f;
    }
    
    lastMousePressed = mousePressed;
    lastRightMousePressed = rightMousePressed;
}

void UIManager::updateHUDAnimations(float deltaTime, double mouseX, double mouseY) {
    // Calculate hotbar slot positions for hover detection
    float slotSize = 48.0f;
    float slotGap = 3.0f;
    int slots = 9;
    float totalW = slots * slotSize + (slots - 1) * slotGap;
    float hotbarPadding = 6.0f;
    float hotbarW = totalW + hotbarPadding * 2;
    float hotbarH = slotSize + hotbarPadding * 2;
    float startX = (width - hotbarW) / 2.0f;
    float startY = height - hotbarH - 8.0f;
    float slotStartX = startX + hotbarPadding;
    float slotStartY = startY + hotbarPadding;
    
    // Detect which slot is hovered
    int newHoveredSlot = -1;
    for (int i = 0; i < slots; ++i) {
        float x = slotStartX + i * (slotSize + slotGap);
        float y = slotStartY;
        
        if (mouseX >= x && mouseX <= x + slotSize &&
            mouseY >= y - 10 && mouseY <= y + slotSize + 10) {  // Slightly larger hit area
            newHoveredSlot = i;
            break;
        }
    }
    
    // Update hover animations
    float hoverSpeed = 8.0f;
    float rotationSpeed = 45.0f;  // Degrees per second when hovered
    for (int i = 0; i < 9; ++i) {
        float targetHover = (i == newHoveredSlot) ? 1.0f : 0.0f;
        float diff = targetHover - hotbarSlotHover[i];
        hotbarSlotHover[i] += diff * deltaTime * hoverSpeed;
        hotbarSlotHover[i] = std::clamp(hotbarSlotHover[i], 0.0f, 1.0f);
        
        // Bounce animation decays
        if (hotbarSlotBounce[i] > 0.0f) {
            hotbarSlotBounce[i] -= deltaTime * 4.0f;
            if (hotbarSlotBounce[i] < 0.0f) hotbarSlotBounce[i] = 0.0f;
        }
        
        // Rotation animation for hovered slots - spin slowly when hovered
        if (i == newHoveredSlot) {
            hotbarSlotRotation[i] += deltaTime * rotationSpeed;
            if (hotbarSlotRotation[i] >= 360.0f) hotbarSlotRotation[i] -= 360.0f;
        } else {
            // Smoothly return to default rotation
            if (hotbarSlotRotation[i] > 0.0f) {
                hotbarSlotRotation[i] -= deltaTime * rotationSpeed * 2.0f;
                if (hotbarSlotRotation[i] < 0.0f) hotbarSlotRotation[i] = 0.0f;
            }
        }
    }
    
    // Update inventory item rotations when inventory is open
    if (currentMenuState == MenuState::INVENTORY) {
        // Find which inventory item is hovered
        int newInventoryHovered = -1;
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& el = elements[i];
            if (el.isInventoryItem && el.isHovered && el.inventoryIndex >= 0) {
                newInventoryHovered = el.inventoryIndex;
                break;
            }
        }
        inventoryHoveredIndex = newInventoryHovered;
        
        // Update rotations for inventory items
        for (int i = 0; i < 256; ++i) {
            if (i == inventoryHoveredIndex) {
                inventoryItemRotations[i] += deltaTime * rotationSpeed;
                if (inventoryItemRotations[i] >= 360.0f) inventoryItemRotations[i] -= 360.0f;
            } else {
                // Smoothly return to default rotation
                if (inventoryItemRotations[i] > 0.0f) {
                    inventoryItemRotations[i] -= deltaTime * rotationSpeed * 2.0f;
                    if (inventoryItemRotations[i] < 0.0f) inventoryItemRotations[i] = 0.0f;
                }
            }
        }
    }
    
    hoveredSlot = newHoveredSlot;
    
    // Selection bounce animation
    static int lastSelectedSlot = -1;
    if (selectedSlot != lastSelectedSlot) {
        selectionBounce = 1.0f;
        hotbarSlotBounce[selectedSlot] = 1.0f;
        lastSelectedSlot = selectedSlot;
    }
    if (selectionBounce > 0.0f) {
        selectionBounce -= deltaTime * 5.0f;
        if (selectionBounce < 0.0f) selectionBounce = 0.0f;
    }
    
    // Health flash decays
    if (healthFlash > 0.0f) {
        healthFlash -= deltaTime * 3.0f;
        if (healthFlash < 0.0f) healthFlash = 0.0f;
    }
    
    // XP glow decays
    if (xpBarGlow > 0.0f) {
        xpBarGlow -= deltaTime * 2.0f;
        if (xpBarGlow < 0.0f) xpBarGlow = 0.0f;
    }
    
    // Track health changes for flash effect
    static int lastHealth = 20;
    if (playerHealth < lastHealth) {
        healthFlash = 1.0f;  // Trigger flash when taking damage
    }
    lastHealth = playerHealth;
    
    // Track XP changes for glow effect
    static float lastXP = 0.0f;
    if (playerXP > lastXP) {
        xpBarGlow = 1.0f;  // Trigger glow when gaining XP
    }
    lastXP = playerXP;
}

void UIManager::addChatMessage(const std::string& senderName, const std::string& message) {
    ChatEntry entry;
    entry.playerName = senderName;
    entry.message = message;
    entry.timestamp = 0.0f; // Will be updated in render
    chatMessages.push_back(entry);
    
    // Keep only last N messages based on settings
    int maxMessages = Settings::instance().maxChatMessages;
    while (chatMessages.size() > static_cast<size_t>(maxMessages)) {
        chatMessages.erase(chatMessages.begin());
    }
}

void UIManager::openChat() {
    chatInput.clear();
    currentMenuState = MenuState::CHAT;
}

void UIManager::closeChat() {
    chatInput.clear();
    currentMenuState = MenuState::NONE;
}

void UIManager::loadPreviewModel(int modelIndex) {
    if (modelIndex < 0 || modelIndex >= Settings::NUM_PLAYER_MODELS) return;
    if (previewModelIndex == modelIndex && previewModel) return; // Already loaded
    
    previewModelIndex = modelIndex;
    previewRotation = 0.0f;
    
    try {
        std::string path = Settings::PLAYER_MODEL_PATHS[modelIndex];
        std::cout << "Loading preview model: " << path << std::endl;
        previewModel = std::make_shared<ModelSystem::Model>(path);
        std::cout << "Preview model loaded successfully" << std::endl;
        
        // Try to play idle animation
        auto animations = previewModel->getAnimationNames();
        std::cout << "Available animations: " << animations.size() << std::endl;
        for (const auto& anim : animations) {
            std::cout << "  - " << anim << std::endl;
            // Look for idle animation (case insensitive check)
            std::string lowerAnim = anim;
            for (auto& c : lowerAnim) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowerAnim.find("idle") != std::string::npos) {
                previewModel->playAnimation(anim, true);
                std::cout << "Playing animation: " << anim << std::endl;
                break;
            }
        }
        // If no idle found, play first animation if available
        if (previewModel->getCurrentAnimation().empty() && !animations.empty()) {
            previewModel->playAnimation(animations[0], true);
            std::cout << "Playing first animation: " << animations[0] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load preview model: " << e.what() << std::endl;
        previewModel = nullptr;
    }
}
