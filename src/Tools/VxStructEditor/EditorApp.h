// ============================================================================
// VxStruct Editor - Application Class Declaration
// Contains the VxStructEditor class, EditorAction struct, and all method decls.
// ============================================================================
#pragma once

#include "EditorCommon.h"

// ============================================================================
// Undo/Redo Action
// ============================================================================

struct EditorAction {
    enum class Type { PLACE_BLOCK, REMOVE_BLOCK, PLACE_MULTIPLE, REMOVE_MULTIPLE, PASTE, FILL_SELECTION };
    Type type;

    // Single block ops
    glm::ivec3 position;
    BlockType blockType = BlockType::AIR;
    BlockType previousType = BlockType::AIR;
    uint8_t metadata = 0;
    uint8_t previousMetadata = 0;

    // Multi-block ops
    std::vector<StructureBlock> blocks;
    std::vector<StructureBlock> previousBlocks;
};

// ============================================================================
// VxStruct Editor Application
// ============================================================================

class VxStructEditor {
public:
    VxStructEditor() = default;
    ~VxStructEditor() { cleanup(); }

    bool initialize();
    void run();
    void cleanup();

private:
    // --- Window ---
    GLFWwindow* m_window = nullptr;
    int m_windowWidth = 1600;
    int m_windowHeight = 900;

    // --- Shaders ---
    GLuint m_blockShader = 0;
    GLuint m_gridShader = 0;
    GLuint m_wireShader = 0;

    // --- Mesh buffers ---
    GLuint m_blockVAO = 0, m_blockVBO = 0;
    GLuint m_gridVAO = 0, m_gridVBO = 0;
    GLuint m_wireVAO = 0, m_wireVBO = 0;
    int m_blockVertexCount = 0;
    int m_gridVertexCount = 0;

    // --- Camera ---
    OrbitCamera m_camera;
    bool m_isDragging = false;
    bool m_isPanning = false;
    double m_lastMouseX = 0, m_lastMouseY = 0;

    // --- Editor state ---
    Structure m_structure;
    std::string m_currentFilePath;
    bool m_modified = false;

    // --- Block palette ---
    BlockType m_selectedBlock = BlockType::OAK_PLANKS;
    std::string m_paletteFilter;
    std::string m_selectedCategory = "All";

    // --- Tools ---
    enum class EditorTool { PLACE, ERASE, PICK, MARKER, SELECT };
    EditorTool m_currentTool = EditorTool::PLACE;

    // --- Grid/display ---
    int m_gridSize = 32;
    bool m_showGrid = true;
    bool m_showWireframe = true;
    bool m_showAxes = true;
    int m_currentLayer = -1;

    // --- Hover/rayhit ---
    RayHit m_hoverHit;
    glm::ivec3 m_hoverPlacePos = {0, 0, 0};
    bool m_hasHover = false;

    // --- Multi-block selection ---
    std::set<int64_t> m_selectedBlocks;
    bool m_boxSelectActive = false;
    glm::ivec3 m_boxSelectStart = {0, 0, 0};
    glm::ivec3 m_boxSelectEnd = {0, 0, 0};
    bool m_boxSelectHasStart = false;
    glm::ivec3 m_selectionAnchor = {0, 0, 0};  // Anchor for Shift+click range select
    bool m_hasSelectionAnchor = false;

    // --- Clipboard ---
    std::vector<StructureBlock> m_clipboard;
    glm::ivec3 m_clipboardOrigin = {0, 0, 0};

    // --- Undo/Redo ---
    std::deque<EditorAction> m_undoStack;
    std::deque<EditorAction> m_redoStack;
    static const size_t MAX_UNDO = 200;

    // --- Marker editing ---
    std::string m_markerType = "door";
    std::string m_markerData;

    // --- Structure metadata ---
    char m_nameBuffer[128] = "New Structure";
    char m_authorBuffer[128] = "VxStruct Editor";
    int m_categoryIndex = 0;
    bool m_requiresFlat = true;
    float m_minGroundCoverage = 0.7f;
    char m_tagBuffer[64] = "";
    std::vector<std::string> m_tags;

    // --- UI popups ---
    bool m_showHelpWindow = false;
    bool m_showAboutWindow = false;
    bool m_showSettingsWindow = false;

    // --- Rotation ---
    int m_rotationAngle = 0;
    RotationAxis m_rotationAxis = RotationAxis::Y;
    RotationPivot m_rotationPivot = RotationPivot::BOUNDING_CENTER;
    glm::ivec3 m_customPivot = {0, 0, 0};

    // --- Move axis constraint ---
    MoveAxis m_moveAxis = MoveAxis::FREE;

    // --- Block-based move offset (for GUI move tool) ---
    int m_moveOffsetX = 0;
    int m_moveOffsetY = 0;
    int m_moveOffsetZ = 0;

    // --- Cumulative move tracking (Blender-style visible offset) ---
    glm::ivec3 m_cumulativeMoveOffset = {0, 0, 0};
    glm::ivec3 m_moveOriginCenter = {0, 0, 0}; // center of selection when move started
    bool m_hasMoveOrigin = false;

    // --- Block atlas texture ---
    GLuint m_atlasTexture = 0;

    // --- PBR textures (albedo array) ---
    GLuint m_pbrAlbedoArray = 0;
    std::unordered_map<std::string, int> m_pbrTextureMap;
    int m_pbrTexSize = 0;
    int m_textureMode = 0; // 0=color, 1=atlas, 2=PBR

    // --- Settings & auto-save ---
    EditorSettings m_settings;
    std::string m_settingsFilePath = "editor_settings.ini";
    double m_lastAutoSaveTime = 0.0;

    // ---- Selection encoding helpers ----
    static int64_t encodePos(const glm::ivec3& p) {
        return ((int64_t)(p.x + 10000) << 32) | ((int64_t)(p.y + 10000) << 16) | (int64_t)(p.z + 10000);
    }
    static glm::ivec3 decodePos(int64_t e) {
        int x = (int)((e >> 32) & 0xFFFF) - 10000;
        int y = (int)((e >> 16) & 0xFFFF) - 10000;
        int z = (int)(e & 0xFFFF) - 10000;
        return {x, y, z};
    }
    bool isBlockSelected(const glm::ivec3& pos) const {
        return m_selectedBlocks.count(encodePos(pos)) > 0;
    }

    // ---- Core loop ----
    void processInput();
    void updateHover();
    void rebuildBlockMesh();
    void rebuildGridMesh();

    // ---- Rendering ----
    void render();
    void renderSelectionHighlights();
    void renderPivotMarker();
    void renderMoveOffsetIndicator();

    // ---- UI ----
    void renderUI();
    void renderMenuBar();
    void renderToolbar();
    void renderBlockPalette();
    void renderProperties();
    void renderStructureInfo();
    void renderMarkerPanel();
    void renderHelpWindow();
    void renderAboutWindow();
    void renderSelectionPanel();
    void renderSettingsWindow();

    // ---- File I/O ----
    void newStructure();
    void loadStructure();
    void saveStructure();
    void saveStructureAs();
    void exportStructure();
    void openRecentFile(const std::string& path);
    void autoSave();
    void updateTextureMode();

    // ---- Undo/Redo ----
    void pushAction(const EditorAction& action);
    void undo();
    void redo();

    // ---- Edit operations ----
    void placeBlockWithUndo(const glm::ivec3& pos, BlockType type, uint8_t metadata = 0);
    void removeBlockWithUndo(const glm::ivec3& pos);
    void deleteSelectedBlocks();
    void copySelection();
    void cutSelection();
    void moveSelection();
    void pasteClipboard();
    void selectAll();
    void deselectAll();
    void invertSelection();
    void selectRange(const glm::ivec3& from, const glm::ivec3& to);
    void rotateStructure();
    void rotateSelection();
    void rotateStructureAroundAxis(RotationAxis axis);
    void rotateSelectionAroundAxis(RotationAxis axis);
    void rotateBlockFaces(const glm::ivec3& pos);          // Rotate single block's faces 90° CW
    void rotateSelectedBlocksFaces();                       // Rotate all selected blocks' faces 90° CW
    void moveSelectionByOffset(const glm::ivec3& offset);
    void fillSelection(BlockType type);
    void duplicateSelection();

    // ---- GLFW Callbacks ----
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};
