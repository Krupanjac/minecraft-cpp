#include "UIManager.h"
#include "../World/WorldSerializer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <random>
#include <climits>

UIManager::UIManager() : vao(0), vbo(0), width(1280), height(720), showDebug(false), currentFPS(0.0f), currentMenuState(MenuState::MAIN_MENU) {}

void UIManager::initialize(int windowWidth, int windowHeight) {
    width = windowWidth;
    height = windowHeight;

    // Simple UI shader
    const char* vertSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 uProjection;
        uniform mat4 uModel;
        void main() {
            gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* fragSrc = R"(
        #version 450 core
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    uiShader.loadFromSource(vertSrc, fragSrc);

    // Setup quad VAO
    float vertices[] = { 
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f, 
    
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    setupMainMenu();
}

void UIManager::handleResize(int w, int h) {
    width = w;
    height = h;
    // Re-setup menu to center elements
    if (isMenuOpen()) setMenuState(currentMenuState); 
}

void UIManager::setMenuState(MenuState state) {
    currentMenuState = state;
    elements.clear();
    
    switch (state) {
        case MenuState::MAIN_MENU: setupMainMenu(); break;
        case MenuState::IN_GAME_MENU: setupInGameMenu(); break;
        case MenuState::SETTINGS: setupSettingsMenu(); break;
        case MenuState::VIDEO_SETTINGS: setupVideoSettingsMenu(); break;
        case MenuState::CONTROLS: setupControlsMenu(); break;
        case MenuState::LOAD_GAME: setupLoadGameMenu(); break;
        case MenuState::NEW_GAME: setupNewGameMenu(); break;
        case MenuState::INVENTORY: setupInventoryMenu(); break;
        case MenuState::NONE: break;
    }
}

void UIManager::handleCharInput(unsigned int codepoint) {
    if (!isMenuOpen()) return;
    
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (codepoint >= 32 && codepoint <= 126) {
                *el.textRef += (char)codepoint;
            }
            // Update display text
            if (el.text.find("NAME:") != std::string::npos) el.text = "NAME: " + *el.textRef;
            if (el.text.find("SEED:") != std::string::npos) el.text = "SEED: " + *el.textRef;
        }
    }
}

void UIManager::handleKeyInput(int key) {
    if (!isMenuOpen()) return;
    
    // For text input
    if (currentMenuState == MenuState::NEW_GAME) {
       for (auto& el : elements) {
           if (el.isInput && el.isHovered) {
               if (key == 259 && el.textRef && !el.textRef->empty()) { // Backspace
                   el.textRef->pop_back();
                   if (el.textRef == &newWorldName) el.text = "World Name: " + *el.textRef;
                   else if (el.textRef == &newWorldSeed) el.text = "Seed: " + *el.textRef;
               }
           }
       }
    } else { // Original text input handling for other menus if applicable
        for (auto& el : elements) {
            if (el.isInput && el.isHovered && el.textRef) {
                if (key == 259) { // GLFW_KEY_BACKSPACE
                    if (!el.textRef->empty()) el.textRef->pop_back();
                    // Update display text
                    if (el.text.find("NAME:") != std::string::npos) el.text = "NAME: " + *el.textRef;
                    if (el.text.find("SEED:") != std::string::npos) el.text = "SEED: " + *el.textRef;
                }
            }
        }
    }
    
    // For Keybinding
    if (waitingForKeyBind && keyBindPtr) {
        *keyBindPtr = key;
        waitingForKeyBind = false;
        keyBindPtr = nullptr;
        setupControlsMenu(); // Refresh text
        if (onSettingsChanged) onSettingsChanged();
    }
}

void UIManager::setupMainMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    elements.push_back({centerX - btnW/2, centerY - 100, btnW, btnH, "NEW GAME", false, [this]() { 
        setMenuState(MenuState::NEW_GAME); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 100 + btnH + gap, btnW, btnH, "LOAD GAME", false, [this]() { 
        setMenuState(MenuState::LOAD_GAME); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*2, btnW, btnH, "SETTINGS", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*3, btnW, btnH, "EXIT", false, [this]() { 
        if (onExit) onExit(); 
    }});
}

void UIManager::setupInGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    elements.push_back({centerX - btnW/2, centerY - 50, btnW, btnH, "RESUME", false, [this]() { 
        setMenuState(MenuState::NONE); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 50 + btnH + gap, btnW, btnH, "SAVE GAME", false, [this]() { 
        if (onSave) onSave();
    }});

    elements.push_back({centerX - btnW/2, centerY - 50 + (btnH + gap)*2, btnW, btnH, "SETTINGS", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 50 + (btnH + gap)*3, btnW, btnH, "MAIN MENU", false, [this]() { 
        if (onSave) onSave(); // Auto save on exit to menu
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::setupSettingsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 40.0f;
    float gap = 10.0f;
    float startY = cy - 100.0f;

    elements.push_back({cx - btnW/2, startY, btnW, btnH, "VIDEO SETTINGS", false, [this]() { 
        setMenuState(MenuState::VIDEO_SETTINGS); 
    }});
    startY += btnH + gap;

    // Controls
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "CONTROLS", false, [this]() {
         setMenuState(MenuState::CONTROLS);
    }});
    startY += btnH + gap;

    // Back
    elements.push_back({cx - btnW/2, startY + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::setupVideoSettingsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 30.0f;
    float gap = 10.0f;
    float startY = cy - 150.0f;

    auto& s = Settings::instance();

    // Render Distance
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "RENDER DIST: " + std::to_string(s.renderDistance), false, nullptr, true, nullptr, &s.renderDistance, nullptr, 2.0f, 32.0f});
    startY += btnH + gap;

    // FOV
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "FOV: " + std::to_string((int)s.fov), false, nullptr, true, &s.fov, nullptr, nullptr, 30.0f, 110.0f});
    startY += btnH + gap;

    // AO Strength
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "AO STRENGTH: " + std::to_string(s.aoStrength).substr(0, 3), false, nullptr, true, &s.aoStrength, nullptr, nullptr, 0.0f, 2.0f});
    startY += btnH + gap;

    // Gamma
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "GAMMA: " + std::to_string(s.gamma).substr(0, 3), false, nullptr, true, &s.gamma, nullptr, nullptr, 1.0f, 3.0f});
    startY += btnH + gap;

    // Exposure (Brightness)
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "BRIGHTNESS: " + std::to_string(s.exposure).substr(0, 3), false, nullptr, true, &s.exposure, nullptr, nullptr, 0.1f, 5.0f});
    startY += btnH + gap;

    // Sun Size
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "SUN SIZE: " + std::to_string(s.sunSize).substr(0, 3), false, nullptr, true, &s.sunSize, nullptr, nullptr, 0.5f, 10.0f});
    startY += btnH + gap;

    // Moon Size
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "MOON SIZE: " + std::to_string(s.moonSize).substr(0, 3), false, nullptr, true, &s.moonSize, nullptr, nullptr, 0.5f, 10.0f});
    startY += btnH + gap;

    // VSync (Toggle)
    std::string vsyncText = "VSYNC: " + std::string(s.vsync ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, vsyncText, false, [](){}, false, nullptr, nullptr, &s.vsync});
    startY += btnH + gap;

    // SSAO (Toggle)
    std::string ssaoText = "SSAO: " + std::string(s.enableSSAO ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, ssaoText, false, [](){}, false, nullptr, nullptr, &s.enableSSAO});
    startY += btnH + gap;

    // Volumetrics (Toggle)
    std::string volText = "VOLUMETRICS: " + std::string(s.enableVolumetrics ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, volText, false, [](){}, false, nullptr, nullptr, &s.enableVolumetrics});
    startY += btnH + gap;

    // TAA (Toggle)
    std::string taaText = "TAA: " + std::string(s.enableTAA ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, taaText, false, [](){}, false, nullptr, nullptr, &s.enableTAA});
    startY += btnH + gap;

    // Shadows (Toggle)
    std::string shadowText = "SHADOWS: " + std::string(s.enableShadows ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, shadowText, false, [](){}, false, nullptr, nullptr, &s.enableShadows});
    startY += btnH + gap;

    // Shadow Distance
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "SHADOW DIST: " + std::to_string((int)s.shadowDistance), false, nullptr, true, &s.shadowDistance, nullptr, nullptr, 50.0f, 300.0f});
    startY += btnH + gap;

    // Fullscreen (Cycle)
    std::string fsText = "WINDOW MODE: ";
    if (s.fullscreen == 0) fsText += "WINDOWED";
    else if (s.fullscreen == 1) fsText += "FULLSCREEN";
    else if (s.fullscreen == 2) fsText += "BORDERLESS";
    
    elements.push_back({cx - btnW/2, startY, btnW, btnH, fsText, false, [](){}, false, nullptr, &s.fullscreen, nullptr, 0.0f, 2.0f});
    startY += btnH + gap;

    // Back
    elements.push_back({cx - btnW/2, startY + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
}

std::string getKeyName(int key) {
    if (key > 32 && key <= 126) {
        return std::string(1, (char)key);
    }
    switch (key) {
        case 32: return "SPACE";
        case 256: return "ESC";
        case 257: return "ENTER";
        case 258: return "TAB";
        case 259: return "BACKSPACE";
        case 260: return "INSERT";
        case 261: return "DELETE";
        case 262: return "RIGHT";
        case 263: return "LEFT";
        case 264: return "DOWN";
        case 265: return "UP";
        case 266: return "PAGE UP";
        case 267: return "PAGE DOWN";
        case 268: return "HOME";
        case 269: return "END";
        case 280: return "CAPS LOCK";
        case 281: return "SCROLL LOCK";
        case 282: return "NUM LOCK";
        case 283: return "PRINT SCREEN";
        case 284: return "PAUSE";
        case 290: return "F1";
        case 291: return "F2";
        case 292: return "F3";
        case 293: return "F4";
        case 294: return "F5";
        case 295: return "F6";
        case 296: return "F7";
        case 297: return "F8";
        case 298: return "F9";
        case 299: return "F10";
        case 300: return "F11";
        case 301: return "F12";
        case 340: return "L-SHIFT";
        case 341: return "L-CTRL";
        case 342: return "L-ALT";
        case 343: return "L-SUPER";
        case 344: return "R-SHIFT";
        case 345: return "R-CTRL";
        case 346: return "R-ALT";
        case 347: return "R-SUPER";
        case 348: return "MENU";
        default: return "KEY " + std::to_string(key);
    }
}

void UIManager::setupControlsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 30.0f;
    float gap = 5.0f;
    float startY = cy - 200.0f;

    auto& k = Settings::instance().keys;
    
    auto addKeyBtn = [&](const std::string& label, int* keyRef) {
        std::string text = label + ": " + getKeyName(*keyRef);
        UIElement el;
        el.x = cx - btnW/2;
        el.y = startY;
        el.w = btnW;
        el.h = btnH;
        el.text = text;
        el.isKeybind = true;
        el.keyBindRef = keyRef;
        // onClick handled in update loop for keybinds
        elements.push_back(el);
        startY += btnH + gap;
    };

    addKeyBtn("FORWARD", &k.forward);
    addKeyBtn("BACKWARD", &k.backward);
    addKeyBtn("LEFT", &k.left);
    addKeyBtn("RIGHT", &k.right);
    addKeyBtn("JUMP", &k.jump);
    addKeyBtn("SPRINT", &k.sprint);
    addKeyBtn("SNEAK", &k.sneak);
    addKeyBtn("INVENTORY", &k.inventory);

    // Back
    startY += 10;
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
}

void UIManager::setupLoadGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float startY = height / 2.0f - 150.0f;
    float btnW = 300.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    std::vector<std::string> worlds = WorldSerializer::getAvailableWorlds();
    
    for (size_t i = 0; i < worlds.size(); i++) {
        std::string wName = worlds[i];
        elements.push_back({centerX - btnW/2, startY + i * (btnH + gap), btnW, btnH, wName, false, [this, wName]() { 
            if (onLoadGame) onLoadGame(wName);
            setMenuState(MenuState::NONE);
        }});
    }

    elements.push_back({centerX - btnW/2, startY + worlds.size() * (btnH + gap) + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::setupNewGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    // Input fields
    static std::string nameInput = "New World";
    
    // Generate a random seed immediately for display
    static std::string seedInput = "";
    if (seedInput.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(INT_MIN, INT_MAX);
        seedInput = std::to_string(dis(gen));
    }

    UIElement nameField = {centerX - btnW/2, centerY - 100, btnW, btnH, "NAME: " + nameInput, false, nullptr, false, nullptr, nullptr, nullptr, 0.0f, 0.0f, true, &nameInput};
    elements.push_back(nameField);

    UIElement seedField = {centerX - btnW/2, centerY - 100 + btnH + gap, btnW, btnH, "SEED: " + seedInput, false, nullptr, false, nullptr, nullptr, nullptr, 0.0f, 0.0f, true, &seedInput};
    elements.push_back(seedField);

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*2 + 20, btnW, btnH, "CREATE WORLD", false, [this]() { 
        int seed = 0;
        std::string seedStr = *elements[1].textRef;
        
        try { 
            seed = std::stoi(seedStr); 
        } catch(...) { 
            // Fallback if user entered garbage, though we pre-filled it
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dis(INT_MIN, INT_MAX);
            seed = dis(gen);
        }
        
        if (onNewGame) onNewGame(*elements[0].textRef, seed);
        setMenuState(MenuState::NONE);
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*3 + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::setupInventoryMenu() {
    elements.clear();
    
    // Inventory Grid similar to Minecraft
    float slotSize = 60.0f;
    float gap = 10.0f;
    int cols = 9; 
    
    std::vector<BlockType> blocks = {
        BlockType::GRASS, BlockType::DIRT, BlockType::STONE, BlockType::SAND, 
        BlockType::WOOD, BlockType::LOG, BlockType::LEAVES, BlockType::GRAVEL, 
        BlockType::SANDSTONE, BlockType::SNOW, BlockType::ICE, BlockType::WATER,
        BlockType::TALL_GRASS, BlockType::ROSE, BlockType::BEDROCK
    };
    
    float totalW = cols * slotSize + (cols - 1) * gap;
    // float startX = (width - totalW) / 2.0f;
    // Align with Minecraft style, usually centered
    float startX = (width - totalW) / 2.0f;
    float startY = height / 2.0f - slotSize - 20.0f; 
    
    // Draw "Survival Inventory" label
    elements.push_back({width/2.0f - 150, startY - 60, 300, 30, "INVENTORY", false, nullptr});

    for (size_t i = 0; i < blocks.size(); i++) {
        int col = i % cols;
        int row = i / cols;
        
        float x = startX + col * (slotSize + gap);
        float y = startY + row * (slotSize + gap);
        
        UIElement el;
        el.x = x;
        el.y = y;
        el.w = slotSize;
        el.h = slotSize;
        el.text = ""; 
        el.isInventoryItem = true;
        el.blockType = blocks[i];
        el.onClick = [this, type = blocks[i]]() {
            // Left click selects slot for placement (legacy) or maybe primary functionality?
            // Actually, let's keep it simple: Left click just does nothing special or maybe selects for later drag/drop if we implemented it.
            // But per plan, we rely on Right Click for assignment.
        };
        el.onRightClick = [this, type = blocks[i]]() {
             // Assign to current hotbar slot
             hotbar[selectedSlot] = type;
        };
        elements.push_back(el);
    }
    
    // Add close instruction
    elements.push_back({width/2.0f - 100, height - 100.0f, 200, 30, "PRESS [E] TO CLOSE", false, nullptr});
}

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

void UIManager::update(float /*deltaTime*/, double mouseX, double mouseY, bool mousePressed, bool rightMousePressed) {
    if (!isMenuOpen()) {
        lastMousePressed = mousePressed;
        return;
    }

    std::function<void()> pendingClick = nullptr;

    // Block clicks if waiting for keybind
    if (waitingForKeyBind) return;

    for (auto& el : elements) {
        // Hit test
        if (mouseX >= el.x && mouseX <= el.x + el.w &&
            mouseY >= el.y && mouseY <= el.y + el.h) {
            
            el.isHovered = true;
            
            if (mousePressed) {
                if (el.isSlider) {
                    // Calculate slider value
                    float pct = (float)(mouseX - el.x) / el.w;
                    float val = el.minVal + pct * (el.maxVal - el.minVal);
                    val = std::max(el.minVal, std::min(el.maxVal, val));
                    
                    if (el.intValueRef) {
                        *el.intValueRef = (int)val;
                        el.text = "RENDER DIST: " + std::to_string(*el.intValueRef);
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
                    }
                    
                    if (onSettingsChanged) onSettingsChanged();
                } else if (!lastMousePressed) {
                    // Button clicks (Rising Edge)
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

    if (pendingClick) {
        pendingClick();
    }
    
    lastMousePressed = mousePressed;
    lastRightMousePressed = rightMousePressed;
}

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

            for (const auto& el : elements) {
                glm::vec4 color = el.isHovered ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f) : glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
                
                if (el.isInventoryItem) {
                    // Draw slot background (darker)
                     color = el.isHovered ? glm::vec4(0.5f, 0.5f, 0.5f, 0.9f) : glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);
                     drawRect(el.x, el.y, el.w, el.h, color);
                     
                     // Draw block color
                     glm::vec4 blkColor = getBlockColor(el.blockType);
                     drawRect(el.x + 6, el.y + 6, el.w - 12, el.h - 12, blkColor);
                     
                     // Draw selection highlight
                     if (el.blockType == selectedBlock) {
                         glm::vec4 hl(1.0f, 1.0f, 1.0f, 1.0f);
                         float t = 4.0f;
                         drawRect(el.x, el.y, el.w, t, hl); // Top
                         drawRect(el.x, el.y + el.h - t, el.w, t, hl); // Bottom
                         drawRect(el.x, el.y, t, el.h, hl); // Left
                         drawRect(el.x + el.w - t, el.y, t, el.h, hl); // Right
                     }
                     continue;
                }

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
        }

        uiShader.unuse();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    
    // Always render HUD unless in Main Menu / Settings / Load Game (basically if we are 'in game' or 'inventory')
    // Specifically: NONE (playing) or INVENTORY. Not IN_GAME_MENU (pause).
    if (currentMenuState == MenuState::NONE || currentMenuState == MenuState::INVENTORY) {
        renderHUD();
    }
}

void UIManager::renderHUD() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    // Hotbar Settings
    float slotSize = 40.0f;
    float gap = 4.0f;
    int slots = 9;
    float totalW = slots * slotSize + (slots - 1) * gap;
    float startX = (width - totalW) / 2.0f;
    float startY = height - slotSize - 10.0f;
    
    // 1. Hotbar Background
    for (int i = 0; i < slots; ++i) {
        float x = startX + i * (slotSize + gap);
        float y = startY;
        
        // Selection highlight
        if (i == selectedSlot) {
            drawRect(x - 2, y - 2, slotSize + 4, slotSize + 4, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        
        // Slot background
        drawRect(x, y, slotSize, slotSize, glm::vec4(0.2f, 0.2f, 0.2f, 0.8f));
        
        // Item
        BlockType type = hotbar[i];
        if (type != BlockType::AIR) {
            drawRect(x + 4, y + 4, slotSize - 8, slotSize - 8, getBlockColor(type));
        }
    }
    
    // 2. Health Bar (Hearts) - Left above hotbar
    // 10 hearts, 2 health per heart
    float heartSize = 16.0f;
    float heartGap = 2.0f;
    float healthStartX = startX;
    float healthStartY = startY - heartSize - 15.0f; // Above XP bar usually, but simplifying layer
    
    // Draw max health background? Maybe just current health for now
    for (int i = 0; i < 10; ++i) {
        float x = healthStartX + i * (heartSize + heartGap);
        // Background (empty heart - dark red)
        drawRect(x, healthStartY, heartSize, heartSize, glm::vec4(0.3f, 0.0f, 0.0f, 1.0f));
        
        // Filled based on health
        int heartHealth = (i + 1) * 2;
        if (playerHealth >= heartHealth) {
            // Full heart
            drawRect(x, healthStartY, heartSize, heartSize, glm::vec4(0.9f, 0.1f, 0.1f, 1.0f));
        } else if (playerHealth == heartHealth - 1) {
            // Half heart
            drawRect(x, healthStartY, heartSize / 2, heartSize, glm::vec4(0.9f, 0.1f, 0.1f, 1.0f));
        }
    }
    
    // 3. Food Bar (Hunger) - Right above hotbar
    float foodStartX = startX + totalW - (10 * (heartSize + heartGap)) + heartGap; // Align right
    for (int i = 0; i < 10; ++i) {
        // Draw reverse order to align right? Or just draw left-to-right from calculated start
        // Minecraft draws right-to-left usually but visual result is same 
        float x = foodStartX + i * (heartSize + heartGap);
        
         // Background (empty food - dark brown)
        drawRect(x, healthStartY, heartSize, heartSize, glm::vec4(0.3f, 0.2f, 0.1f, 1.0f));
        
        // Filled
        int foodLevel = (i + 1) * 2; // Logic is tricky if we want right-alignment visual but usually simple enough
        // Actually, MC fills from right to left? No, usually 0 is left. 
        // Let's just draw 0..9 left to right.
        
        if (playerFood >= foodLevel) {
            drawRect(x, healthStartY, heartSize, heartSize, glm::vec4(0.6f, 0.4f, 0.2f, 1.0f));
        }
    }
    
    // 4. XP Bar - Between hotbar and stats
    float xpH = 5.0f;
    float xpY = startY - xpH - 4.0f;
    // Background
    drawRect(startX, xpY, totalW, xpH, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
    // Progress
    drawRect(startX, xpY, totalW * playerXP, xpH, glm::vec4(0.2f, 0.9f, 0.2f, 1.0f));
    
    uiShader.unuse();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

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

void UIManager::updateDebugInfo(float fps, const std::string& blockName, const glm::vec3& playerPos, const glm::vec3& playerVel) {
    currentFPS = fps;
    currentBlockName = blockName;
    currentPlayerPos = playerPos;
    currentPlayerVel = playerVel;
}


