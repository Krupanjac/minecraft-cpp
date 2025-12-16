#pragma once

#include "../Render/Shader.h"
#include "../Core/Settings.h"
#include "../World/Block.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

enum class MenuState {
    NONE,
    MAIN_MENU,
    IN_GAME_MENU,
    SETTINGS,
    VIDEO_SETTINGS,
    LOAD_GAME,
    NEW_GAME,
    INVENTORY,
    CONTROLS
};

struct UIElement {
    float x, y, w, h;
    std::string text;
    bool isHovered = false;
    std::function<void()> onClick;
    
    // For sliders
    bool isSlider = false;
    float* valueRef = nullptr;
    int* intValueRef = nullptr; // Added for integer support
    bool* boolValueRef = nullptr; // Added for toggle support
    float minVal = 0.0f;
    float maxVal = 1.0f;
    
    // For text input
    bool isInput = false;
    std::string* textRef = nullptr;

    // For Inventory
    BlockType blockType = BlockType::AIR;
    bool isInventoryItem = false;
    std::function<void()> onRightClick;

    // For Keybinding
    bool isKeybind = false;
    int* keyBindRef = nullptr;
};

class UIManager {
public:
    UIManager();
    ~UIManager() = default;

    void initialize(int windowWidth, int windowHeight);
    void render();
    void update(float deltaTime, double mouseX, double mouseY, bool mousePressed, bool rightMousePressed = false);
    void handleResize(int width, int height);
    void handleCharInput(unsigned int codepoint); // For text input
    void handleKeyInput(int key); // For special keys like Backspace

    void setMenuState(MenuState state);
    MenuState getMenuState() const { return currentMenuState; }
    bool isMenuOpen() const { return currentMenuState != MenuState::NONE; }

    void setOnSettingsChanged(std::function<void()> callback) { onSettingsChanged = callback; }
    void setOnNewGame(std::function<void(std::string, long)> callback) { onNewGame = callback; }
    void setOnLoadGame(std::function<void(std::string)> callback) { onLoadGame = callback; }
    void setOnExit(std::function<void()> callback) { onExit = callback; }
    void setOnSave(std::function<void()> callback) { onSave = callback; }

    void toggleDebug() { showDebug = !showDebug; }

    void updateDebugInfo(float fps, const std::string& blockName, const glm::vec3& playerPos, const glm::vec3& playerVel);

    BlockType getSelectedBlock() const { return hotbar[selectedSlot]; }
    void selectHotbarSlot(int slot) { if (slot >= 0 && slot < 9) selectedSlot = slot; }

    // HUD Stats
    int playerHealth = 20; // 0-20 (10 hearts)
    int playerFood = 20;   // 0-20 (10 shanks)
    float playerXP = 0.0f; // 0.0 - 1.0
    int playerLevel = 0;
    
    // Hotbar
    BlockType hotbar[9] = { 
        BlockType::STONE, BlockType::DIRT, BlockType::WOOD, BlockType::LEAVES, 
        BlockType::SAND, BlockType::GRAVEL, BlockType::GRASS, BlockType::WATER, BlockType::AIR 
    };
    int selectedSlot = 0;
    
    // Debug controls
    float timeOfDay = 0.0f; // 0-1200
    bool isDayNightPaused = false;

private:
    MenuState currentMenuState = MenuState::MAIN_MENU;
    bool showDebug = false;
    float currentFPS = 0.0f;
    std::string currentBlockName = "None";
    glm::vec3 currentPlayerPos = glm::vec3(0.0f);
    glm::vec3 currentPlayerVel = glm::vec3(0.0f);

    bool waitingForKeyBind = false;
    int* keyBindPtr = nullptr;

    BlockType selectedBlock = BlockType::STONE; // Deprecated by hotbar, keeping for internal ref if needed, but hotbar[selectedSlot] is primary.
    
    void renderHUD();

    int width, height;
    Shader uiShader;
    GLuint vao, vbo;
    
    std::vector<UIElement> elements;
    std::function<void()> onSettingsChanged;
    std::function<void(std::string, long)> onNewGame;
    std::function<void(std::string)> onLoadGame;
    std::function<void()> onExit;
    std::function<void()> onSave;
    
    // Input state
    std::string newWorldName = "New World";
    std::string newWorldSeed = "12345";
    bool lastMousePressed = false;
    bool lastRightMousePressed = false;
    
    void setupMainMenu();
    void setupInGameMenu();
    void setupSettingsMenu();
    void setupVideoSettingsMenu();
    void setupControlsMenu();
    void setupLoadGameMenu();
    void setupNewGameMenu();
    void setupInventoryMenu();
    
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color);
    
    // Helper for vector font
    void drawLine(float x1, float y1, float x2, float y2, const glm::vec4& color);
    
    // Helper to get color for block preview
    glm::vec4 getBlockColor(BlockType type);
};
