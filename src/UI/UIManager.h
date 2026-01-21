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
    PLAYER_SETTINGS,
    LOAD_GAME,
    NEW_GAME,
    INVENTORY,
    CONTROLS,
    MAP,
    MULTIPLAYER,
    HOST_GAME,
    JOIN_GAME,
    CHAT,
    ABOUT
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
    
    // For tooltips
    std::string tooltip;
    
    // Visual styling
    bool isLabel = false;      // Non-interactive label
    bool isCard = false;       // Card-style container
    bool isHeader = false;     // Section header
    glm::vec4 customColor = glm::vec4(0.0f);  // Custom color (if any component > 0)
    
    // For world preview thumbnails
    GLuint textureId = 0;      // OpenGL texture ID for preview image
    float thumbnailX = 0;      // X position for thumbnail within element
    float thumbnailY = 0;      // Y position for thumbnail within element
    float thumbnailW = 0;      // Width of thumbnail
    float thumbnailH = 0;      // Height of thumbnail
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
    void setOnTeleport(std::function<void(float, float)> callback) { onTeleport = callback; }
    void setWorldGenerator(class WorldGenerator* gen) { worldGenerator = gen; }
    void setOnReturnToMainMenu(std::function<void()> callback) { onReturnToMainMenu = callback; }
    
    // Multiplayer callbacks
    void setOnHostGame(std::function<void(std::string, int)> callback) { onHostGame = callback; }
    void setOnJoinGame(std::function<void(std::string, std::string, int)> callback) { onJoinGame = callback; }
    void setOnDisconnect(std::function<void()> callback) { onDisconnectGame = callback; }
    void setNetworkStatus(const std::string& status) { networkStatus = status; }
    void setIsOnline(bool online) { isOnline = online; }
    
    // Chat
    void setOnSendChat(std::function<void(const std::string&)> callback) { onSendChat = callback; }
    void addChatMessage(const std::string& playerName, const std::string& message);
    bool isChatOpen() const { return currentMenuState == MenuState::CHAT; }
    void openChat();
    void closeChat();
    
    // World state
    void setWorldLoaded(bool loaded) { worldLoaded = loaded; }
    bool isWorldLoaded() const { return worldLoaded; }

    void toggleDebug() { showDebug = !showDebug; }

    void updateDebugInfo(float fps, const std::string& blockName, const glm::vec3& playerPos, const glm::vec3& playerVel, float taaMotion = 0.0f, float taaHistoryWeight = 0.0f);

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
    // TAA debug metrics
    float lastTaaMotion = 0.0f;
    float lastTaaHistoryWeight = 0.0f;
    float currentFPS = 0.0f;
    std::string currentBlockName = "None";
    glm::vec3 currentPlayerPos = glm::vec3(0.0f);
    glm::vec3 currentPlayerVel = glm::vec3(0.0f);

    bool waitingForKeyBind = false;
    int* keyBindPtr = nullptr;
    
    // Tooltip state
    std::string currentTooltip;
    float tooltipX = 0.0f, tooltipY = 0.0f;
    float tooltipTimer = 0.0f;
    const float tooltipDelay = 0.5f; // Seconds before tooltip appears

    BlockType selectedBlock = BlockType::STONE; // Deprecated by hotbar, keeping for internal ref if needed, but hotbar[selectedSlot] is primary.
    
    void renderHUD();
    void renderChat();

    int width, height;
    Shader uiShader;
    Shader texturedShader;  // For rendering world preview thumbnails
    Shader blockIconShader; // For rendering 3D isometric block icons
    GLuint vao, vbo;
    GLuint texturedVao, texturedVbo;  // VAO/VBO with texture coords
    GLuint blockIconVao, blockIconVbo; // VAO/VBO for isometric block cube
    GLuint blockAtlasTexture = 0;      // Block texture atlas
    
    std::vector<UIElement> elements;
    std::function<void()> onSettingsChanged;
    std::function<void(std::string, long)> onNewGame;
    std::function<void(std::string)> onLoadGame;
    std::function<void()> onExit;
    std::function<void()> onSave;
    std::function<void()> onReturnToMainMenu;
    std::function<void(std::string, int)> onHostGame;
    std::function<void(std::string, std::string, int)> onJoinGame;
    std::function<void()> onDisconnectGame;
    std::function<void(const std::string&)> onSendChat;
    
    // Chat state
    std::string chatInput;
    struct ChatEntry {
        std::string playerName;
        std::string message;
        float timestamp;
    };
    std::vector<ChatEntry> chatMessages;
    
    // Input state
    std::string newWorldName = "New World";
    std::string newWorldSeed = "12345";
    std::string playerName = "Player";
    std::string serverAddress = "127.0.0.1";
    std::string serverPort = "25565";
    std::string networkStatus;
    bool isOnline = false;
    bool worldLoaded = false;
    bool lastMousePressed = false;
    bool lastRightMousePressed = false;
    
    void setupMainMenu();
    void setupInGameMenu();
    void setupSettingsMenu();
    void setupVideoSettingsMenu();
    void setupPlayerSettingsMenu();
    void setupControlsMenu();
    void setupLoadGameMenu();
    void setupNewGameMenu();
    void setupInventoryMenu();
    void setupMapMenu();
    void setupMultiplayerMenu();
    void setupHostGameMenu();
    void setupJoinGameMenu();
    void setupAboutMenu();
    void generateMapTexture();
    
    // Map data
    class WorldGenerator* worldGenerator = nullptr;
    GLuint mapTexture = 0;
    int mapTextureSize = 512; // Size of the map texture
    float mapCenterX = 0.0f;  // World X coordinate at map center
    float mapCenterZ = 0.0f;  // World Z coordinate at map center
    float mapScale = 8.0f;    // Blocks per pixel
    std::function<void(float, float)> onTeleport;
    
    // World preview textures cache
    std::unordered_map<std::string, GLuint> worldPreviewTextures;
    GLuint loadWorldPreviewTexture(const std::string& worldName);
    void clearWorldPreviewTextures();
    
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawTexturedRect(float x, float y, float w, float h, GLuint textureId);
    void drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color);
    void drawBlockIcon(float x, float y, float size, BlockType type); // Draw 3D isometric block
    
    // Helper for vector font
    void drawLine(float x1, float y1, float x2, float y2, const glm::vec4& color);
    
    // Helper to get color for block preview
    glm::vec4 getBlockColor(BlockType type);
    
    // Helper to get texture atlas index for a block face
    int getBlockTextureIndex(BlockType type, int face); // face: 0=top, 1=bottom, 2=side
};
