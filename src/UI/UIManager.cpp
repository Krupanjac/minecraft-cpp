#include "UIManager.h"
#include "../World/WorldSerializer.h"
#include "../World/WorldGenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <random>
#include <climits>
#include <stb_image.h>

UIManager::UIManager() : vao(0), vbo(0), texturedVao(0), texturedVbo(0), width(1280), height(720), showDebug(false), currentFPS(0.0f), currentMenuState(MenuState::MAIN_MENU) {}

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
    
    // Textured UI shader for world preview thumbnails
    const char* texVertSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        uniform mat4 uProjection;
        uniform mat4 uModel;
        void main() {
            gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* texFragSrc = R"(
        #version 450 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        void main() {
            FragColor = texture(uTexture, TexCoord);
        }
    )";
    
    texturedShader.loadFromSource(texVertSrc, texFragSrc);

    // Setup quad VAO (no texture coords)
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
    
    // Setup textured quad VAO (with texture coords)
    float texturedVertices[] = {
        // pos        // tex
        0.0f, 1.0f,   0.0f, 0.0f,
        1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 0.0f,   0.0f, 1.0f,
        
        0.0f, 1.0f,   0.0f, 0.0f,
        1.0f, 1.0f,   1.0f, 0.0f,
        1.0f, 0.0f,   1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &texturedVao);
    glGenBuffers(1, &texturedVbo);
    
    glBindVertexArray(texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, texturedVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texturedVertices), texturedVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
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
        case MenuState::PLAYER_SETTINGS: setupPlayerSettingsMenu(); break;
        case MenuState::CONTROLS: setupControlsMenu(); break;
        case MenuState::LOAD_GAME: setupLoadGameMenu(); break;
        case MenuState::NEW_GAME: setupNewGameMenu(); break;
        case MenuState::INVENTORY: setupInventoryMenu(); break;
        case MenuState::MAP: setupMapMenu(); break;
        case MenuState::MULTIPLAYER: setupMultiplayerMenu(); break;
        case MenuState::HOST_GAME: setupHostGameMenu(); break;
        case MenuState::JOIN_GAME: setupJoinGameMenu(); break;
        case MenuState::ABOUT: setupAboutMenu(); break;
        case MenuState::CHAT: break; // Chat doesn't use elements system
        case MenuState::NONE: break;
    }
}

void UIManager::handleCharInput(unsigned int codepoint) {
    // Handle chat input
    if (currentMenuState == MenuState::CHAT) {
        if (codepoint >= 32 && codepoint <= 126) {
            chatInput += (char)codepoint;
        }
        return;
    }
    
    if (!isMenuOpen()) return;
    
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (codepoint >= 32 && codepoint <= 126) {
                *el.textRef += (char)codepoint;
            }
            // Update display text - sync with textRef for all input fields
            el.text = *el.textRef;
        }
    }
}

void UIManager::handleKeyInput(int key) {
    // Handle chat keys
    if (currentMenuState == MenuState::CHAT) {
        if (key == 259) { // GLFW_KEY_BACKSPACE
            if (!chatInput.empty()) {
                chatInput.pop_back();
            }
        } else if (key == 257) { // GLFW_KEY_ENTER
            if (!chatInput.empty() && onSendChat) {
                onSendChat(chatInput);
                chatInput.clear();
            }
            closeChat();
        } else if (key == 256) { // GLFW_KEY_ESCAPE
            closeChat();
        }
        return;
    }
    
    if (!isMenuOpen()) return;
    
    // Handle backspace for text input in any menu
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (key == 259) { // GLFW_KEY_BACKSPACE
                if (!el.textRef->empty()) {
                    el.textRef->pop_back();
                    el.text = *el.textRef;
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
    float centerY = height / 2.0f + 30.0f; // Shifted down to make room for title
    float btnW = 280.0f;  // Wider buttons
    float btnH = 50.0f;   // Taller buttons
    float gap = 12.0f;    // More spacing

    elements.push_back({centerX - btnW/2, centerY - 125, btnW, btnH, "Singleplayer", false, [this]() { 
        setMenuState(MenuState::NEW_GAME); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 125 + btnH + gap, btnW, btnH, "Load World", false, [this]() { 
        setMenuState(MenuState::LOAD_GAME); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 125 + (btnH + gap)*2, btnW, btnH, "Multiplayer", false, [this]() { 
        setMenuState(MenuState::MULTIPLAYER); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 125 + (btnH + gap)*3, btnW, btnH, "Options", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 125 + (btnH + gap)*4, btnW, btnH, "About", false, [this]() { 
        setMenuState(MenuState::ABOUT); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 125 + (btnH + gap)*5, btnW, btnH, "Quit Game", false, [this]() { 
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

    elements.push_back({centerX - btnW/2, centerY - 75, btnW, btnH, "RESUME", false, [this]() { 
        setMenuState(MenuState::NONE); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 75 + btnH + gap, btnW, btnH, "SAVE GAME", false, [this]() { 
        if (onSave) onSave();
    }});

    elements.push_back({centerX - btnW/2, centerY - 75 + (btnH + gap)*2, btnW, btnH, "SETTINGS", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    if (isOnline) {
        elements.push_back({centerX - btnW/2, centerY - 75 + (btnH + gap)*3, btnW, btnH, "DISCONNECT", false, [this]() { 
            if (onDisconnectGame) onDisconnectGame();
            worldLoaded = false;
            if (onReturnToMainMenu) onReturnToMainMenu();
            setMenuState(MenuState::MAIN_MENU); 
        }});
    } else {
        elements.push_back({centerX - btnW/2, centerY - 75 + (btnH + gap)*3, btnW, btnH, "MAIN MENU", false, [this]() { 
            if (onSave) onSave(); // Auto save on exit to menu
            worldLoaded = false;
            if (onReturnToMainMenu) onReturnToMainMenu();
            setMenuState(MenuState::MAIN_MENU); 
        }});
    }
}

void UIManager::setupSettingsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 40.0f;
    float gap = 10.0f;
    float startY = cy - 120.0f;

    elements.push_back({cx - btnW/2, startY, btnW, btnH, "VIDEO SETTINGS", false, [this]() { 
        setMenuState(MenuState::VIDEO_SETTINGS); 
    }});
    startY += btnH + gap;

    // Player Settings (nickname + model)
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "PLAYER SETTINGS", false, [this]() {
         setMenuState(MenuState::PLAYER_SETTINGS);
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
    
    auto& s = Settings::instance();
    
    // Grid layout - 2 columns
    float colWidth = 280.0f;
    float colGap = 40.0f;
    float rowHeight = 35.0f;
    float rowGap = 8.0f;
    float startY = 80.0f;
    float leftCol = width / 2.0f - colWidth - colGap / 2;
    float rightCol = width / 2.0f + colGap / 2;
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 100;
    title.y = 30;
    title.w = 200;
    title.h = 30;
    title.text = "Video Settings";
    title.isLabel = true;
    elements.push_back(title);
    
    // === LEFT COLUMN - Performance ===
    UIElement perfHeader;
    perfHeader.x = leftCol;
    perfHeader.y = startY;
    perfHeader.w = colWidth;
    perfHeader.h = 25;
    perfHeader.text = "-- Performance --";
    perfHeader.isLabel = true;
    perfHeader.isHeader = true;
    elements.push_back(perfHeader);
    
    float y = startY + 35;
    
    // Render Distance
    UIElement rd;
    rd.x = leftCol; rd.y = y; rd.w = colWidth; rd.h = rowHeight;
    rd.text = "Render Distance: " + std::to_string(s.renderDistance);
    rd.isSlider = true;
    rd.intValueRef = &s.renderDistance;
    rd.minVal = 2.0f; rd.maxVal = 32.0f;
    rd.tooltip = "How far chunks are rendered (2-32)";
    elements.push_back(rd);
    y += rowHeight + rowGap;
    
    // Shadow Distance
    UIElement sd;
    sd.x = leftCol; sd.y = y; sd.w = colWidth; sd.h = rowHeight;
    sd.text = "Shadow Distance: " + std::to_string((int)s.shadowDistance);
    sd.isSlider = true;
    sd.valueRef = &s.shadowDistance;
    sd.minVal = 50.0f; sd.maxVal = 300.0f;
    sd.tooltip = "Max distance for shadow rendering";
    elements.push_back(sd);
    y += rowHeight + rowGap;
    
    // VSync
    UIElement vs;
    vs.x = leftCol; vs.y = y; vs.w = colWidth; vs.h = rowHeight;
    vs.text = "VSync: " + std::string(s.vsync ? "ON" : "OFF");
    vs.boolValueRef = &s.vsync;
    vs.onClick = [](){};
    vs.tooltip = "Sync framerate to monitor refresh rate";
    elements.push_back(vs);
    y += rowHeight + rowGap;
    
    // Window Mode
    UIElement wm;
    wm.x = leftCol; wm.y = y; wm.w = colWidth; wm.h = rowHeight;
    std::string wmText = "Window: ";
    if (s.fullscreen == 0) wmText += "Windowed";
    else if (s.fullscreen == 1) wmText += "Fullscreen";
    else wmText += "Borderless";
    wm.text = wmText;
    wm.intValueRef = &s.fullscreen;
    wm.onClick = [](){};
    wm.minVal = 0.0f; wm.maxVal = 2.0f;
    wm.tooltip = "Click to cycle: Windowed/Fullscreen/Borderless";
    elements.push_back(wm);
    y += rowHeight + rowGap;
    
    // === LEFT COLUMN - Effects ===
    y += 15;
    UIElement fxHeader;
    fxHeader.x = leftCol;
    fxHeader.y = y;
    fxHeader.w = colWidth;
    fxHeader.h = 25;
    fxHeader.text = "-- Effects --";
    fxHeader.isLabel = true;
    fxHeader.isHeader = true;
    elements.push_back(fxHeader);
    y += 35;
    
    // Shadows
    UIElement sh;
    sh.x = leftCol; sh.y = y; sh.w = colWidth; sh.h = rowHeight;
    sh.text = "Shadows: " + std::string(s.enableShadows ? "ON" : "OFF");
    sh.boolValueRef = &s.enableShadows;
    sh.onClick = [](){};
    sh.tooltip = "Enable dynamic shadows from sun/moon";
    elements.push_back(sh);
    y += rowHeight + rowGap;
    
    // SSAO
    UIElement ssao;
    ssao.x = leftCol; ssao.y = y; ssao.w = colWidth; ssao.h = rowHeight;
    ssao.text = "SSAO: " + std::string(s.enableSSAO ? "ON" : "OFF");
    ssao.boolValueRef = &s.enableSSAO;
    ssao.onClick = [](){};
    ssao.tooltip = "Screen-space ambient occlusion";
    elements.push_back(ssao);
    y += rowHeight + rowGap;
    
    // Volumetrics
    UIElement vol;
    vol.x = leftCol; vol.y = y; vol.w = colWidth; vol.h = rowHeight;
    vol.text = "Volumetrics: " + std::string(s.enableVolumetrics ? "ON" : "OFF");
    vol.boolValueRef = &s.enableVolumetrics;
    vol.onClick = [](){};
    vol.tooltip = "Volumetric lighting (god rays)";
    elements.push_back(vol);
    y += rowHeight + rowGap;
    
    // TAA
    UIElement taa;
    taa.x = leftCol; taa.y = y; taa.w = colWidth; taa.h = rowHeight;
    taa.text = "TAA: " + std::string(s.enableTAA ? "ON" : "OFF");
    taa.boolValueRef = &s.enableTAA;
    taa.onClick = [](){};
    taa.tooltip = "Temporal anti-aliasing (may cause jitter)";
    elements.push_back(taa);
    
    // === RIGHT COLUMN - Visual ===
    y = startY;
    UIElement visHeader;
    visHeader.x = rightCol;
    visHeader.y = y;
    visHeader.w = colWidth;
    visHeader.h = 25;
    visHeader.text = "-- Visual --";
    visHeader.isLabel = true;
    visHeader.isHeader = true;
    elements.push_back(visHeader);
    y += 35;
    
    // FOV
    UIElement fov;
    fov.x = rightCol; fov.y = y; fov.w = colWidth; fov.h = rowHeight;
    fov.text = "FOV: " + std::to_string((int)s.fov);
    fov.isSlider = true;
    fov.valueRef = &s.fov;
    fov.minVal = 30.0f; fov.maxVal = 110.0f;
    fov.tooltip = "Field of view (30-110 degrees)";
    elements.push_back(fov);
    y += rowHeight + rowGap;
    
    // AO Strength
    UIElement ao;
    ao.x = rightCol; ao.y = y; ao.w = colWidth; ao.h = rowHeight;
    ao.text = "AO Strength: " + std::to_string(s.aoStrength).substr(0, 3);
    ao.isSlider = true;
    ao.valueRef = &s.aoStrength;
    ao.minVal = 0.0f; ao.maxVal = 2.0f;
    ao.tooltip = "Ambient occlusion intensity";
    elements.push_back(ao);
    y += rowHeight + rowGap;
    
    // Gamma
    UIElement gm;
    gm.x = rightCol; gm.y = y; gm.w = colWidth; gm.h = rowHeight;
    gm.text = "Gamma: " + std::to_string(s.gamma).substr(0, 3);
    gm.isSlider = true;
    gm.valueRef = &s.gamma;
    gm.minVal = 1.0f; gm.maxVal = 3.0f;
    gm.tooltip = "Display gamma correction";
    elements.push_back(gm);
    y += rowHeight + rowGap;
    
    // Brightness
    UIElement br;
    br.x = rightCol; br.y = y; br.w = colWidth; br.h = rowHeight;
    br.text = "Brightness: " + std::to_string(s.exposure).substr(0, 3);
    br.isSlider = true;
    br.valueRef = &s.exposure;
    br.minVal = 0.1f; br.maxVal = 5.0f;
    br.tooltip = "Overall scene brightness";
    elements.push_back(br);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - Celestial ===
    y += 15;
    UIElement celHeader;
    celHeader.x = rightCol;
    celHeader.y = y;
    celHeader.w = colWidth;
    celHeader.h = 25;
    celHeader.text = "-- Celestial --";
    celHeader.isLabel = true;
    celHeader.isHeader = true;
    elements.push_back(celHeader);
    y += 35;
    
    // Sun Size
    UIElement ss;
    ss.x = rightCol; ss.y = y; ss.w = colWidth; ss.h = rowHeight;
    ss.text = "Sun Size: " + std::to_string(s.sunSize).substr(0, 3);
    ss.isSlider = true;
    ss.valueRef = &s.sunSize;
    ss.minVal = 0.5f; ss.maxVal = 10.0f;
    ss.tooltip = "Size of the sun in the sky";
    elements.push_back(ss);
    y += rowHeight + rowGap;
    
    // Moon Size
    UIElement ms;
    ms.x = rightCol; ms.y = y; ms.w = colWidth; ms.h = rowHeight;
    ms.text = "Moon Size: " + std::to_string(s.moonSize).substr(0, 3);
    ms.isSlider = true;
    ms.valueRef = &s.moonSize;
    ms.minVal = 0.5f; ms.maxVal = 10.0f;
    ms.tooltip = "Size of the moon in the sky";
    elements.push_back(ms);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - UI ===
    y += 15;
    UIElement uiHeader;
    uiHeader.x = rightCol;
    uiHeader.y = y;
    uiHeader.w = colWidth;
    uiHeader.h = 25;
    uiHeader.text = "-- Interface --";
    uiHeader.isLabel = true;
    uiHeader.isHeader = true;
    elements.push_back(uiHeader);
    y += 35;
    
    // Tooltips
    UIElement tt;
    tt.x = rightCol; tt.y = y; tt.w = colWidth; tt.h = rowHeight;
    tt.text = "Tooltips: " + std::string(s.enableTooltips ? "ON" : "OFF");
    tt.boolValueRef = &s.enableTooltips;
    tt.onClick = [](){};
    tt.tooltip = "Show helpful tooltips on hover";
    elements.push_back(tt);
    
    // Back button centered at bottom
    UIElement back;
    back.x = width / 2.0f - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::SETTINGS); };
    elements.push_back(back);
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
    
    auto& k = Settings::instance().keys;
    
    // Grid layout - 2 columns
    float colWidth = 240.0f;
    float colGap = 60.0f;
    float rowHeight = 38.0f;
    float rowGap = 10.0f;
    float startY = 80.0f;
    float leftCol = width / 2.0f - colWidth - colGap / 2;
    float rightCol = width / 2.0f + colGap / 2;
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 80;
    title.y = 30;
    title.w = 160;
    title.h = 30;
    title.text = "Controls";
    title.isLabel = true;
    elements.push_back(title);
    
    // === LEFT COLUMN - Movement ===
    UIElement moveHeader;
    moveHeader.x = leftCol;
    moveHeader.y = startY;
    moveHeader.w = colWidth;
    moveHeader.h = 25;
    moveHeader.text = "-- Movement --";
    moveHeader.isLabel = true;
    moveHeader.isHeader = true;
    elements.push_back(moveHeader);
    
    float y = startY + 35;
    
    auto addKeyBtn = [&](float x, float& yPos, const std::string& label, int* keyRef, const std::string& tip) {
        UIElement el;
        el.x = x;
        el.y = yPos;
        el.w = colWidth;
        el.h = rowHeight;
        el.text = label + ": " + getKeyName(*keyRef);
        el.isKeybind = true;
        el.keyBindRef = keyRef;
        el.tooltip = tip;
        elements.push_back(el);
        yPos += rowHeight + rowGap;
    };
    
    addKeyBtn(leftCol, y, "Forward", &k.forward, "Move forward");
    addKeyBtn(leftCol, y, "Backward", &k.backward, "Move backward");
    addKeyBtn(leftCol, y, "Strafe Left", &k.left, "Move left");
    addKeyBtn(leftCol, y, "Strafe Right", &k.right, "Move right");
    addKeyBtn(leftCol, y, "Jump", &k.jump, "Jump / Fly up");
    addKeyBtn(leftCol, y, "Sprint", &k.sprint, "Hold to run faster");
    addKeyBtn(leftCol, y, "Sneak", &k.sneak, "Sneak / Fly down");
    
    // === RIGHT COLUMN - Actions ===
    y = startY;
    UIElement actHeader;
    actHeader.x = rightCol;
    actHeader.y = y;
    actHeader.w = colWidth;
    actHeader.h = 25;
    actHeader.text = "-- Actions --";
    actHeader.isLabel = true;
    actHeader.isHeader = true;
    elements.push_back(actHeader);
    y += 35;
    
    addKeyBtn(rightCol, y, "Inventory", &k.inventory, "Open inventory (E)");
    
    // Info labels
    y += 20;
    UIElement info1;
    info1.x = rightCol; info1.y = y; info1.w = colWidth; info1.h = 25;
    info1.text = "Mouse: Look around";
    info1.isLabel = true;
    elements.push_back(info1);
    y += 30;
    
    UIElement info2;
    info2.x = rightCol; info2.y = y; info2.w = colWidth; info2.h = 25;
    info2.text = "LMB: Break block";
    info2.isLabel = true;
    elements.push_back(info2);
    y += 30;
    
    UIElement info3;
    info3.x = rightCol; info3.y = y; info3.w = colWidth; info3.h = 25;
    info3.text = "RMB: Place block";
    info3.isLabel = true;
    elements.push_back(info3);
    y += 30;
    
    UIElement info4;
    info4.x = rightCol; info4.y = y; info4.w = colWidth; info4.h = 25;
    info4.text = "1-9: Select hotbar";
    info4.isLabel = true;
    elements.push_back(info4);
    y += 30;
    
    UIElement info5;
    info5.x = rightCol; info5.y = y; info5.w = colWidth; info5.h = 25;
    info5.text = "Scroll: Cycle hotbar";
    info5.isLabel = true;
    elements.push_back(info5);
    y += 30;
    
    UIElement info6;
    info6.x = rightCol; info6.y = y; info6.w = colWidth; info6.h = 25;
    info6.text = "F1: Toggle debug";
    info6.isLabel = true;
    elements.push_back(info6);
    y += 30;
    
    UIElement info7;
    info7.x = rightCol; info7.y = y; info7.w = colWidth; info7.h = 25;
    info7.text = "M: Open map";
    info7.isLabel = true;
    elements.push_back(info7);
    
    // Instruction
    UIElement inst;
    inst.x = width / 2.0f - 150;
    inst.y = height - 130;
    inst.w = 300;
    inst.h = 25;
    inst.text = "Click a key to rebind";
    inst.isLabel = true;
    inst.customColor = glm::vec4(0.7f, 0.7f, 0.5f, 1.0f);
    elements.push_back(inst);
    
    // Back button
    UIElement back;
    back.x = width / 2.0f - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::SETTINGS); };
    elements.push_back(back);
}

void UIManager::setupLoadGameMenu() {
    elements.clear();
    
    // Clear old preview textures and reload them
    clearWorldPreviewTextures();
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 100;
    title.y = 40;
    title.w = 200;
    title.h = 35;
    title.text = "Load World";
    title.isLabel = true;
    elements.push_back(title);
    
    std::vector<std::string> worlds = WorldSerializer::getAvailableWorlds();
    
    // World cards layout - larger to accommodate thumbnail
    float cardW = 400.0f;
    float cardH = 80.0f;
    float cardGap = 15.0f;
    float startY = 100.0f;
    float centerX = width / 2.0f;
    float thumbW = 100.0f;  // Thumbnail width
    float thumbH = 56.0f;   // Thumbnail height (16:9 aspect)
    float thumbPadding = 12.0f;
    
    // Scrollable area hint if many worlds
    if (worlds.empty()) {
        UIElement noWorlds;
        noWorlds.x = centerX - 150;
        noWorlds.y = height / 2.0f - 30;
        noWorlds.w = 300;
        noWorlds.h = 30;
        noWorlds.text = "No saved worlds found";
        noWorlds.isLabel = true;
        noWorlds.customColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        elements.push_back(noWorlds);
    } else {
        for (size_t i = 0; i < worlds.size() && i < 6; i++) { // Max 6 visible
            std::string wName = worlds[i];
            
            // Try to load preview texture
            GLuint previewTex = loadWorldPreviewTexture(wName);
            
            // World card (acts like a big button)
            UIElement card;
            card.x = centerX - cardW / 2;
            card.y = startY + i * (cardH + cardGap);
            card.w = cardW;
            card.h = cardH;
            card.text = wName;
            card.isCard = true;
            card.tooltip = "Click to load \"" + wName + "\"";
            
            // Set thumbnail info if preview exists
            if (previewTex != 0) {
                card.textureId = previewTex;
                card.thumbnailX = card.x + thumbPadding;
                card.thumbnailY = card.y + (cardH - thumbH) / 2.0f;
                card.thumbnailW = thumbW;
                card.thumbnailH = thumbH;
            }
            
            card.onClick = [this, wName]() { 
                if (onLoadGame) onLoadGame(wName);
                setMenuState(MenuState::NONE);
            };
            elements.push_back(card);
        }
        
        if (worlds.size() > 6) {
            UIElement more;
            more.x = centerX - 100;
            more.y = startY + 6 * (cardH + cardGap);
            more.w = 200;
            more.h = 25;
            more.text = "+" + std::to_string(worlds.size() - 6) + " more worlds...";
            more.isLabel = true;
            more.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            elements.push_back(more);
        }
    }
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupNewGameMenu() {
    elements.clear();
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 120;
    title.y = 50;
    title.w = 240;
    title.h = 35;
    title.text = "Create New World";
    title.isLabel = true;
    elements.push_back(title);
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f - 40;
    float inputW = 320.0f;
    float inputH = 45.0f;
    float gap = 20.0f;
    
    // Seed constraints
    constexpr long MAX_SEED = 999999999L;
    constexpr long MIN_SEED = 1L;

    // Static inputs to preserve values
    static std::string nameInput = "New World";
    static std::string seedInput = "";
    if (seedInput.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<long> dis(MIN_SEED, MAX_SEED);
        seedInput = std::to_string(dis(gen));
    }
    
    // World Name Label
    UIElement nameLabel;
    nameLabel.x = centerX - inputW / 2;
    nameLabel.y = centerY - 80;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "World Name:";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);

    // World Name Input
    UIElement nameField;
    nameField.x = centerX - inputW / 2;
    nameField.y = centerY - 55;
    nameField.w = inputW;
    nameField.h = inputH;
    nameField.text = nameInput;
    nameField.isInput = true;
    nameField.textRef = &nameInput;
    nameField.tooltip = "Enter a name for your world";
    elements.push_back(nameField);

    // Seed Label
    UIElement seedLabel;
    seedLabel.x = centerX - inputW / 2;
    seedLabel.y = centerY + 10;
    seedLabel.w = inputW;
    seedLabel.h = 20;
    seedLabel.text = "World Seed:";
    seedLabel.isLabel = true;
    elements.push_back(seedLabel);
    
    // Seed Input
    UIElement seedField;
    seedField.x = centerX - inputW / 2;
    seedField.y = centerY + 35;
    seedField.w = inputW;
    seedField.h = inputH;
    seedField.text = seedInput;
    seedField.isInput = true;
    seedField.textRef = &seedInput;
    seedField.tooltip = "Numeric seed (1 - 999,999,999)";
    elements.push_back(seedField);
    
    // Seed hint
    UIElement seedHint;
    seedHint.x = centerX - inputW / 2;
    seedHint.y = centerY + 85;
    seedHint.w = inputW;
    seedHint.h = 18;
    seedHint.text = "Leave empty or use numbers only";
    seedHint.isLabel = true;
    seedHint.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    elements.push_back(seedHint);

    // Create World Button
    UIElement createBtn;
    createBtn.x = centerX - 120;
    createBtn.y = centerY + 130;
    createBtn.w = 240;
    createBtn.h = 50;
    createBtn.text = "Create World";
    createBtn.tooltip = "Generate and enter a new world";
    createBtn.onClick = [this]() { 
        long seed = 0;
        std::string seedStr = *elements[4].textRef; // seedField is element 4
        
        // Filter non-numeric characters
        std::string cleanSeed;
        for (char c : seedStr) {
            if (c >= '0' && c <= '9') cleanSeed += c;
        }
        
        try { 
            if (!cleanSeed.empty()) {
                seed = std::stol(cleanSeed);
            }
        } catch(...) { 
            seed = 0;
        }
        
        constexpr long MAX_SEED = 999999999L;
        constexpr long MIN_SEED = 1L;
        
        if (seed < MIN_SEED || seed > MAX_SEED) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<long> dis(MIN_SEED, MAX_SEED);
            seed = dis(gen);
        }
        
        if (onNewGame) onNewGame(*elements[2].textRef, seed); // nameField is element 2
        setMenuState(MenuState::NONE);
    };
    elements.push_back(createBtn);

    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
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

void UIManager::update(float deltaTime, double mouseX, double mouseY, bool mousePressed, bool rightMousePressed) {
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
    
    // Map click handling - teleport on left click
    if (currentMenuState == MenuState::MAP && mousePressed && !lastMousePressed && !elements.empty()) {
        const auto& mapEl = elements[0];
        
        // Check if click is on the map
        if (mouseX >= mapEl.x && mouseX <= mapEl.x + mapEl.w &&
            mouseY >= mapEl.y && mouseY <= mapEl.y + mapEl.h) {
            
            // Convert screen coords to map coords
            float relX = (float)(mouseX - mapEl.x) / (float)mapEl.w; // 0 to 1
            float relY = (float)(mouseY - mapEl.y) / (float)mapEl.h; // 0 to 1
            
            // Convert to world coords
            float worldX = mapCenterX + (relX - 0.5f) * mapTextureSize * mapScale;
            float worldZ = mapCenterZ + (relY - 0.5f) * mapTextureSize * mapScale;
            
            // Teleport
            if (onTeleport) {
                onTeleport(worldX, worldZ);
                setMenuState(MenuState::NONE); // Close map after teleport
            }
        }
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
            }
            
            // Special handling for MAP - render the map using colored rectangles
            if (currentMenuState == MenuState::MAP && !elements.empty()) {
                const auto& mapEl = elements[0];
                
                // Draw the map background
                drawRect(mapEl.x, mapEl.y, mapEl.w, mapEl.h, glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
                
                // Draw map pixels as small colored rectangles
                // We sample the heightmap at lower resolution for performance
                if (worldGenerator) {
                    int pixelRes = 128; // Sample resolution
                    float pixelSize = mapEl.w / pixelRes;
                    
                    for (int py = 0; py < pixelRes; py++) {
                        for (int px = 0; px < pixelRes; px++) {
                            // Convert pixel coords to world coords
                            float worldX = mapCenterX + (px - pixelRes / 2) * mapScale * (mapTextureSize / pixelRes);
                            float worldZ = mapCenterZ + (py - pixelRes / 2) * mapScale * (mapTextureSize / pixelRes);
                            
                            // Get height and biome
                            float h = worldGenerator->getHeight(worldX, worldZ);
                            BiomeType biome = worldGenerator->getBiome(worldX, worldZ);
                            
                            // Determine color
                            glm::vec4 color;
                            
                            if (h < 32) { // SEA_LEVEL
                                float depth = (32 - h) / 32.0f;
                                color = glm::vec4(0.08f + 0.15f * (1.0f - depth), 
                                                  0.3f + 0.3f * (1.0f - depth), 
                                                  0.7f + 0.2f * (1.0f - depth), 1.0f);
                            } else {
                                switch (biome) {
                                    case BiomeType::OCEAN:
                                        color = glm::vec4(0.15f, 0.4f, 0.8f, 1.0f);
                                        break;
                                    case BiomeType::PLAINS:
                                        color = glm::vec4(0.47f, 0.7f, 0.3f, 1.0f);
                                        break;
                                    case BiomeType::DESERT:
                                        color = glm::vec4(0.86f, 0.78f, 0.55f, 1.0f);
                                        break;
                                    case BiomeType::FOREST:
                                        color = glm::vec4(0.2f, 0.5f, 0.2f, 1.0f);
                                        break;
                                    case BiomeType::MOUNTAINS:
                                        if (h > 120) {
                                            color = glm::vec4(0.94f, 0.96f, 0.98f, 1.0f); // Snow
                                        } else {
                                            float t = (h - 50.0f) / 70.0f;
                                            color = glm::vec4(0.4f + 0.3f * t, 0.4f + 0.3f * t, 0.43f + 0.27f * t, 1.0f);
                                        }
                                        break;
                                    case BiomeType::SNOWY_TUNDRA:
                                        color = glm::vec4(0.86f, 0.9f, 0.94f, 1.0f);
                                        break;
                                    default:
                                        color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                                        break;
                                }
                            }
                            
                            float rx = mapEl.x + px * pixelSize;
                            float ry = mapEl.y + py * pixelSize;
                            drawRect(rx, ry, pixelSize + 1, pixelSize + 1, color);
                        }
                    }
                    
                    // Draw player marker (red dot at center)
                    float markerSize = 8.0f;
                    float markerX = mapEl.x + mapEl.w / 2 - markerSize / 2;
                    float markerY = mapEl.y + mapEl.h / 2 - markerSize / 2;
                    drawRect(markerX, markerY, markerSize, markerSize, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
                    
                    // Draw border
                    drawRect(markerX - 1, markerY - 1, markerSize + 2, 2, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Top
                    drawRect(markerX - 1, markerY + markerSize - 1, markerSize + 2, 2, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Bottom
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

GLuint UIManager::loadWorldPreviewTexture(const std::string& worldName) {
    // Check cache first
    auto it = worldPreviewTextures.find(worldName);
    if (it != worldPreviewTextures.end()) {
        return it->second;
    }
    
    // Check if preview file exists
    if (!WorldSerializer::hasScreenshot(worldName)) {
        return 0;
    }
    
    std::string path = WorldSerializer::getScreenshotPath(worldName);
    
    int imgWidth, imgHeight, channels;
    unsigned char* data = stbi_load(path.c_str(), &imgWidth, &imgHeight, &channels, 3);
    
    if (!data) {
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imgWidth, imgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    
    // Cache the texture
    worldPreviewTextures[worldName] = texture;
    
    return texture;
}

void UIManager::clearWorldPreviewTextures() {
    for (auto& pair : worldPreviewTextures) {
        if (pair.second != 0) {
            glDeleteTextures(1, &pair.second);
        }
    }
    worldPreviewTextures.clear();
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

void UIManager::updateDebugInfo(float fps, const std::string& blockName, const glm::vec3& playerPos, const glm::vec3& playerVel, float taaMotion, float taaHistoryWeight) {
    currentFPS = fps;
    currentBlockName = blockName;
    currentPlayerPos = playerPos;
    currentPlayerVel = playerVel;
    // TAA debug metrics
    lastTaaMotion = taaMotion;
    lastTaaHistoryWeight = taaHistoryWeight;
}

void UIManager::generateMapTexture() {
    if (!worldGenerator) return;
    
    // Create map texture if not exists
    if (mapTexture == 0) {
        glGenTextures(1, &mapTexture);
    }
    
    // Generate pixel data
    std::vector<unsigned char> pixels(mapTextureSize * mapTextureSize * 3);
    
    for (int py = 0; py < mapTextureSize; py++) {
        for (int px = 0; px < mapTextureSize; px++) {
            // Convert pixel coords to world coords
            float worldX = mapCenterX + (px - mapTextureSize / 2) * mapScale;
            float worldZ = mapCenterZ + (py - mapTextureSize / 2) * mapScale;
            
            // Get height and biome
            float terrainHeight = worldGenerator->getHeight(worldX, worldZ);
            BiomeType biome = worldGenerator->getBiome(worldX, worldZ);
            
            // Determine color based on biome and height
            unsigned char r, g, b;
            
            // Sea level check
            if (terrainHeight < 32) { // SEA_LEVEL
                // Water - deeper = darker blue
                float depth = (32 - terrainHeight) / 32.0f;
                r = static_cast<unsigned char>(20 + 40 * (1.0f - depth));
                g = static_cast<unsigned char>(80 + 80 * (1.0f - depth));
                b = static_cast<unsigned char>(180 + 50 * (1.0f - depth));
            } else {
                // Land - color by biome
                switch (biome) {
                    case BiomeType::OCEAN:
                        r = 40; g = 100; b = 200;
                        break;
                    case BiomeType::PLAINS:
                        r = 120; g = 180; b = 80;
                        // Height shading
                        r = static_cast<unsigned char>(r * (0.7f + 0.3f * std::min(terrainHeight / 100.0f, 1.0f)));
                        g = static_cast<unsigned char>(g * (0.7f + 0.3f * std::min(terrainHeight / 100.0f, 1.0f)));
                        break;
                    case BiomeType::DESERT:
                        r = 220; g = 200; b = 140;
                        break;
                    case BiomeType::FOREST:
                        r = 50; g = 130; b = 50;
                        // Darker for dense forest
                        r = static_cast<unsigned char>(r * (0.8f + 0.2f * std::min(terrainHeight / 80.0f, 1.0f)));
                        g = static_cast<unsigned char>(g * (0.8f + 0.2f * std::min(terrainHeight / 80.0f, 1.0f)));
                        break;
                    case BiomeType::MOUNTAINS:
                        // Gray stone, whiter at peaks
                        if (terrainHeight > 120) {
                            // Snow caps
                            r = 240; g = 245; b = 250;
                        } else {
                            float t = (terrainHeight - 50.0f) / 70.0f;
                            r = static_cast<unsigned char>(100 + 80 * t);
                            g = static_cast<unsigned char>(100 + 80 * t);
                            b = static_cast<unsigned char>(110 + 70 * t);
                        }
                        break;
                    case BiomeType::SNOWY_TUNDRA:
                        r = 220; g = 230; b = 240;
                        break;
                    default:
                        r = 128; g = 128; b = 128;
                        break;
                }
            }
            
            int idx = (py * mapTextureSize + px) * 3;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
        }
    }
    
    // Upload texture
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mapTextureSize, mapTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void UIManager::setupMapMenu() {
    elements.clear();
    
    // Center the player position for the map
    mapCenterX = currentPlayerPos.x;
    mapCenterZ = currentPlayerPos.z;
    
    // Generate the map texture
    generateMapTexture();
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    
    // Map display size (as large as possible while fitting on screen)
    float mapDisplaySize = std::min(width, height) * 0.85f;
    float mapX = centerX - mapDisplaySize / 2;
    float mapY = centerY - mapDisplaySize / 2;
    
    // Map element - clicking on it will teleport
    UIElement mapElement;
    mapElement.x = mapX;
    mapElement.y = mapY;
    mapElement.w = mapDisplaySize;
    mapElement.h = mapDisplaySize;
    mapElement.text = ""; // No text, we'll render the texture
    mapElement.isHovered = false;
    mapElement.onClick = nullptr; // Handle click specially in update
    elements.push_back(mapElement);
    
    // Close button
    float btnW = 200.0f;
    float btnH = 40.0f;
    elements.push_back({
        centerX - btnW / 2, mapY + mapDisplaySize + 10, btnW, btnH, 
        "CLOSE (M)", false, [this]() {
            setMenuState(MenuState::NONE);
        }
    });
    
    // Zoom controls
    elements.push_back({
        mapX - 50, centerY - 20, 40, 40, "+", false, [this]() {
            mapScale = std::max(1.0f, mapScale / 2.0f);
            generateMapTexture();
        }
    });
    elements.push_back({
        mapX - 50, centerY + 30, 40, 40, "-", false, [this]() {
            mapScale = std::min(64.0f, mapScale * 2.0f);
            generateMapTexture();
        }
    });
    
    // Info labels
    UIElement scaleLabel = {mapX, mapY - 25, 300, 20, "Scale: " + std::to_string(static_cast<int>(mapScale)) + " blocks/pixel", false, nullptr};
    elements.push_back(scaleLabel);
    
    UIElement coordLabel = {mapX, mapY - 50, 400, 20, "Center: X=" + std::to_string(static_cast<int>(mapCenterX)) + " Z=" + std::to_string(static_cast<int>(mapCenterZ)), false, nullptr};
    elements.push_back(coordLabel);
    
    UIElement helpLabel = {centerX - 150, mapY + mapDisplaySize + 55, 300, 20, "Click on map to teleport", false, nullptr};
    elements.push_back(helpLabel);
}

void UIManager::setupMultiplayerMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Multiplayer";
    title.isLabel = true;
    elements.push_back(title);
    
    // Card-style buttons
    float cardW = 300.0f;
    float cardH = 80.0f;
    float gap = 25.0f;
    float startY = height / 2.0f - cardH - gap/2;
    
    // Host Game Card
    UIElement hostCard;
    hostCard.x = centerX - cardW/2;
    hostCard.y = startY;
    hostCard.w = cardW;
    hostCard.h = cardH;
    hostCard.text = "Host Game";
    hostCard.isCard = true;
    hostCard.tooltip = "Create a server for others to join";
    hostCard.onClick = [this]() { setMenuState(MenuState::HOST_GAME); };
    elements.push_back(hostCard);
    
    // Join Game Card
    UIElement joinCard;
    joinCard.x = centerX - cardW/2;
    joinCard.y = startY + cardH + gap;
    joinCard.w = cardW;
    joinCard.h = cardH;
    joinCard.text = "Join Game";
    joinCard.isCard = true;
    joinCard.tooltip = "Connect to an existing server";
    joinCard.onClick = [this]() { setMenuState(MenuState::JOIN_GAME); };
    elements.push_back(joinCard);

    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupHostGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Host Game";
    title.isLabel = true;
    elements.push_back(title);
    
    float inputW = 320.0f;
    float inputH = 42.0f;
    float gap = 20.0f;
    float startY = height / 2.0f - 100;
    
    // Load last used values from settings
    auto& settings = Settings::instance();
    if (playerName.empty() || playerName == "Player") {
        playerName = settings.lastPlayerName;
    }
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name Label
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name:";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name Input
    UIElement nameInput;
    nameInput.x = centerX - inputW/2;
    nameInput.y = startY + 25;
    nameInput.w = inputW;
    nameInput.h = inputH;
    nameInput.text = playerName;
    nameInput.isInput = true;
    nameInput.textRef = &playerName;
    nameInput.tooltip = "Name shown to other players";
    elements.push_back(nameInput);
    
    // Port Label
    UIElement portLabel;
    portLabel.x = centerX - inputW/2;
    portLabel.y = startY + inputH + gap + 25;
    portLabel.w = inputW;
    portLabel.h = 20;
    portLabel.text = "Server Port:";
    portLabel.isLabel = true;
    elements.push_back(portLabel);
    
    // Port Input
    UIElement portInput;
    portInput.x = centerX - inputW/2;
    portInput.y = startY + inputH + gap + 50;
    portInput.w = inputW;
    portInput.h = inputH;
    portInput.text = serverPort;
    portInput.isInput = true;
    portInput.textRef = &serverPort;
    portInput.tooltip = "Port to host on (default: 25565)";
    elements.push_back(portInput);
    
    // Host button
    UIElement hostBtn;
    hostBtn.x = centerX - 120;
    hostBtn.y = startY + (inputH + gap)*2 + 70;
    hostBtn.w = 240;
    hostBtn.h = 50;
    hostBtn.text = "Start Hosting";
    hostBtn.tooltip = "Create server and wait for players";
    hostBtn.onClick = [this]() {
        if (onHostGame) {
            int port = std::stoi(serverPort.empty() ? "25565" : serverPort);
            Settings::instance().lastPlayerName = playerName;
            Settings::instance().lastServerPort = port;
            onHostGame(playerName, port);
        }
    };
    elements.push_back(hostBtn);
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MULTIPLAYER); };
    elements.push_back(back);
}

void UIManager::setupJoinGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Join Game";
    title.isLabel = true;
    elements.push_back(title);
    
    float inputW = 320.0f;
    float inputH = 42.0f;
    float gap = 18.0f;
    float btnW = 240.0f;
    float btnH = 50.0f;
    float startY = height / 2.0f - 140;
    
    // Load last used values from settings
    auto& settings = Settings::instance();
    if (playerName.empty() || playerName == "Player") {
        playerName = settings.lastPlayerName;
    }
    if (serverAddress.empty() || serverAddress == "localhost") {
        serverAddress = settings.lastServerAddress;
    }
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name Label
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name:";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name Input
    UIElement nameInput;
    nameInput.x = centerX - inputW/2;
    nameInput.y = startY + 25;
    nameInput.w = inputW;
    nameInput.h = inputH;
    nameInput.text = playerName;
    nameInput.isInput = true;
    nameInput.textRef = &playerName;
    nameInput.tooltip = "Name shown to other players";
    elements.push_back(nameInput);
    
    // Server Address Label
    UIElement addrLabel;
    addrLabel.x = centerX - inputW/2;
    addrLabel.y = startY + inputH + gap + 25;
    addrLabel.w = inputW;
    addrLabel.h = 20;
    addrLabel.text = "Server Address:";
    addrLabel.isLabel = true;
    elements.push_back(addrLabel);
    
    // Server Address Input
    UIElement addrInput;
    addrInput.x = centerX - inputW/2;
    addrInput.y = startY + inputH + gap + 50;
    addrInput.w = inputW;
    addrInput.h = inputH;
    addrInput.text = serverAddress;
    addrInput.isInput = true;
    addrInput.textRef = &serverAddress;
    addrInput.tooltip = "IP address or hostname of the server";
    elements.push_back(addrInput);
    
    // Port Label
    UIElement portLabel;
    portLabel.x = centerX - inputW/2;
    portLabel.y = startY + (inputH + gap)*2 + 50;
    portLabel.w = inputW;
    portLabel.h = 20;
    portLabel.text = "Server Port:";
    portLabel.isLabel = true;
    elements.push_back(portLabel);
    
    // Port Input
    UIElement portInput;
    portInput.x = centerX - inputW/2;
    portInput.y = startY + (inputH + gap)*2 + 75;
    portInput.w = inputW;
    portInput.h = inputH;
    portInput.text = serverPort;
    portInput.isInput = true;
    portInput.textRef = &serverPort;
    portInput.tooltip = "Port to connect to (default: 25565)";
    elements.push_back(portInput);
    
    // Status display
    if (!networkStatus.empty()) {
        UIElement statusLabel;
        statusLabel.x = centerX - inputW/2;
        statusLabel.y = startY + (inputH + gap)*3 + 90;
        statusLabel.w = inputW;
        statusLabel.h = 20;
        statusLabel.text = networkStatus;
        statusLabel.isLabel = true;
        statusLabel.customColor = glm::vec4(1.0f, 0.7f, 0.3f, 1.0f);
        elements.push_back(statusLabel);
    }
    
    // Join button
    UIElement joinBtn;
    joinBtn.x = centerX - btnW/2;
    joinBtn.y = startY + (inputH + gap)*3 + 110;
    joinBtn.w = btnW;
    joinBtn.h = btnH;
    joinBtn.text = "JOIN";
    joinBtn.onClick = [this]() {
        if (onJoinGame) {
            int port = std::stoi(serverPort.empty() ? "25565" : serverPort);
            // Save last used values
            Settings::instance().lastPlayerName = playerName;
            Settings::instance().lastServerAddress = serverAddress;
            Settings::instance().lastServerPort = port;
            onJoinGame(playerName, serverAddress, port);
        }
    };
    elements.push_back(joinBtn);
    
    // Back button
    UIElement backBtn;
    backBtn.x = centerX - btnW/2;
    backBtn.y = startY + (inputH + gap)*3 + 110 + btnH + gap;
    backBtn.w = btnW;
    backBtn.h = btnH;
    backBtn.text = "BACK";
    backBtn.onClick = [this]() { setMenuState(MenuState::MULTIPLAYER); };
    elements.push_back(backBtn);
}

void UIManager::setupAboutMenu() {
    elements.clear();
    
    float centerX = width / 2.0f;
    float startY = 80.0f;
    float lineHeight = 35.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 40;
    title.text = "About Bettercraft";
    title.isHeader = true;
    elements.push_back(title);
    startY += 70.0f;
    
    // Project description
    UIElement desc1;
    desc1.x = centerX - 200;
    desc1.y = startY;
    desc1.w = 400;
    desc1.h = 25;
    desc1.text = "A Minecraft clone written in C++";
    desc1.isLabel = true;
    desc1.customColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    elements.push_back(desc1);
    startY += lineHeight;
    
    UIElement desc2;
    desc2.x = centerX - 200;
    desc2.y = startY;
    desc2.w = 400;
    desc2.h = 25;
    desc2.text = "Built with OpenGL 4.5, GLFW, GLM";
    desc2.isLabel = true;
    desc2.customColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    elements.push_back(desc2);
    startY += lineHeight * 2;
    
    // Developer section
    UIElement devHeader;
    devHeader.x = centerX - 100;
    devHeader.y = startY;
    devHeader.w = 200;
    devHeader.h = 30;
    devHeader.text = "Developer";
    devHeader.isHeader = true;
    devHeader.customColor = glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);
    elements.push_back(devHeader);
    startY += 50.0f;
    
    UIElement devName;
    devName.x = centerX - 150;
    devName.y = startY;
    devName.w = 300;
    devName.h = 25;
    devName.text = "Krupanjac";
    devName.isLabel = true;
    devName.customColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    elements.push_back(devName);
    startY += lineHeight * 1.5f;
    
    // Links section
    UIElement linksHeader;
    linksHeader.x = centerX - 100;
    linksHeader.y = startY;
    linksHeader.w = 200;
    linksHeader.h = 30;
    linksHeader.text = "Links";
    linksHeader.isHeader = true;
    linksHeader.customColor = glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);
    elements.push_back(linksHeader);
    startY += 50.0f;
    
    UIElement website;
    website.x = centerX - 150;
    website.y = startY;
    website.w = 300;
    website.h = 25;
    website.text = "Website: krupanjac.dev";
    website.isLabel = true;
    website.customColor = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    elements.push_back(website);
    startY += lineHeight;
    
    UIElement github;
    github.x = centerX - 200;
    github.y = startY;
    github.w = 400;
    github.h = 25;
    github.text = "GitHub: Krupanjac/minecraft-cpp";
    github.isLabel = true;
    github.customColor = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    elements.push_back(github);
    startY += lineHeight * 2;
    
    // Version info
    UIElement version;
    version.x = centerX - 100;
    version.y = startY;
    version.w = 200;
    version.h = 25;
    version.text = "Version 1.0";
    version.isLabel = true;
    version.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    elements.push_back(version);
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupPlayerSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    float centerX = width / 2.0f;
    float startY = 80.0f;
    float lineHeight = 50.0f;
    float btnW = 400.0f;
    float btnH = 40.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 40;
    title.text = "Player Settings";
    title.isHeader = true;
    elements.push_back(title);
    startY += 70.0f;
    
    // Nickname input
    UIElement nicknameLabel;
    nicknameLabel.x = centerX - 200;
    nicknameLabel.y = startY;
    nicknameLabel.w = 150;
    nicknameLabel.h = 30;
    nicknameLabel.text = "Nickname:";
    nicknameLabel.isLabel = true;
    elements.push_back(nicknameLabel);
    
    UIElement nicknameInput;
    nicknameInput.x = centerX - 40;
    nicknameInput.y = startY - 5;
    nicknameInput.w = 240;
    nicknameInput.h = btnH;
    nicknameInput.text = s.playerNickname;
    nicknameInput.isInput = true;
    nicknameInput.textRef = &s.playerNickname;
    nicknameInput.tooltip = "Your in-game name";
    elements.push_back(nicknameInput);
    startY += lineHeight;
    
    // Player Model selection
    UIElement modelLabel;
    modelLabel.x = centerX - 200;
    modelLabel.y = startY;
    modelLabel.w = 150;
    modelLabel.h = 30;
    modelLabel.text = "Player Model:";
    modelLabel.isLabel = true;
    elements.push_back(modelLabel);
    startY += lineHeight * 0.7f;
    
    // Model buttons - cycle through available models
    for (int i = 0; i < Settings::NUM_PLAYER_MODELS; i++) {
        UIElement modelBtn;
        modelBtn.x = centerX - btnW / 2;
        modelBtn.y = startY;
        modelBtn.w = btnW;
        modelBtn.h = btnH;
        
        std::string prefix = (s.playerModelIndex == i) ? "> " : "  ";
        std::string suffix = (s.playerModelIndex == i) ? " <" : "";
        modelBtn.text = prefix + std::string(Settings::PLAYER_MODEL_NAMES[i]) + suffix;
        
        if (s.playerModelIndex == i) {
            modelBtn.customColor = glm::vec4(0.3f, 0.6f, 0.3f, 1.0f);
        }
        
        int modelIndex = i;
        modelBtn.onClick = [this, modelIndex]() {
            Settings::instance().playerModelIndex = modelIndex;
            if (onSettingsChanged) onSettingsChanged();
            setupPlayerSettingsMenu(); // Refresh to show selection
        };
        
        elements.push_back(modelBtn);
        startY += btnH + 5.0f;
    }
    
    startY += 20.0f;
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { 
        Settings::instance().save();
        setMenuState(MenuState::SETTINGS); 
    };
    elements.push_back(back);
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
