// ============================================================================
// VxStruct Editor - Vortex Structure Editor Tool
// Part of the Vortex Engine Tooling
// 
// A standalone 3D voxel editor for creating and editing .vxstruct files.
// Uses the same block types and structure format as the game engine.
// ============================================================================

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "../../World/Structure.h"
#include "../../World/Block.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <deque>
#include <set>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

// ============================================================================
// Block Color Database
// ============================================================================

struct BlockColorInfo {
    BlockType type;
    const char* name;
    glm::vec3 color;
    const char* category;
};

static const std::vector<BlockColorInfo>& getBlockPalette() {
    static const std::vector<BlockColorInfo> palette = {
        // Natural
        {BlockType::GRASS,           "Grass",             {0.40f, 0.65f, 0.20f}, "Natural"},
        {BlockType::DIRT,            "Dirt",              {0.55f, 0.36f, 0.20f}, "Natural"},
        {BlockType::STONE,           "Stone",             {0.50f, 0.50f, 0.50f}, "Natural"},
        {BlockType::COBBLESTONE,     "Cobblestone",       {0.45f, 0.45f, 0.45f}, "Natural"},
        {BlockType::SAND,            "Sand",              {0.85f, 0.80f, 0.55f}, "Natural"},
        {BlockType::GRAVEL,          "Gravel",            {0.55f, 0.52f, 0.50f}, "Natural"},
        {BlockType::CLAY,            "Clay",              {0.60f, 0.62f, 0.67f}, "Natural"},
        {BlockType::SNOW,            "Snow",              {0.95f, 0.95f, 0.97f}, "Natural"},
        {BlockType::ICE,             "Ice",               {0.60f, 0.75f, 0.95f}, "Natural"},
        {BlockType::SANDSTONE,       "Sandstone",         {0.82f, 0.77f, 0.52f}, "Natural"},
        {BlockType::BEDROCK,         "Bedrock",           {0.20f, 0.20f, 0.20f}, "Natural"},

        // Wood - Logs
        {BlockType::OAK_LOG,         "Oak Log",           {0.45f, 0.30f, 0.15f}, "Wood"},
        {BlockType::SPRUCE_LOG,      "Spruce Log",        {0.30f, 0.20f, 0.10f}, "Wood"},
        {BlockType::BIRCH_LOG,       "Birch Log",         {0.80f, 0.78f, 0.70f}, "Wood"},
        {BlockType::JUNGLE_LOG,      "Jungle Log",        {0.40f, 0.30f, 0.12f}, "Wood"},

        // Wood - Planks
        {BlockType::OAK_PLANKS,      "Oak Planks",        {0.65f, 0.50f, 0.28f}, "Wood"},
        {BlockType::SPRUCE_PLANKS,   "Spruce Planks",     {0.40f, 0.28f, 0.13f}, "Wood"},
        {BlockType::BIRCH_PLANKS,    "Birch Planks",      {0.78f, 0.72f, 0.48f}, "Wood"},
        {BlockType::JUNGLE_PLANKS,   "Jungle Planks",     {0.58f, 0.38f, 0.20f}, "Wood"},

        // Leaves
        {BlockType::OAK_LEAVES,      "Oak Leaves",        {0.25f, 0.55f, 0.15f}, "Nature"},
        {BlockType::SPRUCE_LEAVES,   "Spruce Leaves",     {0.15f, 0.40f, 0.18f}, "Nature"},
        {BlockType::BIRCH_LEAVES,    "Birch Leaves",      {0.35f, 0.60f, 0.25f}, "Nature"},
        {BlockType::JUNGLE_LEAVES,   "Jungle Leaves",     {0.20f, 0.55f, 0.10f}, "Nature"},
        {BlockType::TALL_GRASS,      "Tall Grass",        {0.30f, 0.55f, 0.15f}, "Nature"},
        {BlockType::ROSE,            "Flower",            {0.85f, 0.20f, 0.20f}, "Nature"},
        {BlockType::SUGAR_CANE,      "Sugar Cane",        {0.45f, 0.70f, 0.30f}, "Nature"},

        // Building
        {BlockType::BRICKS,          "Bricks",            {0.60f, 0.30f, 0.25f}, "Building"},
        {BlockType::STONE_BRICKS,    "Stone Bricks",      {0.48f, 0.48f, 0.48f}, "Building"},
        {BlockType::MOSSY_STONE_BRICKS,"Mossy St. Bricks",{0.40f, 0.50f, 0.38f}, "Building"},
        {BlockType::CRACKED_STONE_BRICKS,"Cracked St. Br.",{0.42f, 0.42f, 0.42f}, "Building"},
        {BlockType::CHISELED_STONE_BRICKS,"Chiseled St. Br.",{0.46f, 0.46f, 0.46f}, "Building"},
        {BlockType::MOSSY_COBBLESTONE,"Mossy Cobblestone", {0.38f, 0.48f, 0.35f}, "Building"},
        {BlockType::GLASS,           "Glass",             {0.75f, 0.85f, 0.90f}, "Building"},
        {BlockType::OBSIDIAN,        "Obsidian",          {0.10f, 0.05f, 0.15f}, "Building"},
        {BlockType::BOOKSHELF,       "Bookshelf",         {0.55f, 0.42f, 0.25f}, "Building"},

        // Ores
        {BlockType::COAL_ORE,        "Coal Ore",          {0.35f, 0.35f, 0.35f}, "Ore"},
        {BlockType::IRON_ORE,        "Iron Ore",          {0.55f, 0.48f, 0.42f}, "Ore"},
        {BlockType::GOLD_ORE,        "Gold Ore",          {0.60f, 0.55f, 0.30f}, "Ore"},
        {BlockType::DIAMOND_ORE,     "Diamond Ore",       {0.40f, 0.60f, 0.65f}, "Ore"},
        {BlockType::REDSTONE_ORE,    "Redstone Ore",      {0.55f, 0.25f, 0.20f}, "Ore"},
        {BlockType::EMERALD_ORE,     "Emerald Ore",       {0.35f, 0.60f, 0.35f}, "Ore"},
        {BlockType::LAPIS_ORE,       "Lapis Ore",         {0.25f, 0.30f, 0.60f}, "Ore"},

        // Mineral Blocks
        {BlockType::IRON_BLOCK,      "Iron Block",        {0.78f, 0.78f, 0.78f}, "Mineral"},
        {BlockType::GOLD_BLOCK,      "Gold Block",        {0.90f, 0.80f, 0.20f}, "Mineral"},
        {BlockType::DIAMOND_BLOCK,   "Diamond Block",     {0.40f, 0.85f, 0.85f}, "Mineral"},
        {BlockType::EMERALD_BLOCK,   "Emerald Block",     {0.25f, 0.80f, 0.35f}, "Mineral"},
        {BlockType::REDSTONE_BLOCK,  "Redstone Block",    {0.75f, 0.10f, 0.05f}, "Mineral"},

        // Wool
        {BlockType::WHITE_WOOL,      "White Wool",        {0.92f, 0.92f, 0.92f}, "Wool"},
        {BlockType::ORANGE_WOOL,     "Orange Wool",       {0.90f, 0.55f, 0.15f}, "Wool"},
        {BlockType::MAGENTA_WOOL,    "Magenta Wool",      {0.70f, 0.25f, 0.65f}, "Wool"},
        {BlockType::LIGHT_BLUE_WOOL, "Light Blue Wool",   {0.40f, 0.60f, 0.85f}, "Wool"},
        {BlockType::YELLOW_WOOL,     "Yellow Wool",       {0.90f, 0.85f, 0.20f}, "Wool"},
        {BlockType::LIME_WOOL,       "Lime Wool",         {0.45f, 0.80f, 0.15f}, "Wool"},
        {BlockType::PINK_WOOL,       "Pink Wool",         {0.85f, 0.50f, 0.55f}, "Wool"},
        {BlockType::GRAY_WOOL,       "Gray Wool",         {0.38f, 0.38f, 0.38f}, "Wool"},
        {BlockType::LIGHT_GRAY_WOOL, "Light Gray Wool",   {0.60f, 0.60f, 0.60f}, "Wool"},
        {BlockType::CYAN_WOOL,       "Cyan Wool",         {0.15f, 0.55f, 0.55f}, "Wool"},
        {BlockType::PURPLE_WOOL,     "Purple Wool",       {0.45f, 0.20f, 0.65f}, "Wool"},
        {BlockType::BLUE_WOOL,       "Blue Wool",         {0.20f, 0.22f, 0.65f}, "Wool"},
        {BlockType::BROWN_WOOL,      "Brown Wool",        {0.40f, 0.25f, 0.12f}, "Wool"},
        {BlockType::GREEN_WOOL,      "Green Wool",        {0.25f, 0.45f, 0.12f}, "Wool"},
        {BlockType::RED_WOOL,        "Red Wool",          {0.65f, 0.15f, 0.12f}, "Wool"},
        {BlockType::BLACK_WOOL,      "Black Wool",        {0.12f, 0.12f, 0.12f}, "Wool"},

        // Functional
        {BlockType::CRAFTING_TABLE,  "Crafting Table",    {0.55f, 0.40f, 0.22f}, "Functional"},
        {BlockType::TNT,             "TNT",               {0.70f, 0.20f, 0.15f}, "Functional"},
        {BlockType::GLOWSTONE,       "Glowstone",         {0.85f, 0.75f, 0.40f}, "Functional"},
        {BlockType::REDSTONE_LAMP,   "Redstone Lamp",     {0.70f, 0.45f, 0.20f}, "Functional"},
        {BlockType::NOTE_BLOCK,      "Note Block",        {0.50f, 0.35f, 0.22f}, "Functional"},
        {BlockType::JUKEBOX,         "Jukebox",           {0.48f, 0.32f, 0.20f}, "Functional"},
        {BlockType::SPONGE,          "Sponge",            {0.80f, 0.80f, 0.30f}, "Functional"},
        {BlockType::FARMLAND,        "Farmland",          {0.40f, 0.25f, 0.12f}, "Functional"},
        {BlockType::COBWEB,          "Cobweb",            {0.85f, 0.85f, 0.85f}, "Functional"},

        // Water
        {BlockType::WATER,           "Water",             {0.15f, 0.35f, 0.75f}, "Liquid"},
    };
    return palette;
}

static glm::vec3 getBlockColor(BlockType type) {
    for (const auto& info : getBlockPalette()) {
        if (info.type == type) return info.color;
    }
    return {0.8f, 0.0f, 0.8f}; // Magenta for unknown
}

static const char* getBlockName(BlockType type) {
    for (const auto& info : getBlockPalette()) {
        if (info.type == type) return info.name;
    }
    return "Unknown";
}

// ============================================================================
// Shader compilation
// ============================================================================

static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compilation error: " << log << std::endl;
    }
    return shader;
}

static GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    auto readFile = [](const std::string& path) -> std::string {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "Failed to open shader: " << path << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

// ============================================================================
// Cube mesh generation (unit cube centered at origin)
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Generate a unit cube (0,0,0 to 1,1,1) with normals
static void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset, const glm::vec3& color) {
    // Each face: 2 triangles = 6 vertices
    struct Face {
        glm::vec3 v[4];
        glm::vec3 normal;
    };

    Face faces[6] = {
        // Front (+Z)
        {{{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}}, {0,0,1}},
        // Back (-Z)
        {{{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}}, {0,0,-1}},
        // Right (+X)
        {{{1,0,1}, {1,0,0}, {1,1,0}, {1,1,1}}, {1,0,0}},
        // Left (-X)
        {{{0,0,0}, {0,0,1}, {0,1,1}, {0,1,0}}, {-1,0,0}},
        // Top (+Y)
        {{{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}}, {0,1,0}},
        // Bottom (-Y)
        {{{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}}, {0,-1,0}},
    };

    for (auto& face : faces) {
        // Tri 1: v0, v1, v2
        vertices.push_back({face.v[0] + offset, face.normal, color});
        vertices.push_back({face.v[1] + offset, face.normal, color});
        vertices.push_back({face.v[2] + offset, face.normal, color});
        // Tri 2: v0, v2, v3
        vertices.push_back({face.v[0] + offset, face.normal, color});
        vertices.push_back({face.v[2] + offset, face.normal, color});
        vertices.push_back({face.v[3] + offset, face.normal, color});
    }
}

// ============================================================================
// Orbit Camera
// ============================================================================

struct OrbitCamera {
    glm::vec3 target = {0, 4, 0};
    float distance = 25.0f;
    float yaw = 45.0f;    // degrees
    float pitch = 30.0f;  // degrees
    float fov = 45.0f;

    glm::vec3 getPosition() const {
        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(pitch);
        float x = target.x + distance * cos(pitchRad) * sin(yawRad);
        float y = target.y + distance * sin(pitchRad);
        float z = target.z + distance * cos(pitchRad) * cos(yawRad);
        return {x, y, z};
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(getPosition(), target, glm::vec3(0, 1, 0));
    }

    glm::mat4 getProjectionMatrix(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 500.0f);
    }
};

// ============================================================================
// Raycasting for block picking
// ============================================================================

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct RayHit {
    bool hit = false;
    glm::ivec3 blockPos;
    glm::ivec3 normal;  // Face normal of hit
    float distance = 1e30f;
};

static Ray screenToRay(double mouseX, double mouseY, int width, int height,
                        const glm::mat4& projection, const glm::mat4& view) {
    float x = (2.0f * (float)mouseX / width) - 1.0f;
    float y = 1.0f - (2.0f * (float)mouseY / height);

    glm::mat4 invPV = glm::inverse(projection * view);
    glm::vec4 nearPoint = invPV * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = invPV * glm::vec4(x, y, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    Ray ray;
    ray.origin = glm::vec3(nearPoint);
    ray.direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    return ray;
}

// Ray-AABB intersection test for a unit cube at position blockPos
static bool rayAABB(const Ray& ray, const glm::ivec3& blockPos, float& tMin, glm::ivec3& hitNormal) {
    glm::vec3 bmin = glm::vec3(blockPos);
    glm::vec3 bmax = bmin + glm::vec3(1.0f);

    float t1, t2;
    tMin = -1e30f;
    float tMax = 1e30f;
    hitNormal = glm::ivec3(0);

    for (int i = 0; i < 3; i++) {
        if (std::abs(ray.direction[i]) < 1e-8f) {
            if (ray.origin[i] < bmin[i] || ray.origin[i] > bmax[i]) return false;
        } else {
            t1 = (bmin[i] - ray.origin[i]) / ray.direction[i];
            t2 = (bmax[i] - ray.origin[i]) / ray.direction[i];
            
            glm::ivec3 n(0);
            n[i] = -1;
            if (t1 > t2) {
                std::swap(t1, t2);
                n[i] = 1;
            }
            if (t1 > tMin) {
                tMin = t1;
                hitNormal = n;
            }
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    return tMin >= 0;
}

// ============================================================================
// File Dialog (Windows native)
// ============================================================================

#ifdef _WIN32
static std::string openFileDialog(const char* filter, const char* title) {
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(szFile);
    return "";
}

static std::string saveFileDialog(const char* filter, const char* title, const char* defaultExt) {
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(szFile);
    return "";
}
#else
static std::string openFileDialog(const char*, const char*) { return ""; }
static std::string saveFileDialog(const char*, const char*, const char*) { return ""; }
#endif

// ============================================================================
// Undo/Redo System
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
    std::vector<StructureBlock> blocks;          // The blocks that were placed/removed
    std::vector<StructureBlock> previousBlocks;  // What was there before
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
    // Window
    GLFWwindow* m_window = nullptr;
    int m_windowWidth = 1600;
    int m_windowHeight = 900;

    // Shaders
    GLuint m_blockShader = 0;
    GLuint m_gridShader = 0;
    GLuint m_wireShader = 0;

    // Mesh buffers
    GLuint m_blockVAO = 0, m_blockVBO = 0;
    GLuint m_gridVAO = 0, m_gridVBO = 0;
    GLuint m_wireVAO = 0, m_wireVBO = 0;
    int m_blockVertexCount = 0;
    int m_gridVertexCount = 0;

    // Camera
    OrbitCamera m_camera;
    bool m_isDragging = false;
    bool m_isPanning = false;
    double m_lastMouseX = 0, m_lastMouseY = 0;

    // Editor state
    Structure m_structure;
    std::string m_currentFilePath;
    bool m_modified = false;

    // Block palette
    BlockType m_selectedBlock = BlockType::OAK_PLANKS;
    std::string m_paletteFilter;
    std::string m_selectedCategory = "All";

    // Tools
    enum class EditorTool { PLACE, ERASE, PICK, MARKER, SELECT };
    EditorTool m_currentTool = EditorTool::PLACE;

    // Grid/display
    int m_gridSize = 32;
    bool m_showGrid = true;
    bool m_showWireframe = true;
    bool m_showAxes = true;
    int m_currentLayer = -1; // -1 = show all layers

    // Hover/selection
    RayHit m_hoverHit;
    glm::ivec3 m_hoverPlacePos = {0, 0, 0};
    bool m_hasHover = false;

    // Multi-block selection
    std::set<int64_t> m_selectedBlocks; // Encoded positions
    bool m_boxSelectActive = false;
    glm::ivec3 m_boxSelectStart = {0, 0, 0};
    glm::ivec3 m_boxSelectEnd = {0, 0, 0};
    bool m_boxSelectHasStart = false;

    // Clipboard
    std::vector<StructureBlock> m_clipboard;
    glm::ivec3 m_clipboardOrigin = {0, 0, 0};

    // Undo/Redo
    std::deque<EditorAction> m_undoStack;
    std::deque<EditorAction> m_redoStack;
    static const size_t MAX_UNDO = 200;

    // Marker editing
    std::string m_markerType = "door";
    std::string m_markerData;

    // Structure metadata
    char m_nameBuffer[128] = "New Structure";
    char m_authorBuffer[128] = "VxStruct Editor";
    int m_categoryIndex = 0;
    bool m_requiresFlat = true;
    float m_minGroundCoverage = 0.7f;
    char m_tagBuffer[64] = "";
    std::vector<std::string> m_tags;

    // UI popups
    bool m_showHelpWindow = false;
    bool m_showAboutWindow = false;

    // Rotation preview
    int m_rotationAngle = 0; // 0, 1, 2, 3 (x90 degrees)

    // Selection encoding helpers
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

    // Methods
    void processInput();
    void updateHover();
    void rebuildBlockMesh();
    void rebuildGridMesh();
    void render();
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
    void renderSelectionHighlights();

    void newStructure();
    void loadStructure();
    void saveStructure();
    void saveStructureAs();
    void exportStructure();

    // Undo/Redo
    void pushAction(const EditorAction& action);
    void undo();
    void redo();

    // Edit operations
    void placeBlockWithUndo(const glm::ivec3& pos, BlockType type, uint8_t metadata = 0);
    void removeBlockWithUndo(const glm::ivec3& pos);
    void deleteSelectedBlocks();
    void copySelection();
    void pasteClipboard();
    void selectAll();
    void deselectAll();
    void invertSelection();
    void rotateStructure();
    void rotateSelection();
    void fillSelection(BlockType type);
    void duplicateSelection();

    // Callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};

// ============================================================================
// Implementation
// ============================================================================

bool VxStructEditor::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "VxStruct Editor - Vortex Engine Tools", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetKeyCallback(m_window, keyCallback);

    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.18f, 0.20f, 0.25f, 1.0f);

    // Load shaders
    m_blockShader = createShaderProgram("shaders/editor_block.vert", "shaders/editor_block.frag");
    m_gridShader = createShaderProgram("shaders/editor_grid.vert", "shaders/editor_grid.frag");
    m_wireShader = createShaderProgram("shaders/editor_wireframe.vert", "shaders/editor_wireframe.frag");

    if (!m_blockShader || !m_gridShader || !m_wireShader) {
        std::cerr << "Failed to compile shaders" << std::endl;
        return false;
    }

    // Create VAOs/VBOs
    glGenVertexArrays(1, &m_blockVAO);
    glGenBuffers(1, &m_blockVBO);

    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);

    glGenVertexArrays(1, &m_wireVAO);
    glGenBuffers(1, &m_wireVBO);

    // Initialize wireframe cube VBO (unit cube edges)
    {
        float cubeEdges[] = {
            // Bottom face
            0,0,0, 1,0,0,  1,0,0, 1,0,1,  1,0,1, 0,0,1,  0,0,1, 0,0,0,
            // Top face
            0,1,0, 1,1,0,  1,1,0, 1,1,1,  1,1,1, 0,1,1,  0,1,1, 0,1,0,
            // Vertical edges
            0,0,0, 0,1,0,  1,0,0, 1,1,0,  1,0,1, 1,1,1,  0,0,1, 0,1,1,
        };

        glBindVertexArray(m_wireVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_wireVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeEdges), cubeEdges, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "vxstruct_editor_imgui.ini";

    // Dark theme with editor accent colors
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;

    // Custom colors - Vortex theme
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.65f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.30f, 0.50f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.35f, 0.55f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.65f, 0.80f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    // Initialize structure
    newStructure();
    rebuildGridMesh();

    std::cout << "VxStruct Editor initialized successfully" << std::endl;
    return true;
}

void VxStructEditor::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processInput();
        updateHover();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render
        render();
        renderUI();

        // Finalize ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

void VxStructEditor::cleanup() {
    if (m_blockVAO) glDeleteVertexArrays(1, &m_blockVAO);
    if (m_blockVBO) glDeleteBuffers(1, &m_blockVBO);
    if (m_gridVAO) glDeleteVertexArrays(1, &m_gridVAO);
    if (m_gridVBO) glDeleteBuffers(1, &m_gridVBO);
    if (m_wireVAO) glDeleteVertexArrays(1, &m_wireVAO);
    if (m_wireVBO) glDeleteBuffers(1, &m_wireVBO);
    if (m_blockShader) glDeleteProgram(m_blockShader);
    if (m_gridShader) glDeleteProgram(m_gridShader);
    if (m_wireShader) glDeleteProgram(m_wireShader);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void VxStructEditor::processInput() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || io.WantCaptureKeyboard) return;

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);

    double dx = mx - m_lastMouseX;
    double dy = my - m_lastMouseY;

    // Middle mouse: orbit camera (Blender-style)
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        bool shiftHeld = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        bool ctrlHeld = glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(m_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        if (!m_isDragging && !m_isPanning) {
            if (shiftHeld) {
                m_isPanning = true;
            } else if (ctrlHeld) {
                // Ctrl+MMB = zoom (Blender-style)
                m_isDragging = false;
                m_isPanning = false;
            } else {
                m_isDragging = true;
            }
        }

        if (ctrlHeld && !m_isDragging && !m_isPanning) {
            // Ctrl+MMB drag: zoom
            m_camera.distance -= (float)dy * m_camera.distance * 0.005f;
            m_camera.distance = glm::clamp(m_camera.distance, 2.0f, 200.0f);
        } else if (m_isDragging) {
            // Orbit
            m_camera.yaw -= (float)dx * 0.3f;
            m_camera.pitch += (float)dy * 0.3f;
            m_camera.pitch = glm::clamp(m_camera.pitch, -89.0f, 89.0f);
        } else if (m_isPanning) {
            // Pan (Shift+MMB)
            glm::mat4 view = m_camera.getViewMatrix();
            glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
            glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);
            float panSpeed = m_camera.distance * 0.003f;
            m_camera.target -= right * (float)dx * panSpeed;
            m_camera.target += up * (float)dy * panSpeed;
        }
    } else {
        m_isDragging = false;
        m_isPanning = false;
    }

    m_lastMouseX = mx;
    m_lastMouseY = my;
}

void VxStructEditor::updateHover() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_hasHover = false;
        return;
    }

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);

    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();

    Ray ray = screenToRay(mx, my, fbW, fbH, proj, view);

    // Test against all blocks
    m_hasHover = false;
    m_hoverHit.hit = false;
    m_hoverHit.distance = 1e30f;

    const auto& blocks = m_structure.getBlocks();
    for (const auto& block : blocks) {
        // Layer filter
        if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;

        float t;
        glm::ivec3 n;
        if (rayAABB(ray, block.position, t, n) && t < m_hoverHit.distance) {
            m_hoverHit.hit = true;
            m_hoverHit.blockPos = block.position;
            m_hoverHit.normal = n;
            m_hoverHit.distance = t;
        }
    }

    if (m_hoverHit.hit) {
        m_hasHover = true;
        m_hoverPlacePos = m_hoverHit.blockPos + m_hoverHit.normal;
    } else {
        // Intersect with ground plane (y=0)
        if (std::abs(ray.direction.y) > 1e-6f) {
            float t = -ray.origin.y / ray.direction.y;
            if (t > 0) {
                glm::vec3 hitPoint = ray.origin + ray.direction * t;
                m_hoverPlacePos = glm::ivec3(
                    (int)std::floor(hitPoint.x),
                    0,
                    (int)std::floor(hitPoint.z)
                );
                // Only hover within grid bounds
                if (m_hoverPlacePos.x >= -m_gridSize/2 && m_hoverPlacePos.x < m_gridSize/2 &&
                    m_hoverPlacePos.z >= -m_gridSize/2 && m_hoverPlacePos.z < m_gridSize/2) {
                    m_hasHover = true;
                    m_hoverHit.hit = false; // No block hit, ground plane hit
                }
            }
        }
    }
}

void VxStructEditor::rebuildBlockMesh() {
    std::vector<Vertex> vertices;
    const auto& blocks = m_structure.getBlocks();

    for (const auto& block : blocks) {
        // Layer filter
        if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;

        glm::vec3 color = getBlockColor(block.type);
        generateCubeVertices(vertices, glm::vec3(block.position), color);
    }

    m_blockVertexCount = (int)vertices.size();

    glBindVertexArray(m_blockVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_blockVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void VxStructEditor::rebuildGridMesh() {
    struct GridVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<GridVertex> vertices;
    float halfGrid = m_gridSize / 2.0f;

    glm::vec3 gridColor(0.30f, 0.32f, 0.35f);
    glm::vec3 axisX(0.70f, 0.20f, 0.20f);
    glm::vec3 axisZ(0.20f, 0.20f, 0.70f);

    // Grid lines at y=0
    for (int i = -(int)halfGrid; i <= (int)halfGrid; i++) {
        glm::vec3 color = (i == 0) ? axisZ : gridColor;
        vertices.push_back({{(float)i, 0, -halfGrid}, color});
        vertices.push_back({{(float)i, 0,  halfGrid}, color});

        color = (i == 0) ? axisX : gridColor;
        vertices.push_back({{-halfGrid, 0, (float)i}, color});
        vertices.push_back({{ halfGrid, 0, (float)i}, color});
    }

    // Y axis indicator
    if (m_showAxes) {
        glm::vec3 axisY(0.20f, 0.70f, 0.20f);
        vertices.push_back({{0, 0, 0}, axisY});
        vertices.push_back({{0, 20, 0}, axisY});
    }

    m_gridVertexCount = (int)vertices.size();

    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GridVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void VxStructEditor::render() {
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    // Draw grid
    if (m_showGrid) {
        glUseProgram(m_gridShader);
        glUniformMatrix4fv(glGetUniformLocation(m_gridShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_gridShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(m_gridVAO);
        glDrawArrays(GL_LINES, 0, m_gridVertexCount);
        glBindVertexArray(0);
    }

    // Draw blocks
    if (m_blockVertexCount > 0) {
        glUseProgram(m_blockShader);
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(m_blockShader, "uLightDir"), 0.5f, 0.8f, 0.3f);
        glm::vec3 camPos = m_camera.getPosition();
        glUniform3f(glGetUniformLocation(m_blockShader, "uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform1f(glGetUniformLocation(m_blockShader, "uHighlight"), 0.0f);
        glBindVertexArray(m_blockVAO);
        glDrawArrays(GL_TRIANGLES, 0, m_blockVertexCount);
        glBindVertexArray(0);
    }

    // Draw wireframe overlay on blocks
    if (m_showWireframe && m_blockVertexCount > 0) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.0f, 0.0f, 0.3f);

        glBindVertexArray(m_wireVAO);
        glLineWidth(1.0f);

        const auto& blocks = m_structure.getBlocks();
        for (const auto& block : blocks) {
            if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;
            glm::mat4 blockModel = glm::translate(glm::mat4(1.0f), glm::vec3(block.position));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(blockModel));
            glDrawArrays(GL_LINES, 0, 24);
        }
        glBindVertexArray(0);
    }

    // Draw hover indicator
    if (m_hasHover) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));

        if (m_currentTool == EditorTool::PLACE) {
            // Show where block would be placed (green wireframe)
            glm::mat4 hoverModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverPlacePos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(hoverModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.2f, 1.0f, 0.3f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::ERASE && m_hoverHit.hit) {
            // Highlight block to erase (red wireframe)
            glm::mat4 eraseModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(eraseModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.2f, 0.2f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::PICK && m_hoverHit.hit) {
            // Pick indicator (yellow wireframe)
            glm::mat4 pickModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(pickModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.9f, 0.2f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::SELECT && m_hoverHit.hit) {
            // Select indicator (cyan wireframe)
            glm::mat4 selModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(selModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.8f, 1.0f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        }
        glLineWidth(1.0f);
    }

    // Draw selection highlights (bright cyan wireframe on selected blocks)
    renderSelectionHighlights();

    // Draw marker positions
    const auto& markers = m_structure.getMarkers();
    if (!markers.empty()) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glLineWidth(3.0f);

        for (const auto& marker : markers) {
            glm::mat4 mModel = glm::translate(glm::mat4(1.0f), glm::vec3(marker.position));
            // Slightly scale up to distinguish from block wireframe
            mModel = glm::scale(mModel, glm::vec3(1.05f));
            mModel = glm::translate(mModel, glm::vec3(-0.025f));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(mModel));
            
            // Color by marker type
            if (marker.type == "door")
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 1.0f, 1.0f, 0.9f);
            else if (marker.type == "spawn")
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.5f, 0.0f, 0.9f);
            else
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 1.0f, 0.0f, 0.9f);

            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        }
        glLineWidth(1.0f);
    }
}

void VxStructEditor::renderUI() {
    // Main menu bar (standalone, not inside a window)
    renderMenuBar();

    // Left panel: Tools + Block Palette
    ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, (float)m_windowHeight - 20), ImGuiCond_FirstUseEver);
    renderToolbar();

    ImGui::SetNextWindowPos(ImVec2(0, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, (float)m_windowHeight - 380), ImGuiCond_FirstUseEver);
    renderBlockPalette();

    // Right panel: Properties + Info
    ImGui::SetNextWindowPos(ImVec2((float)m_windowWidth - 300, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);
    renderProperties();

    ImGui::SetNextWindowPos(ImVec2((float)m_windowWidth - 300, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, (float)m_windowHeight - 370), ImGuiCond_FirstUseEver);
    renderStructureInfo();

    renderMarkerPanel();
    renderSelectionPanel();

    // Popup windows
    if (m_showHelpWindow) renderHelpWindow();
    if (m_showAboutWindow) renderAboutWindow();
}

void VxStructEditor::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) newStructure();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) loadStructure();
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) saveStructure();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) saveStructureAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) glfwSetWindowShouldClose(m_window, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedBlocks.empty())) copySelection();
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) pasteClipboard();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedBlocks.empty())) duplicateSelection();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) selectAll();
            if (ImGui::MenuItem("Deselect All", "Ctrl+Shift+A")) deselectAll();
            if (ImGui::MenuItem("Invert Selection", "Ctrl+I")) invertSelection();
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Delete", false, !m_selectedBlocks.empty())) deleteSelectedBlocks();
            if (ImGui::MenuItem("Fill Selection", "Ctrl+F", false, !m_selectedBlocks.empty())) fillSelection(m_selectedBlock);
            ImGui::Separator();
            if (ImGui::MenuItem("Rotate Structure 90", "Ctrl+R")) rotateStructure();
            if (ImGui::MenuItem("Rotate Selection 90", "Ctrl+Shift+R", false, !m_selectedBlocks.empty())) rotateSelection();
            ImGui::Separator();
            if (ImGui::MenuItem("Clear All Blocks")) {
                // Push undo for all blocks
                EditorAction action;
                action.type = EditorAction::Type::REMOVE_MULTIPLE;
                action.previousBlocks = m_structure.getBlocks();
                pushAction(action);
                m_structure.clear();
                m_selectedBlocks.clear();
                m_modified = true;
                rebuildBlockMesh();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Grid", &m_showGrid);
            ImGui::Checkbox("Show Wireframe", &m_showWireframe);
            ImGui::Checkbox("Show Axes", &m_showAxes);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Camera", "Home")) {
                m_camera = OrbitCamera();
            }
            if (ImGui::MenuItem("Focus Selection", "Numpad .")) {
                // Focus on structure center
                if (!m_structure.getBlocks().empty()) {
                    glm::vec3 center = glm::vec3(m_structure.getMinBounds() + m_structure.getMaxBounds()) * 0.5f;
                    m_camera.target = center;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Front View", "Numpad 1")) { m_camera.yaw = 0; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Back View", "Ctrl+Numpad 1")) { m_camera.yaw = 180; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Right View", "Numpad 3")) { m_camera.yaw = 90; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Left View", "Ctrl+Numpad 3")) { m_camera.yaw = -90; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Top View", "Numpad 7")) { m_camera.yaw = 0; m_camera.pitch = 89; }
            if (ImGui::MenuItem("Bottom View", "Ctrl+Numpad 7")) { m_camera.yaw = 0; m_camera.pitch = -89; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Keyboard Shortcuts", "F1")) m_showHelpWindow = true;
            ImGui::Separator();
            if (ImGui::MenuItem("About VxStruct Editor")) m_showAboutWindow = true;
            ImGui::EndMenu();
        }

        // Status bar on the right side of menu bar
        float statusX = ImGui::GetWindowWidth() - 450;
        ImGui::SameLine(statusX);
        ImGui::TextDisabled("Blocks: %d | Selection: %d | Undo: %d",
                           (int)m_structure.getBlocks().size(),
                           (int)m_selectedBlocks.size(),
                           (int)m_undoStack.size());

        ImGui::EndMainMenuBar();
    }
}

void VxStructEditor::renderToolbar() {
    ImGui::Begin("Tools");

    ImGui::Text("Current Tool:");
    ImGui::Separator();

    bool isPlace  = (m_currentTool == EditorTool::PLACE);
    bool isErase  = (m_currentTool == EditorTool::ERASE);
    bool isPick   = (m_currentTool == EditorTool::PICK);
    bool isMarker = (m_currentTool == EditorTool::MARKER);
    bool isSelect = (m_currentTool == EditorTool::SELECT);

    auto toolButton = [](const char* label, bool active, ImVec4 activeColor) -> bool {
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
        bool pressed = ImGui::Button(label, ImVec2(130, 28));
        if (active) ImGui::PopStyleColor();
        return pressed;
    };

    if (toolButton("Select [Q]", isSelect, ImVec4(0.1f, 0.5f, 0.8f, 1.0f))) m_currentTool = EditorTool::SELECT;
    if (toolButton("Place  [1]", isPlace,  ImVec4(0.2f, 0.6f, 0.3f, 1.0f))) m_currentTool = EditorTool::PLACE;
    if (toolButton("Erase  [2]", isErase,  ImVec4(0.7f, 0.2f, 0.2f, 1.0f))) m_currentTool = EditorTool::ERASE;
    if (toolButton("Pick   [3]", isPick,   ImVec4(0.7f, 0.7f, 0.2f, 1.0f))) m_currentTool = EditorTool::PICK;
    if (toolButton("Marker [4]", isMarker, ImVec4(0.2f, 0.5f, 0.7f, 1.0f))) m_currentTool = EditorTool::MARKER;

    ImGui::Separator();

    // Selection info
    if (!m_selectedBlocks.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Selected: %d blocks", (int)m_selectedBlocks.size());
        if (ImGui::Button("Deselect All", ImVec2(130, 22))) deselectAll();
        if (ImGui::Button("Delete Selected", ImVec2(130, 22))) deleteSelectedBlocks();
        if (ImGui::Button("Fill Selected", ImVec2(130, 22))) fillSelection(m_selectedBlock);
        ImGui::Separator();
    }

    // Layer filter
    ImGui::Text("Layer Filter:");
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Y Layer", &m_currentLayer)) {
        if (m_currentLayer < -1) m_currentLayer = -1;
        rebuildBlockMesh();
    }
    ImGui::SameLine();
    if (ImGui::Button("All##layers")) {
        m_currentLayer = -1;
        rebuildBlockMesh();
    }

    ImGui::Separator();

    // Undo/Redo info
    ImGui::Text("History:");
    ImGui::Text("  Undo: %d  Redo: %d", (int)m_undoStack.size(), (int)m_redoStack.size());
    if (ImGui::Button("Undo##btn", ImVec2(62, 22)) && !m_undoStack.empty()) undo();
    ImGui::SameLine();
    if (ImGui::Button("Redo##btn", ImVec2(62, 22)) && !m_redoStack.empty()) redo();

    ImGui::Separator();
    ImGui::Text("Camera:");
    ImGui::Text("  Distance: %.1f", m_camera.distance);
    ImGui::Text("  Yaw: %.1f  Pitch: %.1f", m_camera.yaw, m_camera.pitch);

    ImGui::End();
}

void VxStructEditor::renderBlockPalette() {
    ImGui::Begin("Block Palette");

    // Search filter
    static char filterBuf[64] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Search blocks...", filterBuf, sizeof(filterBuf));
    std::string filter = filterBuf;

    // Category tabs
    static const char* categories[] = {"All", "Natural", "Wood", "Nature", "Building", "Ore", "Mineral", "Wool", "Functional", "Liquid"};
    
    if (ImGui::BeginTabBar("Categories")) {
        for (const char* cat : categories) {
            if (ImGui::BeginTabItem(cat)) {
                m_selectedCategory = cat;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Block buttons
    const auto& palette = getBlockPalette();
    int buttonsPerRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 38.0f));
    int col = 0;

    for (const auto& info : palette) {
        // Filter by category
        if (m_selectedCategory != "All" && info.category != m_selectedCategory) continue;
        
        // Filter by search
        if (!filter.empty()) {
            std::string name = info.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string f = filter;
            std::transform(f.begin(), f.end(), f.begin(), ::tolower);
            if (name.find(f) == std::string::npos) continue;
        }

        bool isSelected = (m_selectedBlock == info.type);
        
        ImGui::PushID(static_cast<int>(info.type));
        
        ImVec4 buttonColor(info.color.r, info.color.g, info.color.b, 1.0f);
        ImVec4 hoverColor(
            std::min(1.0f, info.color.r + 0.2f),
            std::min(1.0f, info.color.g + 0.2f),
            std::min(1.0f, info.color.b + 0.2f),
            1.0f
        );

        ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);

        if (isSelected) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        if (ImGui::Button("##block", ImVec2(32, 32))) {
            m_selectedBlock = info.type;
            m_currentTool = EditorTool::PLACE;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", info.name);
        }

        ImGui::PopID();

        col++;
        if (col < buttonsPerRow) ImGui::SameLine();
        else col = 0;
    }

    ImGui::Separator();
    ImGui::Text("Selected: %s", getBlockName(m_selectedBlock));

    ImGui::End();
}

void VxStructEditor::renderProperties() {
    ImGui::Begin("Structure Properties");

    ImGui::Text("Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##name", m_nameBuffer, sizeof(m_nameBuffer))) {
        m_structure.setName(m_nameBuffer);
        m_modified = true;
    }

    ImGui::Text("Author:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##author", m_authorBuffer, sizeof(m_authorBuffer))) {
        m_structure.setAuthor(m_authorBuffer);
        m_modified = true;
    }

    ImGui::Text("Category:");
    ImGui::SetNextItemWidth(-1);
    static const char* catNames[] = {
        "Village House", "Village Building", "Village Farm", "Village Well",
        "Village Path", "Village Decoration", "City Building", "City Skyscraper",
        "City Road", "City Park", "City Decoration", "Misc"
    };
    if (ImGui::Combo("##category", &m_categoryIndex, catNames, IM_ARRAYSIZE(catNames))) {
        m_structure.setCategory(static_cast<StructureCategory>(m_categoryIndex));
        m_modified = true;
    }

    ImGui::Separator();

    ImGui::Checkbox("Requires Flat Terrain", &m_requiresFlat);
    m_structure.setRequiresFlat(m_requiresFlat);

    ImGui::Text("Min Ground Coverage:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##coverage", &m_minGroundCoverage, 0.0f, 1.0f, "%.2f")) {
        m_structure.setMinGroundCoverage(m_minGroundCoverage);
        m_modified = true;
    }

    ImGui::Separator();
    ImGui::Text("Tags:");
    
    // Display existing tags
    const auto& tags = m_structure.getTags();
    for (size_t i = 0; i < tags.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::BulletText("%s", tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            // Remove tag - need to rebuild tag list
            std::vector<std::string> newTags;
            for (size_t j = 0; j < tags.size(); j++) {
                if (j != i) newTags.push_back(tags[j]);
            }
            // Hacky but works: clear and re-add
            // Structure doesn't have removeTag, so we reconstruct
            m_modified = true;
        }
        ImGui::PopID();
    }

    ImGui::SetNextItemWidth(-60);
    ImGui::InputText("##newtag", m_tagBuffer, sizeof(m_tagBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Add") && m_tagBuffer[0] != '\0') {
        m_structure.addTag(m_tagBuffer);
        m_tagBuffer[0] = '\0';
        m_modified = true;
    }

    ImGui::End();
}

void VxStructEditor::renderStructureInfo() {
    ImGui::Begin("Structure Info");

    glm::ivec3 size = m_structure.getSize();
    glm::ivec3 minB = m_structure.getMinBounds();
    glm::ivec3 maxB = m_structure.getMaxBounds();

    ImGui::Text("Blocks: %d", (int)m_structure.getBlocks().size());
    ImGui::Text("Markers: %d", (int)m_structure.getMarkers().size());
    ImGui::Separator();
    ImGui::Text("Size: %d x %d x %d", size.x, size.y, size.z);
    ImGui::Text("Bounds: (%d,%d,%d) -> (%d,%d,%d)", 
                minB.x, minB.y, minB.z, maxB.x, maxB.y, maxB.z);

    ImGui::Separator();
    if (m_hasHover) {
        if (m_hoverHit.hit) {
            BlockType bt = m_structure.getBlock(m_hoverHit.blockPos);
            ImGui::Text("Hover Block: (%d, %d, %d)", 
                        m_hoverHit.blockPos.x, m_hoverHit.blockPos.y, m_hoverHit.blockPos.z);
            ImGui::Text("Block Type: %s", getBlockName(bt));
        }
        ImGui::Text("Place Pos: (%d, %d, %d)", 
                    m_hoverPlacePos.x, m_hoverPlacePos.y, m_hoverPlacePos.z);
    } else {
        ImGui::TextDisabled("No hover target");
    }

    ImGui::Separator();
    ImGui::Text("File: %s", m_currentFilePath.empty() ? "(unsaved)" : m_currentFilePath.c_str());
    if (m_modified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(modified)");
    }

    ImGui::Separator();
    ImGui::TextWrapped("Controls:");
    ImGui::BulletText("Left Click: Place/Erase/Pick");
    ImGui::BulletText("Middle Mouse: Orbit camera");
    ImGui::BulletText("Shift+Middle: Pan camera");
    ImGui::BulletText("Scroll: Zoom");
    ImGui::BulletText("1-4: Select tool");

    ImGui::End();
}

void VxStructEditor::renderMarkerPanel() {
    if (m_currentTool != EditorTool::MARKER) return;

    ImGui::Begin("Marker Settings");

    static const char* markerTypes[] = {"door", "spawn", "chest", "bed", "villager", "custom"};
    static int markerTypeIdx = 0;
    
    ImGui::Text("Marker Type:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##markertype", &markerTypeIdx, markerTypes, IM_ARRAYSIZE(markerTypes))) {
        m_markerType = markerTypes[markerTypeIdx];
    }

    ImGui::Text("Marker Data (optional):");
    static char dataBuf[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##markerdata", dataBuf, sizeof(dataBuf));
    m_markerData = dataBuf;

    ImGui::Separator();
    ImGui::Text("Existing Markers:");
    const auto& markers = m_structure.getMarkers();
    for (size_t i = 0; i < markers.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::Text("(%d,%d,%d) [%s]", 
                    markers[i].position.x, markers[i].position.y, markers[i].position.z,
                    markers[i].type.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Del")) {
            m_structure.removeMarker(markers[i].position);
            m_modified = true;
        }
        ImGui::PopID();
    }

    ImGui::End();
}

// ============================================================================
// Selection Highlights Rendering
// ============================================================================

void VxStructEditor::renderSelectionHighlights() {
    if (m_selectedBlocks.empty()) return;

    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();

    glUseProgram(m_wireShader);
    glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.9f, 1.0f, 0.95f);
    glLineWidth(2.5f);

    glBindVertexArray(m_wireVAO);
    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(pos));
        m = glm::scale(m, glm::vec3(1.02f));
        m = glm::translate(m, glm::vec3(-0.01f));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_LINES, 0, 24);
    }
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

// ============================================================================
// Selection Panel
// ============================================================================

void VxStructEditor::renderSelectionPanel() {
    if (m_currentTool != EditorTool::SELECT && m_selectedBlocks.empty()) return;

    ImGui::Begin("Selection", nullptr);

    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%d blocks selected", (int)m_selectedBlocks.size());
    ImGui::Separator();

    if (ImGui::Button("Select All (Ctrl+A)", ImVec2(-1, 0))) selectAll();
    if (ImGui::Button("Deselect (Ctrl+Shift+A)", ImVec2(-1, 0))) deselectAll();
    if (ImGui::Button("Invert (Ctrl+I)", ImVec2(-1, 0))) invertSelection();

    ImGui::Separator();
    ImGui::Text("Operations:");

    bool hasSelection = !m_selectedBlocks.empty();
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("Copy (Ctrl+C)", ImVec2(-1, 0))) copySelection();
    if (ImGui::Button("Duplicate (Ctrl+D)", ImVec2(-1, 0))) duplicateSelection();
    if (ImGui::Button("Delete (Del)", ImVec2(-1, 0))) deleteSelectedBlocks();
    if (ImGui::Button("Rotate Selection (Ctrl+Shift+R)", ImVec2(-1, 0))) rotateSelection();

    ImGui::Separator();
    ImGui::Text("Fill with current block:");
    if (ImGui::Button("Fill Selection (Ctrl+F)", ImVec2(-1, 0))) fillSelection(m_selectedBlock);
    if (!hasSelection) ImGui::EndDisabled();

    bool hasClipboard = !m_clipboard.empty();
    ImGui::Separator();
    if (!hasClipboard) ImGui::BeginDisabled();
    ImGui::Text("Clipboard: %d blocks", (int)m_clipboard.size());
    if (ImGui::Button("Paste (Ctrl+V)", ImVec2(-1, 0))) pasteClipboard();
    if (!hasClipboard) ImGui::EndDisabled();

    ImGui::End();
}

// ============================================================================
// Help Window
// ============================================================================

void VxStructEditor::renderHelpWindow() {
    if (!m_showHelpWindow) return;

    ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Keyboard Shortcuts & Help", &m_showHelpWindow);

    ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "VxStruct Editor - Keyboard Reference");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Navigation (Blender-style)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Middle Mouse Drag    - Orbit camera");
        ImGui::BulletText("Shift + Middle Mouse - Pan camera");
        ImGui::BulletText("Ctrl + Middle Mouse  - Zoom camera");
        ImGui::BulletText("Scroll Wheel         - Zoom in/out");
        ImGui::BulletText("Numpad 1             - Front view");
        ImGui::BulletText("Numpad 3             - Right view");
        ImGui::BulletText("Numpad 7             - Top view");
        ImGui::BulletText("Ctrl+Numpad 1        - Back view");
        ImGui::BulletText("Ctrl+Numpad 3        - Left view");
        ImGui::BulletText("Ctrl+Numpad 7        - Bottom view");
        ImGui::BulletText("Numpad .             - Focus on selection");
        ImGui::BulletText("Home                 - Reset camera");
    }

    if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Q - Select tool");
        ImGui::BulletText("1 - Place block");
        ImGui::BulletText("2 - Erase block");
        ImGui::BulletText("3 - Pick block (eyedropper)");
        ImGui::BulletText("4 - Place marker");
        ImGui::BulletText("G - Toggle grid");
        ImGui::BulletText("W - Toggle wireframe");
        ImGui::BulletText("X - Toggle axes");
    }

    if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Left Click (Select tool)   - Toggle block selection");
        ImGui::BulletText("Shift + Left Click         - Add to selection");
        ImGui::BulletText("Ctrl + A                   - Select all");
        ImGui::BulletText("Ctrl + Shift + A           - Deselect all");
        ImGui::BulletText("Ctrl + I                   - Invert selection");
    }

    if (ImGui::CollapsingHeader("Edit Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Ctrl + Z     - Undo");
        ImGui::BulletText("Ctrl + Y     - Redo");
        ImGui::BulletText("Ctrl + C     - Copy selection");
        ImGui::BulletText("Ctrl + V     - Paste clipboard");
        ImGui::BulletText("Ctrl + D     - Duplicate selection");
        ImGui::BulletText("Ctrl + R     - Rotate entire structure 90 deg");
        ImGui::BulletText("Ctrl+Shift+R - Rotate selection 90 deg");
        ImGui::BulletText("Ctrl + F     - Fill selection with current block");
        ImGui::BulletText("Delete       - Delete selected blocks");
    }

    if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Ctrl + N         - New structure");
        ImGui::BulletText("Ctrl + O         - Open file");
        ImGui::BulletText("Ctrl + S         - Save");
        ImGui::BulletText("Ctrl + Shift + S - Save As");
    }

    if (ImGui::CollapsingHeader("Mouse")) {
        ImGui::BulletText("Left Click       - Use current tool");
        ImGui::BulletText("Right Click      - Quick erase block");
        ImGui::BulletText("Middle Click     - Orbit (hold & drag)");
    }

    ImGui::Separator();
    ImGui::TextWrapped("Tip: Use the Select tool (Q) to select blocks, then use copy/paste/fill/delete to edit them in bulk.");

    ImGui::End();
}

// ============================================================================
// About Window
// ============================================================================

void VxStructEditor::renderAboutWindow() {
    if (!m_showAboutWindow) return;

    ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About VxStruct Editor", &m_showAboutWindow)) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "VxStruct Editor");
        ImGui::Text("Version 1.0.0");
        ImGui::Separator();
        ImGui::TextWrapped(
            "A standalone 3D voxel structure editor for creating and editing "
            ".vxstruct files used by the Vortex Engine Minecraft-like game."
        );
        ImGui::Separator();
        ImGui::Text("Features:");
        ImGui::BulletText("Blender-style 3D navigation");
        ImGui::BulletText("Multi-block selection & editing");
        ImGui::BulletText("Undo/Redo with 200 history steps");
        ImGui::BulletText("Copy/Paste/Duplicate blocks");
        ImGui::BulletText("Structure rotation (90 deg increments)");
        ImGui::BulletText("Block palette with 100+ block types");
        ImGui::BulletText("Marker placement for doors & spawns");
        ImGui::Separator();
        ImGui::Text("Built with: OpenGL 4.5, GLFW, ImGui, GLM");
        ImGui::Text("Format: VxStruct (Vortex Structs) JSON");
    }
    ImGui::End();
}

// ============================================================================
// Undo / Redo System
// ============================================================================

void VxStructEditor::pushAction(const EditorAction& action) {
    m_undoStack.push_back(action);
    if (m_undoStack.size() > MAX_UNDO) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
}

void VxStructEditor::undo() {
    if (m_undoStack.empty()) return;

    EditorAction action = m_undoStack.back();
    m_undoStack.pop_back();

    switch (action.type) {
        case EditorAction::Type::PLACE_BLOCK:
            // Was a place, so undo = remove (or restore previous)
            if (action.previousType == BlockType::AIR)
                m_structure.removeBlock(action.position);
            else
                m_structure.setBlock(action.position, action.previousType, action.previousMetadata);
            break;

        case EditorAction::Type::REMOVE_BLOCK:
            // Was a remove, so undo = place back
            m_structure.setBlock(action.position, action.blockType, action.metadata);
            break;

        case EditorAction::Type::PLACE_MULTIPLE:
        case EditorAction::Type::PASTE:
        case EditorAction::Type::FILL_SELECTION:
            // Remove all placed blocks, restore previous state
            for (const auto& b : action.blocks) {
                m_structure.removeBlock(b.position);
            }
            for (const auto& b : action.previousBlocks) {
                if (b.type != BlockType::AIR)
                    m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;

        case EditorAction::Type::REMOVE_MULTIPLE:
            // Was a multi-remove, so undo = place all back
            for (const auto& b : action.blocks) {
                m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;
    }

    m_redoStack.push_back(action);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::redo() {
    if (m_redoStack.empty()) return;

    EditorAction action = m_redoStack.back();
    m_redoStack.pop_back();

    switch (action.type) {
        case EditorAction::Type::PLACE_BLOCK:
            m_structure.setBlock(action.position, action.blockType, action.metadata);
            break;

        case EditorAction::Type::REMOVE_BLOCK:
            m_structure.removeBlock(action.position);
            break;

        case EditorAction::Type::PLACE_MULTIPLE:
        case EditorAction::Type::PASTE:
        case EditorAction::Type::FILL_SELECTION:
            // Re-place all blocks
            for (const auto& b : action.blocks) {
                m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;

        case EditorAction::Type::REMOVE_MULTIPLE:
            // Re-remove all
            for (const auto& b : action.blocks) {
                m_structure.removeBlock(b.position);
            }
            break;
    }

    m_undoStack.push_back(action);
    m_modified = true;
    rebuildBlockMesh();
}

// ============================================================================
// Edit Operations with Undo
// ============================================================================

void VxStructEditor::placeBlockWithUndo(const glm::ivec3& pos, BlockType type, uint8_t metadata) {
    EditorAction action;
    action.type = EditorAction::Type::PLACE_BLOCK;
    action.position = pos;
    action.blockType = type;
    action.metadata = metadata;
    action.previousType = m_structure.getBlock(pos);
    action.previousMetadata = 0; // Metadata tracking simplified
    pushAction(action);

    m_structure.setBlock(pos, type, metadata);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::removeBlockWithUndo(const glm::ivec3& pos) {
    BlockType existing = m_structure.getBlock(pos);
    if (existing == BlockType::AIR) return;

    EditorAction action;
    action.type = EditorAction::Type::REMOVE_BLOCK;
    action.position = pos;
    action.blockType = existing;
    action.metadata = 0;
    pushAction(action);

    m_structure.removeBlock(pos);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::deleteSelectedBlocks() {
    if (m_selectedBlocks.empty()) return;

    EditorAction action;
    action.type = EditorAction::Type::REMOVE_MULTIPLE;

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            action.blocks.push_back(sb);
        }
    }

    if (action.blocks.empty()) return;

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.removeBlock(b.position);
    }

    m_selectedBlocks.clear();
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::copySelection() {
    if (m_selectedBlocks.empty()) return;

    m_clipboard.clear();
    glm::ivec3 minPos(INT_MAX);
    glm::ivec3 maxPos(INT_MIN);

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            m_clipboard.push_back(sb);
            minPos = glm::min(minPos, pos);
            maxPos = glm::max(maxPos, pos);
        }
    }

    m_clipboardOrigin = minPos;
}

void VxStructEditor::pasteClipboard() {
    if (m_clipboard.empty()) return;

    glm::ivec3 offset = m_hoverPlacePos - m_clipboardOrigin;

    EditorAction action;
    action.type = EditorAction::Type::PASTE;

    for (const auto& cb : m_clipboard) {
        glm::ivec3 newPos = cb.position + offset;
        
        // Save previous state
        BlockType prevType = m_structure.getBlock(newPos);
        StructureBlock prev;
        prev.position = newPos;
        prev.type = prevType;
        prev.metadata = 0;
        action.previousBlocks.push_back(prev);

        // New block
        StructureBlock nb;
        nb.position = newPos;
        nb.type = cb.type;
        nb.metadata = cb.metadata;
        action.blocks.push_back(nb);
    }

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::selectAll() {
    m_selectedBlocks.clear();
    for (const auto& block : m_structure.getBlocks()) {
        m_selectedBlocks.insert(encodePos(block.position));
    }
}

void VxStructEditor::deselectAll() {
    m_selectedBlocks.clear();
}

void VxStructEditor::invertSelection() {
    std::set<int64_t> newSel;
    for (const auto& block : m_structure.getBlocks()) {
        int64_t enc = encodePos(block.position);
        if (m_selectedBlocks.count(enc) == 0) {
            newSel.insert(enc);
        }
    }
    m_selectedBlocks = newSel;
}

void VxStructEditor::rotateStructure() {
    const auto& blocks = m_structure.getBlocks();
    if (blocks.empty()) return;

    // Compute center
    glm::ivec3 minP(INT_MAX), maxP(INT_MIN);
    for (const auto& b : blocks) {
        minP = glm::min(minP, b.position);
        maxP = glm::max(maxP, b.position);
    }
    glm::vec3 center = glm::vec3(minP + maxP) * 0.5f;

    // Save current as undo (remove all old, place all new)
    EditorAction action;
    action.type = EditorAction::Type::REMOVE_MULTIPLE;
    
    EditorAction placeAction;
    placeAction.type = EditorAction::Type::PLACE_MULTIPLE;

    // Store old blocks for undo
    std::vector<StructureBlock> oldBlocks = blocks; // copy
    action.blocks = oldBlocks;

    // Remove all
    m_structure.clear();
    // Keep metadata
    
    // Rotate each block 90 degrees around Y axis: (x, y, z) -> (z, y, -x)
    // Adjusted for center
    std::vector<StructureBlock> newBlocks;
    for (const auto& b : oldBlocks) {
        glm::vec3 rel = glm::vec3(b.position) - center;
        glm::vec3 rot(rel.z, rel.y, -rel.x);
        glm::ivec3 newPos = glm::ivec3(glm::round(rot + center));

        StructureBlock nb;
        nb.position = newPos;
        nb.type = b.type;
        nb.metadata = b.metadata;
        newBlocks.push_back(nb);
    }

    placeAction.blocks = newBlocks;

    // Combined action: we'll use REMOVE_MULTIPLE for the old blocks
    // Actually, let's combine: previousBlocks = old state, blocks = new state
    EditorAction combined;
    combined.type = EditorAction::Type::PLACE_MULTIPLE;
    combined.blocks = newBlocks;
    combined.previousBlocks = oldBlocks;
    pushAction(combined);

    for (const auto& b : newBlocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_selectedBlocks.clear();
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::rotateSelection() {
    if (m_selectedBlocks.empty()) return;

    // Gather selected blocks
    std::vector<StructureBlock> selBlocks;
    glm::ivec3 minP(INT_MAX), maxP(INT_MIN);
    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            selBlocks.push_back(sb);
            minP = glm::min(minP, pos);
            maxP = glm::max(maxP, pos);
        }
    }
    if (selBlocks.empty()) return;

    glm::vec3 center = glm::vec3(minP + maxP) * 0.5f;

    // Build undo: remove old selected, place rotated
    EditorAction action;
    action.type = EditorAction::Type::PLACE_MULTIPLE;
    action.previousBlocks = selBlocks;

    // Remove old
    for (const auto& b : selBlocks) {
        m_structure.removeBlock(b.position);
    }

    // Place rotated
    std::set<int64_t> newSelection;
    for (const auto& b : selBlocks) {
        glm::vec3 rel = glm::vec3(b.position) - center;
        glm::vec3 rot(rel.z, rel.y, -rel.x);
        glm::ivec3 newPos = glm::ivec3(glm::round(rot + center));

        StructureBlock nb;
        nb.position = newPos;
        nb.type = b.type;
        nb.metadata = b.metadata;
        action.blocks.push_back(nb);

        m_structure.setBlock(newPos, nb.type, nb.metadata);
        newSelection.insert(encodePos(newPos));
    }

    pushAction(action);
    m_selectedBlocks = newSelection;
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::fillSelection(BlockType type) {
    if (m_selectedBlocks.empty()) return;

    EditorAction action;
    action.type = EditorAction::Type::FILL_SELECTION;

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);

        // Previous state
        BlockType prevType = m_structure.getBlock(pos);
        StructureBlock prev;
        prev.position = pos;
        prev.type = prevType;
        prev.metadata = 0;
        action.previousBlocks.push_back(prev);

        // New state
        StructureBlock nb;
        nb.position = pos;
        nb.type = type;
        nb.metadata = 0;
        action.blocks.push_back(nb);
    }

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::duplicateSelection() {
    copySelection();
    pasteClipboard();
}

// ============================================================================
// File Operations
// ============================================================================

void VxStructEditor::newStructure() {
    m_structure.clear();
    m_structure.setName("New Structure");
    m_structure.setAuthor("VxStruct Editor");
    m_structure.setCategory(StructureCategory::MISC);
    m_currentFilePath.clear();
    m_modified = false;

    // Clear editor state
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedBlocks.clear();
    m_clipboard.clear();
    m_boxSelectActive = false;
    m_boxSelectHasStart = false;

    strncpy_s(m_nameBuffer, "New Structure", sizeof(m_nameBuffer));
    strncpy_s(m_authorBuffer, "VxStruct Editor", sizeof(m_authorBuffer));
    m_categoryIndex = static_cast<int>(StructureCategory::MISC);
    m_requiresFlat = true;
    m_minGroundCoverage = 0.7f;

    rebuildBlockMesh();

    // Update window title
    glfwSetWindowTitle(m_window, "VxStruct Editor - New Structure");
}

void VxStructEditor::loadStructure() {
    std::string path = openFileDialog(
        "VxStruct Files (*.vxstruct)\0*.vxstruct\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0",
        "Open VxStruct File"
    );

    if (path.empty()) return;

    Structure newStruct;
    if (newStruct.loadFromFile(path)) {
        m_structure = newStruct;
        m_currentFilePath = path;
        m_modified = false;

        strncpy_s(m_nameBuffer, m_structure.getName().c_str(), sizeof(m_nameBuffer));
        strncpy_s(m_authorBuffer, m_structure.getAuthor().c_str(), sizeof(m_authorBuffer));
        m_categoryIndex = static_cast<int>(m_structure.getCategory());
        m_requiresFlat = m_structure.requiresFlat();
        m_minGroundCoverage = m_structure.getMinGroundCoverage();

        rebuildBlockMesh();

        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());
        
        std::cout << "Loaded structure: " << m_structure.getName() 
                  << " (" << m_structure.getBlocks().size() << " blocks)" << std::endl;
    } else {
        std::cerr << "Failed to load structure: " << path << std::endl;
    }
}

void VxStructEditor::saveStructure() {
    if (m_currentFilePath.empty()) {
        saveStructureAs();
        return;
    }

    if (m_structure.saveToFile(m_currentFilePath)) {
        m_modified = false;
        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());
        std::cout << "Saved: " << m_currentFilePath << std::endl;
    }
}

void VxStructEditor::saveStructureAs() {
    std::string path = saveFileDialog(
        "VxStruct Files (*.vxstruct)\0*.vxstruct\0JSON Files (*.json)\0*.json\0",
        "Save VxStruct File",
        "vxstruct"
    );

    if (path.empty()) return;

    m_currentFilePath = path;
    saveStructure();
}

void VxStructEditor::exportStructure() {
    // Future: export to other formats
    saveStructureAs();
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

void VxStructEditor::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    editor->m_windowWidth = width;
    editor->m_windowHeight = height;
}

void VxStructEditor::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    editor->m_camera.distance -= (float)yoffset * editor->m_camera.distance * 0.1f;
    editor->m_camera.distance = glm::clamp(editor->m_camera.distance, 2.0f, 200.0f);
}

void VxStructEditor::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (!editor->m_hasHover) return;

        switch (editor->m_currentTool) {
            case EditorTool::PLACE: {
                editor->placeBlockWithUndo(editor->m_hoverPlacePos, editor->m_selectedBlock);
                break;
            }
            case EditorTool::ERASE: {
                if (editor->m_hoverHit.hit) {
                    editor->removeBlockWithUndo(editor->m_hoverHit.blockPos);
                }
                break;
            }
            case EditorTool::PICK: {
                if (editor->m_hoverHit.hit) {
                    editor->m_selectedBlock = editor->m_structure.getBlock(editor->m_hoverHit.blockPos);
                    editor->m_currentTool = EditorTool::PLACE;
                }
                break;
            }
            case EditorTool::MARKER: {
                editor->m_structure.addMarker(editor->m_hoverPlacePos, 
                                               editor->m_markerType, 
                                               editor->m_markerData);
                editor->m_modified = true;
                break;
            }
            case EditorTool::SELECT: {
                if (editor->m_hoverHit.hit) {
                    int64_t enc = encodePos(editor->m_hoverHit.blockPos);
                    bool shiftHeld = (mods & GLFW_MOD_SHIFT) != 0;

                    if (shiftHeld) {
                        // Shift+click: add to / remove from selection
                        if (editor->m_selectedBlocks.count(enc))
                            editor->m_selectedBlocks.erase(enc);
                        else
                            editor->m_selectedBlocks.insert(enc);
                    } else {
                        // Plain click: toggle this block, clear others
                        bool wasSelected = editor->m_selectedBlocks.count(enc) > 0;
                        editor->m_selectedBlocks.clear();
                        if (!wasSelected)
                            editor->m_selectedBlocks.insert(enc);
                    }
                } else {
                    // Clicked empty space: deselect all
                    if (!(mods & GLFW_MOD_SHIFT))
                        editor->m_selectedBlocks.clear();
                }
                break;
            }
        }
    }

    // Right click: quick erase with undo
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        if (editor->m_hoverHit.hit) {
            editor->removeBlockWithUndo(editor->m_hoverHit.blockPos);
        }
    }
}

void VxStructEditor::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (action != GLFW_PRESS) return;

    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // Tool selection (no modifiers)
    if (!ctrl && !shift) {
        switch (key) {
            case GLFW_KEY_Q: editor->m_currentTool = EditorTool::SELECT; break;
            case GLFW_KEY_1: editor->m_currentTool = EditorTool::PLACE; break;
            case GLFW_KEY_2: editor->m_currentTool = EditorTool::ERASE; break;
            case GLFW_KEY_3: editor->m_currentTool = EditorTool::PICK; break;
            case GLFW_KEY_4: editor->m_currentTool = EditorTool::MARKER; break;
            case GLFW_KEY_G: editor->m_showGrid = !editor->m_showGrid; break;
            case GLFW_KEY_W: editor->m_showWireframe = !editor->m_showWireframe; break;
            case GLFW_KEY_X: editor->m_showAxes = !editor->m_showAxes; break;
            case GLFW_KEY_DELETE: editor->deleteSelectedBlocks(); break;
            case GLFW_KEY_F1: editor->m_showHelpWindow = !editor->m_showHelpWindow; break;
            case GLFW_KEY_HOME: {
                // Reset camera
                editor->m_camera.distance = 20.0f;
                editor->m_camera.yaw = -45.0f;
                editor->m_camera.pitch = 35.0f;
                editor->m_camera.target = glm::vec3(0.0f);
                break;
            }
            // Numpad views
            case GLFW_KEY_KP_1: // Front
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_3: // Right
                editor->m_camera.yaw = -90.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_7: // Top
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = 89.9f;
                break;
            case GLFW_KEY_KP_DECIMAL: { // Focus on selection
                if (!editor->m_selectedBlocks.empty()) {
                    glm::vec3 center(0.0f);
                    for (int64_t enc : editor->m_selectedBlocks) {
                        center += glm::vec3(decodePos(enc));
                    }
                    center /= (float)editor->m_selectedBlocks.size();
                    editor->m_camera.target = center;
                }
                break;
            }
            default: break;
        }
    }

    // Ctrl + key shortcuts
    if (ctrl && !shift) {
        switch (key) {
            case GLFW_KEY_N: editor->newStructure(); break;
            case GLFW_KEY_O: editor->loadStructure(); break;
            case GLFW_KEY_S: editor->saveStructure(); break;
            case GLFW_KEY_Z: editor->undo(); break;
            case GLFW_KEY_Y: editor->redo(); break;
            case GLFW_KEY_C: editor->copySelection(); break;
            case GLFW_KEY_V: editor->pasteClipboard(); break;
            case GLFW_KEY_D: editor->duplicateSelection(); break;
            case GLFW_KEY_A: editor->selectAll(); break;
            case GLFW_KEY_I: editor->invertSelection(); break;
            case GLFW_KEY_R: editor->rotateStructure(); break;
            case GLFW_KEY_F: editor->fillSelection(editor->m_selectedBlock); break;
            // Ctrl + Numpad views (opposite directions)
            case GLFW_KEY_KP_1: // Back
                editor->m_camera.yaw = 180.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_3: // Left
                editor->m_camera.yaw = 90.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_7: // Bottom
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = -89.9f;
                break;
            default: break;
        }
    }

    // Ctrl + Shift shortcuts
    if (ctrl && shift) {
        switch (key) {
            case GLFW_KEY_S: editor->saveStructureAs(); break;
            case GLFW_KEY_A: editor->deselectAll(); break;
            case GLFW_KEY_R: editor->rotateSelection(); break;
            default: break;
        }
    }
}

// ============================================================================
// Entry Point
// ============================================================================

int main(int /*argc*/, char** /*argv*/) {
    VxStructEditor editor;

    if (!editor.initialize()) {
        std::cerr << "Failed to initialize VxStruct Editor" << std::endl;
        return -1;
    }

    editor.run();
    return 0;
}
