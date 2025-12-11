#include "UIManager.h"
#include "../World/WorldSerializer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>

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
        case MenuState::LOAD_GAME: setupLoadGameMenu(); break;
        case MenuState::NEW_GAME: setupNewGameMenu(); break;
        case MenuState::NONE: break;
    }
}

void UIManager::handleCharInput(unsigned int codepoint) {
    if (!isMenuOpen()) return;
    
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (codepoint == 8) { // Backspace
                if (!el.textRef->empty()) el.textRef->pop_back();
            } else if (codepoint >= 32 && codepoint <= 126) {
                *el.textRef += (char)codepoint;
            }
            // Update display text
            if (el.text.find("NAME:") != std::string::npos) el.text = "NAME: " + *el.textRef;
            if (el.text.find("SEED:") != std::string::npos) el.text = "SEED: " + *el.textRef;
        }
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

    elements.push_back({centerX - btnW/2, centerY - 50 + (btnH + gap)*2, btnW, btnH, "MAIN MENU", false, [this]() { 
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

    // Placeholder for Controls
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "CONTROLS (TODO)", false, nullptr});
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

    // Fullscreen (Toggle) - Note: Requires restart or complex window handling
    std::string fsText = "FULLSCREEN: " + std::string(s.fullscreen ? "ON" : "OFF");
    elements.push_back({cx - btnW/2, startY, btnW, btnH, fsText, false, [](){}, false, nullptr, nullptr, &s.fullscreen});
    startY += btnH + gap;

    // Back
    elements.push_back({cx - btnW/2, startY + 20, btnW, btnH, "BACK", false, [this]() { 
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
    static std::string seedInput = "12345";

    UIElement nameField = {centerX - btnW/2, centerY - 100, btnW, btnH, "NAME: " + nameInput, false, nullptr, false, nullptr, nullptr, nullptr, 0.0f, 0.0f, true, &nameInput};
    elements.push_back(nameField);

    UIElement seedField = {centerX - btnW/2, centerY - 100 + btnH + gap, btnW, btnH, "SEED: " + seedInput, false, nullptr, false, nullptr, nullptr, nullptr, 0.0f, 0.0f, true, &seedInput};
    elements.push_back(seedField);

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*2 + 20, btnW, btnH, "CREATE WORLD", false, [this]() { 
        int seed = 0;
        try { seed = std::stoi(*elements[1].textRef); } catch(...) { seed = 12345; }
        if (onNewGame) onNewGame(*elements[0].textRef, seed);
        setMenuState(MenuState::NONE);
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*3 + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::update(float /*deltaTime*/, double mouseX, double mouseY, bool mousePressed) {
    if (!isMenuOpen()) {
        lastMousePressed = mousePressed;
        return;
    }

    std::function<void()> pendingClick = nullptr;

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
                } else if (el.onClick) {
                    // Only click on rising edge (first press)
                    if (!lastMousePressed) {
                        if (el.boolValueRef) {
                            *el.boolValueRef = !(*el.boolValueRef);
                            // Update text for toggle
                            size_t colonPos = el.text.find(":");
                            if (colonPos != std::string::npos) {
                                std::string prefix = el.text.substr(0, colonPos + 1);
                                el.text = prefix + (*el.boolValueRef ? " ON" : " OFF");
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

    if (pendingClick) {
        pendingClick();
    }
    
    lastMousePressed = mousePressed;
}

void UIManager::render() {
    if (!isMenuOpen() && !showDebug) return;

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
    }

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
        }
        
        cursorX += 6 * scale;
    }
}

void UIManager::updateDebugInfo(float fps, const std::string& blockName) {
    currentFPS = fps;
    currentBlockName = blockName;
}
