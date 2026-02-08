#include "UIManager.h"
#include "../World/WorldSerializer.h"
#include "../World/WorldGenerator.h"
#include "../Core/HardwareInfo.h"
#include "../Audio/AudioManager.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <random>
#include <climits>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#endif

#include <stb_image.h>

void UIManager::setupMainMenu() {
    elements.clear();
    
    // Play menu music
    Audio::AudioManager::instance().playMusic(Audio::SoundType::MUSIC_MENU, true, 2.0f);
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f + 30.0f; // Shifted down to make room for title
    float btnW = 280.0f;  // Wider buttons
    float btnH = 50.0f;   // Taller buttons
    float gap = 12.0f;    // More spacing

    elements.push_back({centerX - btnW/2, centerY - 95, btnW, btnH, "Singleplayer", false, [this]() { 
        setMenuState(MenuState::NEW_GAME); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap), btnW, btnH, "Multiplayer", false, [this]() { 
        setMenuState(MenuState::MULTIPLAYER); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*2, btnW, btnH, "Options", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*3, btnW, btnH, "About", false, [this]() { 
        setMenuState(MenuState::ABOUT); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*4, btnW, btnH, "Quit Game", false, [this]() { 
        if (onExit) onExit(); 
    }});
    
    // Player card in top-right corner
    UIElement playerCard;
    playerCard.x = width - 220.0f;
    playerCard.y = 20.0f;
    playerCard.w = 200.0f;
    playerCard.h = 60.0f;
    playerCard.text = Settings::instance().playerNickname;
    playerCard.isCard = true;
    playerCard.tooltip = "Click to edit player settings";
    playerCard.onClick = [this]() { setMenuState(MenuState::PLAYER_SETTINGS); };
    elements.push_back(playerCard);
}

void UIManager::setupInGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    elements.push_back({centerX - btnW/2, centerY - 100, btnW, btnH, "RESUME", false, [this]() { 
        setMenuState(MenuState::NONE); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + btnH + gap, btnW, btnH, "SAVE GAME", false, [this]() { 
        if (onSave) onSave();
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*2, btnW, btnH, "SETTINGS", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    // Respawn button - kill self to test death screen
    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*3, btnW, btnH, "RESPAWN", false, [this]() { 
        // This triggers death and respawn
        if (onRespawn) onRespawn();
    }});
    
    if (isOnline) {
        elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*4, btnW, btnH, "DISCONNECT", false, [this]() { 
            if (onDisconnectGame) onDisconnectGame();
            worldLoaded = false;
            if (onReturnToMainMenu) onReturnToMainMenu();
            setMenuState(MenuState::MAIN_MENU); 
        }});
    } else {
        elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*4, btnW, btnH, "MAIN MENU", false, [this]() { 
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
    float startY = cy - 150.0f;

    elements.push_back({cx - btnW/2, startY, btnW, btnH, "VIDEO SETTINGS", false, [this]() { 
        setMenuState(MenuState::VIDEO_SETTINGS); 
    }});
    startY += btnH + gap;
    
    // Audio Settings
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "AUDIO SETTINGS", false, [this]() {
        setMenuState(MenuState::AUDIO_SETTINGS);
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
    float startY = 120.0f; // Moved down to accommodate preset selector
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
    
    // === Graphics Preset Selector (prominent at top) ===
    UIElement presetLabel;
    presetLabel.x = width / 2.0f - 200;
    presetLabel.y = 70;
    presetLabel.w = 100;
    presetLabel.h = 30;
    presetLabel.text = "Preset:";
    presetLabel.isLabel = true;
    elements.push_back(presetLabel);
    
    UIElement preset;
    preset.x = width / 2.0f - 90;
    preset.y = 70;
    preset.w = 280;
    preset.h = 30;
    preset.text = Settings::PRESET_NAMES[s.graphicsPreset];
    preset.intValueRef = &s.graphicsPreset;
    preset.onClick = [](){}; // intValueRef handles cycling, applyPreset called in handleClick
    preset.minVal = 0.0f; 
    preset.maxVal = float(Settings::NUM_PRESETS - 1);
    preset.tooltip = "Quick graphics preset - Low/Medium/High or RT variants (click to cycle)";
    elements.push_back(preset);
    
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
    
    // Anti-aliasing method selector
    UIElement aa;
    aa.x = leftCol; aa.y = y; aa.w = colWidth; aa.h = rowHeight;
    aa.text = "AA METHOD: " + std::string(Settings::AA_METHOD_NAMES[s.aaMethod]);
    aa.intValueRef = &s.aaMethod;
    aa.onClick = [](){};  // intValueRef handles cycling
    aa.minVal = 0.0f; aa.maxVal = 2.0f;  // None=0, FXAA=1, TAA=2
    aa.tooltip = "Anti-aliasing method (None/FXAA/TAA)";
    elements.push_back(aa);
    y += rowHeight + rowGap;
    
    // Ray Tracing toggle
    UIElement rt;
    rt.x = leftCol; rt.y = y; rt.w = colWidth; rt.h = rowHeight;
    rt.text = "Ray Tracing: " + std::string(s.enableRayTracing ? "ON" : "OFF");
    rt.boolValueRef = &s.enableRayTracing;
    rt.onClick = [this]() { setupVideoSettingsMenu(); }; // Refresh menu to show/hide RT options
    rt.tooltip = "Experimental voxel ray tracing (OpenGL 4.3+)";
    elements.push_back(rt);
    y += rowHeight + rowGap;
    
    // Only show RT sub-options when Ray Tracing is enabled
    if (s.enableRayTracing) {
        // Ray Tracing quality selector
        UIElement rtq;
        rtq.x = leftCol; rtq.y = y; rtq.w = colWidth; rtq.h = rowHeight;
        rtq.text = "RT Quality: " + std::string(Settings::RT_QUALITY_NAMES[s.rayTracingQuality]);
        rtq.intValueRef = &s.rayTracingQuality;
        rtq.onClick = [](){};  // intValueRef handles cycling
        rtq.minVal = 0.0f; rtq.maxVal = 2.0f;  // Low=0, Medium=1, High=2
        rtq.tooltip = "Ray tracing quality (Low/Medium/High)";
        elements.push_back(rtq);
        y += rowHeight + rowGap;
        
        // Shadow Method selector
        UIElement sm;
        sm.x = leftCol; sm.y = y; sm.w = colWidth; sm.h = rowHeight;
        sm.text = "Shadows: " + std::string(Settings::SHADOW_METHOD_NAMES[s.shadowMethod]);
        sm.intValueRef = &s.shadowMethod;
        sm.onClick = [](){};
        sm.minVal = 0.0f; sm.maxVal = 1.0f;  // ShadowMap=0, RayTraced=1
        sm.tooltip = "Shadow method (Shadow Map = fast, Ray Traced = accurate)";
        elements.push_back(sm);
        y += rowHeight + rowGap;
        
        // RT Shadows toggle
        UIElement rts;
        rts.x = leftCol; rts.y = y; rts.w = colWidth; rts.h = rowHeight;
        rts.text = "RT Shadows: " + std::string(s.rtShadows ? "ON" : "OFF");
        rts.boolValueRef = &s.rtShadows;
        rts.onClick = [](){};
        rts.tooltip = "Ray traced shadows (requires RT enabled)";
        elements.push_back(rts);
        y += rowHeight + rowGap;
        
        // RT Reflections toggle
        UIElement rtrefl;
        rtrefl.x = leftCol; rtrefl.y = y; rtrefl.w = colWidth; rtrefl.h = rowHeight;
        rtrefl.text = "RT Reflections: " + std::string(s.rtReflections ? "ON" : "OFF");
        rtrefl.boolValueRef = &s.rtReflections;
        rtrefl.onClick = [](){};
        rtrefl.tooltip = "Screen-space water reflections (requires RT enabled)";
        elements.push_back(rtrefl);
    }
    
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

    // Color Template
    UIElement ct;
    ct.x = rightCol; ct.y = y; ct.w = colWidth; ct.h = rowHeight;
    ct.text = "Color Template: " + std::string(Settings::COLOR_TEMPLATE_NAMES[s.colorTemplate]);
    ct.intValueRef = &s.colorTemplate;
    ct.onClick = [](){};
    ct.minVal = 0.0f; ct.maxVal = float(Settings::NUM_COLOR_TEMPLATES - 1);
    ct.tooltip = "Cycle color grading templates";
    elements.push_back(ct);
    y += rowHeight + rowGap;

    // Sharpness
    UIElement shp;
    shp.x = rightCol; shp.y = y; shp.w = colWidth; shp.h = rowHeight;
    shp.text = "Sharpness: " + std::to_string(s.sharpness).substr(0, 3);
    shp.isSlider = true;
    shp.valueRef = &s.sharpness;
    shp.minVal = 0.0f; shp.maxVal = 1.0f;
    shp.tooltip = "Sharpen postprocess (0-1)";
    elements.push_back(shp);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - Resource Packs ===
    y += 15;
    UIElement rpHeader;
    rpHeader.x = rightCol;
    rpHeader.y = y;
    rpHeader.w = colWidth;
    rpHeader.h = 25;
    rpHeader.text = "-- Resource Packs --";
    rpHeader.isLabel = true;
    rpHeader.isHeader = true;
    elements.push_back(rpHeader);
    y += 35;
    
    // PBRANDPOM Resource Pack toggle
    UIElement pbr;
    pbr.x = rightCol; pbr.y = y; pbr.w = colWidth; pbr.h = rowHeight;
    pbr.text = "PBRANDPOM: " + std::string(s.usePBRResourcePack ? "ON" : "OFF");
    pbr.boolValueRef = &s.usePBRResourcePack;
    pbr.onClick = [this]() { 
        Settings::instance().pbrSettingsChanged = true; // Trigger shadow recalculation
        setupVideoSettingsMenu(); 
    }; // Refresh to update text
    pbr.tooltip = "Use PBRANDPOM resource pack with PBR textures (triggers shadow update)";
    elements.push_back(pbr);
    y += rowHeight + rowGap;
    
    // Parallax Mapping (3D depth effect)
    UIElement parallax;
    parallax.x = rightCol; parallax.y = y; parallax.w = colWidth; parallax.h = rowHeight;
    parallax.text = "3D Parallax: " + std::string(s.enableParallaxMapping ? "ON" : "OFF");
    parallax.boolValueRef = &s.enableParallaxMapping;
    parallax.onClick = [this]() { setupVideoSettingsMenu(); };
    parallax.tooltip = "Enable parallax occlusion mapping for 3D depth in textures";
    elements.push_back(parallax);
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
    
    // Hardware info labels in bottom-left corner
    auto cpuInfo = HardwareInfo::getCPUInfo();
    auto memInfo = HardwareInfo::getMemoryInfo();
    auto& gpuInfo = HardwareInfo::getGPUInfo();
    
    float infoY = height - 100.0f;
    float infoX = 20.0f;
    float infoH = 18.0f;
    
    UIElement cpuLabel;
    cpuLabel.x = infoX;
    cpuLabel.y = infoY;
    cpuLabel.w = 500.0f;
    cpuLabel.h = infoH;
    cpuLabel.text = "CPU: " + cpuInfo.name;
    cpuLabel.isLabel = true;
    cpuLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(cpuLabel);
    
    UIElement coreLabel;
    coreLabel.x = infoX;
    coreLabel.y = infoY + infoH;
    coreLabel.w = 500.0f;
    coreLabel.h = infoH;
    coreLabel.text = std::to_string(cpuInfo.physicalCores) + " cores / " + 
                     std::to_string(cpuInfo.logicalCores) + " threads | " +
                     std::to_string(memInfo.totalPhysicalMB / 1024) + " GB RAM";
    coreLabel.isLabel = true;
    coreLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(coreLabel);
    
    UIElement gpuLabel;
    gpuLabel.x = infoX;
    gpuLabel.y = infoY + infoH * 2;
    gpuLabel.w = 500.0f;
    gpuLabel.h = infoH;
    gpuLabel.text = "GPU: " + gpuInfo.renderer;
    gpuLabel.isLabel = true;
    gpuLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(gpuLabel);
    
    UIElement glLabel;
    glLabel.x = infoX;
    glLabel.y = infoY + infoH * 3;
    glLabel.w = 500.0f;
    glLabel.h = infoH;
    glLabel.text = "OpenGL: " + gpuInfo.version;
    glLabel.isLabel = true;
    glLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(glLabel);
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
    y += 30;
    
    UIElement info8;
    info8.x = rightCol; info8.y = y; info8.w = colWidth; info8.h = 25;
    info8.text = "~: Toggle console";
    info8.isLabel = true;
    elements.push_back(info8);
    
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

void UIManager::setupDeathScreen() {
    elements.clear();
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 50.0f;
    float gap = 15.0f;
    
    // "You Died!" title - will be rendered specially in render()
    UIElement title;
    title.x = centerX - 100;
    title.y = centerY - 120;
    title.w = 200;
    title.h = 50;
    title.text = "YOU DIED";
    title.isLabel = true;
    title.customColor = glm::vec4(0.8f, 0.1f, 0.1f, 1.0f); // Dark red
    elements.push_back(title);
    
    // Respawn button
    elements.push_back({centerX - btnW/2, centerY, btnW, btnH, "RESPAWN", false, [this]() { 
        if (onRespawn) onRespawn();
        setMenuState(MenuState::NONE);
    }});
    
    // Main Menu button
    elements.push_back({centerX - btnW/2, centerY + btnH + gap, btnW, btnH, "MAIN MENU", false, [this]() { 
        worldLoaded = false;
        if (onReturnToMainMenu) onReturnToMainMenu();
        setMenuState(MenuState::MAIN_MENU);
    }});
}

void UIManager::setupNewGameMenu() {
    elements.clear();
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 120;
    title.y = 50;
    title.w = 240;
    title.h = 35;
    title.text = "Singleplayer";
    title.isLabel = true;
    elements.push_back(title);
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f - 20;
    float inputW = 320.0f;
    float inputH = 45.0f;
    float gap = 20.0f;
    
    // Load World bar at top (above input fields)
    UIElement loadWorldBtn;
    loadWorldBtn.x = centerX - inputW / 2;
    loadWorldBtn.y = 100;
    loadWorldBtn.w = inputW;
    loadWorldBtn.h = 50;
    loadWorldBtn.text = "Load Existing World";
    loadWorldBtn.tooltip = "Load a previously saved world";
    loadWorldBtn.onClick = [this]() { 
        setMenuState(MenuState::LOAD_GAME);
    };
    elements.push_back(loadWorldBtn);
    
    // Divider label
    UIElement dividerLabel;
    dividerLabel.x = centerX - 100;
    dividerLabel.y = 165;
    dividerLabel.w = 200;
    dividerLabel.h = 20;
    dividerLabel.text = "-- Or Create New --";
    dividerLabel.isLabel = true;
    dividerLabel.customColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
    elements.push_back(dividerLabel);
    
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
    nameLabel.y = centerY - 60;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "World Name:";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);

    // World Name Input
    UIElement nameField;
    nameField.x = centerX - inputW / 2;
    nameField.y = centerY - 35;
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
    seedLabel.y = centerY + 30;
    seedLabel.w = inputW;
    seedLabel.h = 20;
    seedLabel.text = "World Seed:";
    seedLabel.isLabel = true;
    elements.push_back(seedLabel);
    
    // Seed Input
    UIElement seedField;
    seedField.x = centerX - inputW / 2;
    seedField.y = centerY + 55;
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
    seedHint.y = centerY + 105;
    seedHint.w = inputW;
    seedHint.h = 18;
    seedHint.text = "Leave empty or use numbers only";
    seedHint.isLabel = true;
    seedHint.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    elements.push_back(seedHint);

    // Create World Button
    UIElement createBtn;
    createBtn.x = centerX - inputW / 2;
    createBtn.y = centerY + 140;
    createBtn.w = inputW;
    createBtn.h = 50;
    createBtn.text = "Create World";
    createBtn.tooltip = "Generate and enter a new world";
    createBtn.onClick = [this]() { 
        long seed = 0;
        // Element indices: title=0, loadWorldBtn=1, dividerLabel=2, nameLabel=3, nameField=4, seedLabel=5, seedField=6
        std::string seedStr = *elements[6].textRef; // seedField is element 6
        
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
        
        if (onNewGame) onNewGame(*elements[4].textRef, seed); // nameField is element 4
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
    
    // Inventory Grid similar to Minecraft - with tabs for Blocks and Tools
    float slotSize = 52.0f;
    float gap = 6.0f;
    int cols = 9;
    
    // All available blocks (excluding AIR) - organized by category
    std::vector<BlockType> allBlocks = {
        // Natural terrain
        BlockType::GRASS, BlockType::DIRT,
        BlockType::STONE, BlockType::COBBLESTONE, BlockType::MOSSY_COBBLESTONE,
        BlockType::SAND, BlockType::GRAVEL, BlockType::CLAY,
        
        // Ores
        BlockType::COAL_ORE, BlockType::IRON_ORE, BlockType::GOLD_ORE, BlockType::DIAMOND_ORE,
        BlockType::EMERALD_ORE, BlockType::REDSTONE_ORE, BlockType::LAPIS_ORE,
        
        // Mineral blocks
        BlockType::IRON_BLOCK, BlockType::GOLD_BLOCK, BlockType::DIAMOND_BLOCK,
        BlockType::EMERALD_BLOCK, BlockType::REDSTONE_BLOCK,
        
        // Stone variants
        BlockType::STONE_BRICKS, BlockType::MOSSY_STONE_BRICKS, BlockType::CRACKED_STONE_BRICKS,
        BlockType::CHISELED_STONE_BRICKS,
        
        // Sandstone
        BlockType::SANDSTONE, BlockType::CHISELED_SANDSTONE,
        
        // Building blocks
        BlockType::BRICKS, BlockType::OBSIDIAN,
        BlockType::BOOKSHELF, BlockType::TNT, BlockType::CRAFTING_TABLE,
        
        // Light sources
        BlockType::GLOWSTONE, BlockType::REDSTONE_LAMP,
        
        // Glass
        BlockType::GLASS,
        
        // Wool
        BlockType::WHITE_WOOL, BlockType::ORANGE_WOOL, BlockType::MAGENTA_WOOL,
        BlockType::LIGHT_BLUE_WOOL, BlockType::YELLOW_WOOL, BlockType::LIME_WOOL,
        BlockType::PINK_WOOL, BlockType::GRAY_WOOL, BlockType::LIGHT_GRAY_WOOL,
        BlockType::CYAN_WOOL, BlockType::PURPLE_WOOL, BlockType::BLUE_WOOL,
        BlockType::BROWN_WOOL, BlockType::GREEN_WOOL, BlockType::RED_WOOL, BlockType::BLACK_WOOL,
        
        // Wood - Planks
        BlockType::WOOD, BlockType::OAK_PLANKS, BlockType::SPRUCE_PLANKS, BlockType::BIRCH_PLANKS,
        BlockType::JUNGLE_PLANKS,
        
        // Wood - Logs
        BlockType::LOG, BlockType::OAK_LOG, BlockType::SPRUCE_LOG, BlockType::BIRCH_LOG,
        BlockType::JUNGLE_LOG,
        
        // Leaves
        BlockType::LEAVES, BlockType::OAK_LEAVES, BlockType::SPRUCE_LEAVES, BlockType::BIRCH_LEAVES,
        BlockType::JUNGLE_LEAVES, BlockType::ACACIA_LEAVES, BlockType::DARK_OAK_LEAVES,
        
        // Nature
        BlockType::TALL_GRASS, BlockType::ROSE, BlockType::COBWEB, BlockType::SUGAR_CANE,
        
        // Ice variants
        BlockType::SNOW, BlockType::ICE,
        
        // Water
        BlockType::WATER,
        
        // Misc
        BlockType::SPONGE, BlockType::NOTE_BLOCK, BlockType::JUKEBOX,
        BlockType::FARMLAND,
        
        // Road blocks
        BlockType::GLAZED_TERRACOTTA,
        BlockType::ROAD_STRAIGHT, BlockType::ROAD_LEFT, BlockType::ROAD_RIGHT,
        BlockType::ROAD_LEFT_RIGHT, BlockType::ROAD_T_JUNCTION, BlockType::ROAD_INTERSECTION_YELLOW,
        BlockType::ROAD_MIDDLE_LINES, BlockType::ROAD_MIDDLE_LINES_YELLOW,
        BlockType::ROAD_MIDDLE_RIGHT, BlockType::ROAD_MIDDLE_RIGHT_YELLOW,
        BlockType::ROAD_LEFT_DIAG_45, BlockType::ROAD_LEFT_DIAG_45_YELLOW,
        BlockType::ROAD_LEFT_DIAG_60, BlockType::ROAD_LEFT_DIAG_60_YELLOW,
        BlockType::ROAD_RIGHT_DIAG_60, BlockType::ROAD_RIGHT_DIAG_YELLOW,
        
        // Bedrock (last - creative only)
        BlockType::BEDROCK
    };
    
    // All available tools
    std::vector<ItemType> allTools = {
        ItemType::SWORD_WOOD, ItemType::PICKAXE_WOOD, ItemType::AXE_WOOD, ItemType::SHOVEL_WOOD,
        ItemType::SWORD_STONE, ItemType::PICKAXE_STONE, ItemType::AXE_STONE, ItemType::SHOVEL_STONE,
        ItemType::SWORD_GOLD, ItemType::PICKAXE_GOLD, ItemType::AXE_GOLD, ItemType::SHOVEL_GOLD,
        ItemType::SWORD_DIAMOND, ItemType::PICKAXE_DIAMOND, ItemType::AXE_DIAMOND, ItemType::SHOVEL_DIAMOND
    };
    
    float totalW = cols * slotSize + (cols - 1) * gap;
    float startX = (width - totalW) / 2.0f;
    float baseY = 100.0f;
    
    // ========================================
    // INVENTORY TITLE
    // ========================================
    UIElement titleEl;
    titleEl.x = width / 2.0f - 100;
    titleEl.y = baseY;
    titleEl.w = 200;
    titleEl.h = 30;
    titleEl.text = "INVENTORY";
    titleEl.isLabel = true;
    elements.push_back(titleEl);
    
    // ========================================
    // SEARCH BAR
    // ========================================
    float searchY = baseY + 45;
    float searchW = 280.0f;
    float searchX = width / 2.0f - searchW / 2.0f;
    
    UIElement searchEl;
    searchEl.x = searchX;
    searchEl.y = searchY;
    searchEl.w = searchW;
    searchEl.h = 30;
    searchEl.text = inventorySearch.empty() ? "Search..." : inventorySearch;
    searchEl.isInput = true;
    searchEl.textRef = &inventorySearch;
    elements.push_back(searchEl);
    
    // ========================================
    // CATEGORY TABS
    // ========================================
    float tabY = searchY + 45;
    float tabW = 120.0f;
    float tabH = 28.0f;
    float tabGap = 10.0f;
    float tabsStartX = width / 2.0f - (tabW * 2 + tabGap) / 2.0f;
    
    // Blocks Tab
    UIElement blocksTab;
    blocksTab.x = tabsStartX;
    blocksTab.y = tabY;
    blocksTab.w = tabW;
    blocksTab.h = tabH;
    blocksTab.text = "BLOCKS";
    blocksTab.customColor = (inventoryTab == 0) ? glm::vec4(0.3f, 0.6f, 0.3f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    blocksTab.onClick = [this]() { inventoryTab = 0; setupInventoryMenu(); };
    elements.push_back(blocksTab);
    
    // Tools Tab
    UIElement toolsTab;
    toolsTab.x = tabsStartX + tabW + tabGap;
    toolsTab.y = tabY;
    toolsTab.w = tabW;
    toolsTab.h = tabH;
    toolsTab.text = "TOOLS";
    toolsTab.customColor = (inventoryTab == 1) ? glm::vec4(0.3f, 0.6f, 0.3f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    toolsTab.onClick = [this]() { inventoryTab = 1; setupInventoryMenu(); };
    elements.push_back(toolsTab);
    
    // ========================================
    // ITEMS GRID
    // ========================================
    float gridY = tabY + tabH + 20;
    int itemIndex = 0;
    
    // Convert search to lowercase for comparison
    std::string searchLower = inventorySearch;
    for (auto& c : searchLower) c = std::tolower(c);
    
    if (inventoryTab == 0) {
        // BLOCKS TAB
        for (size_t i = 0; i < allBlocks.size(); i++) {
            BlockType type = allBlocks[i];
            
            // Filter by search
            if (!inventorySearch.empty()) {
                std::string blockName = getBlockName(type);
                std::string nameLower = blockName;
                for (auto& c : nameLower) c = std::tolower(c);
                if (nameLower.find(searchLower) == std::string::npos) {
                    continue;
                }
            }
            
            int col = itemIndex % cols;
            int row = itemIndex / cols;
            
            float x = startX + col * (slotSize + gap);
            float y = gridY + row * (slotSize + gap);
            
            UIElement el;
            el.x = x;
            el.y = y;
            el.w = slotSize;
            el.h = slotSize;
            el.text = "";
            el.isInventoryItem = true;
            el.isInventoryTool = false;
            el.blockType = type;
            el.inventoryIndex = itemIndex;
            el.tooltip = getBlockName(type);
            el.onClick = [this, type]() {
                // Left click - assign block to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = type;  // Keep legacy array in sync
            };
            el.onRightClick = [this, type]() {
                // Right click - also assign block to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = type;  // Keep legacy array in sync
            };
            elements.push_back(el);
            itemIndex++;
        }
    } else {
        // TOOLS TAB
        for (size_t i = 0; i < allTools.size(); i++) {
            ItemType type = allTools[i];
            
            // Filter by search
            if (!inventorySearch.empty()) {
                std::string toolName = getItemName(type);
                std::string nameLower = toolName;
                for (auto& c : nameLower) c = std::tolower(c);
                if (nameLower.find(searchLower) == std::string::npos) {
                    continue;
                }
            }
            
            int col = itemIndex % cols;
            int row = itemIndex / cols;
            
            float x = startX + col * (slotSize + gap);
            float y = gridY + row * (slotSize + gap);
            
            UIElement el;
            el.x = x;
            el.y = y;
            el.w = slotSize;
            el.h = slotSize;
            el.text = "";
            el.isInventoryItem = true;
            el.isInventoryTool = true;
            el.itemType = type;
            el.inventoryIndex = itemIndex;
            el.tooltip = getItemName(type);
            el.onClick = [this, type]() {
                // Left click - assign tool to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = BlockType::AIR;  // Clear legacy array since it's now a tool
            };
            el.onRightClick = [this, type]() {
                // Right click - also assign tool to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = BlockType::AIR;  // Clear legacy array since it's now a tool
            };
            elements.push_back(el);
            itemIndex++;
        }
    }
    
    // ========================================
    // CURRENT HOTBAR DISPLAY
    // ========================================
    float hotbarDisplayY = height - 160.0f;
    float hotbarLabelY = hotbarDisplayY - 30.0f;
    
    UIElement hotbarLabel;
    hotbarLabel.x = width / 2.0f - 80;
    hotbarLabel.y = hotbarLabelY;
    hotbarLabel.w = 160;
    hotbarLabel.h = 20;
    hotbarLabel.text = "HOTBAR";
    hotbarLabel.isLabel = true;
    elements.push_back(hotbarLabel);
    
    float hotbarSlotSize = 48.0f;
    float hotbarGap = 4.0f;
    float hotbarTotalW = 9 * hotbarSlotSize + 8 * hotbarGap;
    float hotbarStartX = (width - hotbarTotalW) / 2.0f;
    
    for (int i = 0; i < 9; i++) {
        const HotbarSlot& hSlot = hotbarSlots[i];
        
        UIElement slot;
        slot.x = hotbarStartX + i * (hotbarSlotSize + hotbarGap);
        slot.y = hotbarDisplayY;
        slot.w = hotbarSlotSize;
        slot.h = hotbarSlotSize;
        slot.text = std::to_string(i + 1);
        slot.isInventoryItem = true;
        slot.inventoryIndex = 100 + i;  // Use 100+ for hotbar slots to differentiate
        
        // Set the slot content based on hotbarSlots
        if (hSlot.isItem) {
            slot.isInventoryTool = true;
            slot.itemType = hSlot.itemStack.type;
            slot.blockType = BlockType::AIR;
        } else {
            slot.isInventoryTool = false;
            slot.blockType = hSlot.blockType;
            slot.itemType = ItemType::NONE;
        }
        
        slot.onClick = [this, i]() {
            selectedSlot = i;
        };
        slot.onRightClick = [this, i]() {
            // Clear slot
            hotbarSlots[i] = HotbarSlot(BlockType::AIR);
            hotbar[i] = BlockType::AIR;
        };
        elements.push_back(slot);
    }
    
    // ========================================
    // CLOSE INSTRUCTION
    // ========================================
    UIElement closeEl;
    closeEl.x = width / 2.0f - 100;
    closeEl.y = height - 60.0f;
    closeEl.w = 200;
    closeEl.h = 30;
    closeEl.text = "PRESS [E] TO CLOSE";
    closeEl.isLabel = true;
    elements.push_back(closeEl);
}

void UIManager::generateMapTexture() {
    if (!worldGenerator) return;
    
    // Check if cache is valid (same position and scale)
    if (mapCacheValid && 
        std::abs(mapCachedCenterX - mapCenterX) < 0.01f &&
        std::abs(mapCachedCenterZ - mapCenterZ) < 0.01f &&
        std::abs(mapCachedScale - mapScale) < 0.01f) {
        return; // Use cached texture
    }
    
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
                // Land - color by biome using BiomeInfo map colors
                BiomeInfo biomeInfo = worldGenerator->getBiomeInfo(biome);
                float heightFactor = std::min(terrainHeight / 100.0f, 1.0f);
                
                switch (biome) {
                    case BiomeType::OCEAN:
                        r = 40; g = 100; b = 200;
                        break;
                    case BiomeType::RIVER:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::PLAINS:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.7f + 0.3f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.7f + 0.3f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::DESERT:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::FOREST:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.8f + 0.2f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.8f + 0.2f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::BIRCH_FOREST:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.85f + 0.15f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.85f + 0.15f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::TAIGA:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.8f + 0.2f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.8f + 0.2f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::JUNGLE:
                        // Jungle - lush dark green
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.75f + 0.25f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.75f + 0.25f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::SWAMP:
                        // Swamp - murky brownish green
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::SAVANNA:
                        // Savanna - dry tan/yellow
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.85f + 0.15f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.85f + 0.15f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
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
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::VILLAGE:
                        // Warm brown
                        r = static_cast<unsigned char>((0.65f + 0.1f * heightFactor) * 255);
                        g = static_cast<unsigned char>((0.52f + 0.08f * heightFactor) * 255);
                        b = static_cast<unsigned char>(0.35f * 255);
                        break;
                    case BiomeType::CITY:
                        // Gray stone
                        r = static_cast<unsigned char>((0.55f + 0.1f * heightFactor) * 255);
                        g = static_cast<unsigned char>((0.55f + 0.1f * heightFactor) * 255);
                        b = static_cast<unsigned char>((0.6f + 0.08f * heightFactor) * 255);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  // Smooth scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Update cache status
    mapCacheValid = true;
    mapCachedCenterX = mapCenterX;
    mapCachedCenterZ = mapCenterZ;
    mapCachedScale = mapScale;
}

void UIManager::setupMapMenu() {
    elements.clear();
    
    // Center the player position for the map
    mapCenterX = currentPlayerPos.x;
    mapCenterZ = currentPlayerPos.z;
    mapTargetCenterX = mapCenterX;
    mapTargetCenterZ = mapCenterZ;
    mapTargetScale = mapScale;
    mapPanVelocityX = 0.0f;
    mapPanVelocityZ = 0.0f;
    mapDragging = false;
    
    // Invalidate cache to force regeneration on first open
    mapCacheValid = false;
    
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
    
    // Close button - positioned below map
    float btnW = 200.0f;
    float btnH = 40.0f;
    elements.push_back({
        centerX - btnW / 2, mapY + mapDisplaySize + 30, btnW, btnH, 
        "CLOSE (M)", false, [this]() {
            setMenuState(MenuState::NONE);
        }
    });
    
    // Zoom controls with smooth animation - left side
    elements.push_back({
        mapX - 55, centerY - 50, 45, 45, "+", false, [this]() {
            mapTargetScale = std::max(1.0f, mapTargetScale / 2.0f);
        }
    });
    elements.push_back({
        mapX - 55, centerY + 10, 45, 45, "-", false, [this]() {
            mapTargetScale = std::min(64.0f, mapTargetScale * 2.0f);
        }
    });
    
    // Center on player button
    elements.push_back({
        mapX - 55, centerY + 70, 45, 45, "@", false, [this]() {
            mapTargetCenterX = currentPlayerPos.x;
            mapTargetCenterZ = currentPlayerPos.z;
            mapPanVelocityX = 0.0f;
            mapPanVelocityZ = 0.0f;
        }
    });
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
    
    // Consistent button styling (matching main menu)
    float btnW = 280.0f;
    float btnH = 50.0f;
    float gap = 12.0f;
    float startY = height / 2.0f - btnH - gap/2;
    
    // Host Game Button
    UIElement hostBtn;
    hostBtn.x = centerX - btnW/2;
    hostBtn.y = startY;
    hostBtn.w = btnW;
    hostBtn.h = btnH;
    hostBtn.text = "Host Game";
    hostBtn.tooltip = "Create a server for others to join";
    hostBtn.onClick = [this]() { setMenuState(MenuState::HOST_GAME); };
    elements.push_back(hostBtn);
    
    // Join Game Button
    UIElement joinBtn;
    joinBtn.x = centerX - btnW/2;
    joinBtn.y = startY + btnH + gap;
    joinBtn.w = btnW;
    joinBtn.h = btnH;
    joinBtn.text = "Join Game";
    joinBtn.tooltip = "Connect to an existing server";
    joinBtn.onClick = [this]() { setMenuState(MenuState::JOIN_GAME); };
    elements.push_back(joinBtn);

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
    
    // Load values from settings - use playerNickname from Player Settings
    auto& settings = Settings::instance();
    playerName = settings.playerNickname; // Always use Player Settings nickname
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name display (read-only, shows from Player Settings)
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name (from Player Settings):";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name display (non-editable)
    UIElement nameDisplay;
    nameDisplay.x = centerX - inputW/2;
    nameDisplay.y = startY + 25;
    nameDisplay.w = inputW;
    nameDisplay.h = inputH;
    nameDisplay.text = settings.playerNickname;
    nameDisplay.isLabel = true;
    nameDisplay.customColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    elements.push_back(nameDisplay);
    
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
    
    // Load values from settings - use playerNickname from Player Settings
    auto& settings = Settings::instance();
    playerName = settings.playerNickname; // Always use Player Settings nickname
    if (serverAddress.empty() || serverAddress == "localhost") {
        serverAddress = settings.lastServerAddress;
    }
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name display (read-only, shows from Player Settings)
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name (from Player Settings):";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name display (non-editable)
    UIElement nameDisplay;
    nameDisplay.x = centerX - inputW/2;
    nameDisplay.y = startY + 25;
    nameDisplay.w = inputW;
    nameDisplay.h = inputH;
    nameDisplay.text = settings.playerNickname;
    nameDisplay.isLabel = true;
    nameDisplay.customColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    elements.push_back(nameDisplay);
    
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

void UIManager::setupAudioSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    float centerX = width / 2.0f;
    float startY = 100.0f;
    float lineHeight = 55.0f;
    float sliderW = 400.0f;
    float sliderH = 30.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 50;
    title.text = "AUDIO SETTINGS";
    elements.push_back(title);
    startY += 70.0f;
    
    // Master Volume
    UIElement master;
    master.x = centerX - sliderW/2;
    master.y = startY;
    master.w = sliderW;
    master.h = sliderH;
    master.text = "Master Volume: " + std::to_string(static_cast<int>(s.masterVolume * 100)) + "%";
    master.isSlider = true;
    master.valueRef = &s.masterVolume;
    master.minVal = 0.0f;
    master.maxVal = 1.0f;
    elements.push_back(master);
    startY += lineHeight;
    
    // Music Volume
    UIElement music;
    music.x = centerX - sliderW/2;
    music.y = startY;
    music.w = sliderW;
    music.h = sliderH;
    music.text = "Music Volume: " + std::to_string(static_cast<int>(s.musicVolume * 100)) + "%";
    music.isSlider = true;
    music.valueRef = &s.musicVolume;
    music.minVal = 0.0f;
    music.maxVal = 1.0f;
    elements.push_back(music);
    startY += lineHeight;
    
    // Sound Effects Volume
    UIElement sounds;
    sounds.x = centerX - sliderW/2;
    sounds.y = startY;
    sounds.w = sliderW;
    sounds.h = sliderH;
    sounds.text = "Sound Effects: " + std::to_string(static_cast<int>(s.soundVolume * 100)) + "%";
    sounds.isSlider = true;
    sounds.valueRef = &s.soundVolume;
    sounds.minVal = 0.0f;
    sounds.maxVal = 1.0f;
    elements.push_back(sounds);
    startY += lineHeight;
    
    // Ambient Volume
    UIElement ambient;
    ambient.x = centerX - sliderW/2;
    ambient.y = startY;
    ambient.w = sliderW;
    ambient.h = sliderH;
    ambient.text = "Ambient Volume: " + std::to_string(static_cast<int>(s.ambientVolume * 100)) + "%";
    ambient.isSlider = true;
    ambient.valueRef = &s.ambientVolume;
    ambient.minVal = 0.0f;
    ambient.maxVal = 1.0f;
    elements.push_back(ambient);
    startY += lineHeight + 20.0f;
    
    // Back button
    UIElement back;
    back.x = centerX - 150;
    back.y = startY;
    back.w = 300;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { 
        // Apply audio settings
        auto& settings = Settings::instance();
        Audio::AudioManager::instance().setMasterVolume(settings.masterVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, settings.musicVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, settings.ambientVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::WEATHER, settings.ambientVolume);
        setMenuState(MenuState::SETTINGS); 
    };
    elements.push_back(back);
}

void UIManager::setupPlayerSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    float centerX = width / 2.0f;
    float startY = 80.0f;
    float lineHeight = 50.0f;
    float btnW = 280.0f;
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
    
    // Load preview model on menu setup
    loadPreviewModel(s.playerModelIndex);
    
    // Nickname input
    UIElement nicknameLabel;
    nicknameLabel.x = 80.0f;
    nicknameLabel.y = startY;
    nicknameLabel.w = 150;
    nicknameLabel.h = 30;
    nicknameLabel.text = "Nickname:";
    nicknameLabel.isLabel = true;
    elements.push_back(nicknameLabel);
    
    UIElement nicknameInput;
    nicknameInput.x = 240.0f;
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
    modelLabel.x = 80.0f;
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
        modelBtn.x = 80.0f;
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
            setupPlayerSettingsMenu(); // Refresh to show selection and preview
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
