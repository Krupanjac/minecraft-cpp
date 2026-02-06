// ============================================================================
// VxStruct Editor - Common Types & Utilities (Implementation)
// ============================================================================
#define STB_IMAGE_IMPLEMENTATION
#include "EditorCommon.h"

// ============================================================================
// Block Color Palette
// ============================================================================

const std::vector<BlockColorInfo>& getBlockPalette() {
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

        // Road blocks
        {BlockType::GLAZED_TERRACOTTA,       "Glazed Terracotta",  {0.55f, 0.42f, 0.35f}, "Road"},
        {BlockType::ROAD_STRAIGHT,           "Road Straight",      {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_LEFT,               "Road Left",          {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_RIGHT,              "Road Right",         {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_LEFT_RIGHT,         "Road Left-Right",    {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_T_JUNCTION,         "Road T-Junction",    {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_INTERSECTION_YELLOW,"Road Intersection",  {0.40f, 0.38f, 0.25f}, "Road"},
        {BlockType::ROAD_MIDDLE_LINES,       "Road Mid Lines",     {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_MIDDLE_LINES_YELLOW,"Road Mid Lines Yel", {0.40f, 0.38f, 0.25f}, "Road"},
        {BlockType::ROAD_MIDDLE_RIGHT,       "Road Mid Right",     {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_MIDDLE_RIGHT_YELLOW,"Road Mid Right Yel", {0.40f, 0.38f, 0.25f}, "Road"},
        {BlockType::ROAD_LEFT_DIAG_45,       "Road Diag 45 L",     {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_LEFT_DIAG_45_YELLOW,"Road Diag 45 Yel",   {0.40f, 0.38f, 0.25f}, "Road"},
        {BlockType::ROAD_LEFT_DIAG_60,       "Road Diag 60 L",     {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_LEFT_DIAG_60_YELLOW,"Road Diag 60 Yel",   {0.40f, 0.38f, 0.25f}, "Road"},
        {BlockType::ROAD_RIGHT_DIAG_60,      "Road Diag 60 R",     {0.35f, 0.35f, 0.38f}, "Road"},
        {BlockType::ROAD_RIGHT_DIAG_YELLOW,  "Road Diag R Yel",    {0.40f, 0.38f, 0.25f}, "Road"},
    };
    return palette;
}

glm::vec3 getBlockColor(BlockType type) {
    for (const auto& info : getBlockPalette()) {
        if (info.type == type) return info.color;
    }
    return {0.8f, 0.0f, 0.8f};
}

const char* getBlockName(BlockType type) {
    for (const auto& info : getBlockPalette()) {
        if (info.type == type) return info.name;
    }
    return "Unknown";
}

// ============================================================================
// Shader Helpers
// ============================================================================

GLuint compileShader(GLenum type, const std::string& source) {
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

GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
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
// Cube Mesh Generation
// ============================================================================

void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset, const glm::vec3& color) {
    // Standard OpenGL UV: V=0 at bottom, V=1 at top (textures loaded with stbi flip=true)
    // All faces use same UV pattern: (0,0)=BL, (1,0)=BR, (1,1)=TR, (0,1)=TL
    struct FaceData {
        glm::vec3 v[4];
        glm::vec3 normal;
    };

    // UV is the same for all faces
    const glm::vec2 uv[4] = {{0,0},{1,0},{1,1},{0,1}};

    FaceData faces[6] = {
        // Front (+Z)  BL       BR       TR       TL
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
        vertices.push_back({face.v[0] + offset, face.normal, color, uv[0], -1.0f});
        vertices.push_back({face.v[1] + offset, face.normal, color, uv[1], -1.0f});
        vertices.push_back({face.v[2] + offset, face.normal, color, uv[2], -1.0f});
        vertices.push_back({face.v[0] + offset, face.normal, color, uv[0], -1.0f});
        vertices.push_back({face.v[2] + offset, face.normal, color, uv[2], -1.0f});
        vertices.push_back({face.v[3] + offset, face.normal, color, uv[3], -1.0f});
    }
}

// Textured overload: computes atlas UVs or PBR layer per face
void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset,
                          const glm::vec3& color, BlockType type, int textureMode,
                          const std::unordered_map<std::string, int>* pbrMap) {
    // Standard OpenGL UV: V=0 at bottom, V=1 at top (stbi flip=true)
    // All faces use (0,0)=BL, (1,0)=BR, (1,1)=TR, (0,1)=TL
    struct Face {
        glm::vec3 v[4];
        glm::vec3 normal;
        int faceCategory; // 0=top, 1=side, 2=bottom
    };

    const glm::vec2 baseUV[4] = {{0,0},{1,0},{1,1},{0,1}};

    Face faces[6] = {
        // Front (+Z)   BL       BR       TR       TL
        {{{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}}, {0,0,1},  1},
        // Back (-Z)
        {{{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}}, {0,0,-1}, 1},
        // Right (+X)
        {{{1,0,1}, {1,0,0}, {1,1,0}, {1,1,1}}, {1,0,0},  1},
        // Left (-X)
        {{{0,0,0}, {0,0,1}, {0,1,1}, {0,1,0}}, {-1,0,0}, 1},
        // Top (+Y)
        {{{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}}, {0,1,0},  0},
        // Bottom (-Y)
        {{{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}}, {0,-1,0}, 2},
    };

    for (int i = 0; i < 6; i++) {
        auto& face = faces[i];
        glm::vec2 uv[4];
        float layer = -1.0f;
        glm::vec3 tint = {1.0f, 1.0f, 1.0f}; // Default: no tint (white)

        if (textureMode == 1) {
            // Atlas mode: map base 0-1 UVs into the atlas cell
            // Atlas loaded with stbi flip=true: V=0=bottom, V=1=top (OpenGL standard)
            // Row 0 (image top) is at V near 1.0, row 15 (image bottom) at V near 0.0
            int atlasIdx = getBlockTextureIndex(type, face.faceCategory);
            float cs = 1.0f / 16.0f;
            int col = atlasIdx % 16;
            int row = atlasIdx / 16;
            float cellU0 = col * cs;
            float cellU1 = (col + 1) * cs;
            float cellV0 = 1.0f - (row + 1) * cs; // Cell bottom (lower V)
            float cellV1 = 1.0f - row * cs;        // Cell top (higher V)
            // Half-texel inset to prevent atlas bleeding
            float texel = 0.5f / 256.0f; // 256px atlas = 16 cells * 16px
            cellU0 += texel; cellV0 += texel;
            cellU1 -= texel; cellV1 -= texel;
            // Map base UVs into the atlas cell
            for (int j = 0; j < 4; j++) {
                float u = cellU0 + baseUV[j].x * (cellU1 - cellU0);
                float v = cellV0 + baseUV[j].y * (cellV1 - cellV0);
                uv[j] = {u, v};
            }
            // Apply biome tint for grass/leaf top faces in atlas mode
            // (Atlas grass_top may be grayscale, needs green tint)
            if (type == BlockType::GRASS && face.faceCategory == 0) {
                tint = {0.49f, 0.78f, 0.30f}; // Green biome tint
            } else if (type == BlockType::OAK_LEAVES || type == BlockType::BIRCH_LEAVES ||
                       type == BlockType::JUNGLE_LEAVES || type == BlockType::SPRUCE_LEAVES) {
                tint = {0.40f, 0.65f, 0.20f}; // Leaf biome tint
            }
        } else if (textureMode == 2 && pbrMap) {
            // PBR mode: use base 0-1 UVs directly, set layer
            // Tinting is already baked into composited textures
            for (int j = 0; j < 4; j++) {
                uv[j] = baseUV[j];
            }
            layer = (float)getPBRTextureLayer(type, face.faceCategory, *pbrMap);
        } else {
            // Color-only fallback
            for (int j = 0; j < 4; j++) uv[j] = baseUV[j];
            tint = color; // Use palette color for color-only
        }

        vertices.push_back({face.v[0] + offset, face.normal, tint, uv[0], layer});
        vertices.push_back({face.v[1] + offset, face.normal, tint, uv[1], layer});
        vertices.push_back({face.v[2] + offset, face.normal, tint, uv[2], layer});
        vertices.push_back({face.v[0] + offset, face.normal, tint, uv[0], layer});
        vertices.push_back({face.v[2] + offset, face.normal, tint, uv[2], layer});
        vertices.push_back({face.v[3] + offset, face.normal, tint, uv[3], layer});
    }
}

// ============================================================================
// Orbit Camera
// ============================================================================

glm::vec3 OrbitCamera::getPosition() const {
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    float x = target.x + distance * cos(pitchRad) * sin(yawRad);
    float y = target.y + distance * sin(pitchRad);
    float z = target.z + distance * cos(pitchRad) * cos(yawRad);
    return {x, y, z};
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    return glm::lookAt(getPosition(), target, glm::vec3(0, 1, 0));
}

glm::mat4 OrbitCamera::getProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 500.0f);
}

// ============================================================================
// Raycasting
// ============================================================================

Ray screenToRay(double mouseX, double mouseY, int width, int height,
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

bool rayAABB(const Ray& ray, const glm::ivec3& blockPos, float& tMin, glm::ivec3& hitNormal) {
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
// File Dialogs
// ============================================================================

#ifdef _WIN32
std::string openFileDialog(const char* filter, const char* title) {
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

std::string saveFileDialog(const char* filter, const char* title, const char* defaultExt) {
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
std::string openFileDialog(const char*, const char*) { return ""; }
std::string saveFileDialog(const char*, const char*, const char*) { return ""; }
#endif

// ============================================================================
// Block Texture Atlas Index Mapping
// ============================================================================

int getBlockTextureIndex(BlockType type, int face) {
    // face: 0=top, 1=side, 2=bottom
    // Returns index 0-255 for 16x16 atlas
    switch (type) {
        case BlockType::GRASS:         { int r[] = {0, 3, 2}; return r[face]; }
        case BlockType::DIRT:          return 2;
        case BlockType::STONE:         return 1;
        case BlockType::SAND:          return 18;
        case BlockType::GRAVEL:        return 19;
        case BlockType::SNOW:          return 66;
        case BlockType::ICE:           return 67;
        case BlockType::BEDROCK:       return 17;
        case BlockType::SANDSTONE:     { int r[] = {176, 192, 176}; return r[face]; }
        case BlockType::COBBLESTONE:   return 16;
        case BlockType::CLAY:          return 53;
        case BlockType::COAL_ORE:      return 34;
        case BlockType::IRON_ORE:      return 33;
        case BlockType::GOLD_ORE:      return 32;
        case BlockType::DIAMOND_ORE:   return 50;
        case BlockType::EMERALD_ORE:   return 171;
        case BlockType::REDSTONE_ORE:  return 51;
        case BlockType::LAPIS_ORE:     return 160;
        case BlockType::IRON_BLOCK:    return 22;
        case BlockType::GOLD_BLOCK:    return 23;
        case BlockType::DIAMOND_BLOCK: return 24;
        case BlockType::EMERALD_BLOCK: return 25;
        case BlockType::REDSTONE_BLOCK:return 215;
        case BlockType::BRICKS:        return 7;
        case BlockType::STONE_BRICKS:  return 54;
        case BlockType::MOSSY_STONE_BRICKS:    return 100;
        case BlockType::CRACKED_STONE_BRICKS:  return 118;
        case BlockType::CHISELED_STONE_BRICKS: return 98;
        case BlockType::MOSSY_COBBLESTONE:     return 36;
        case BlockType::GLASS:         return 49;
        case BlockType::OBSIDIAN:      return 37;
        case BlockType::BOOKSHELF:     { int r[] = {4, 35, 4}; return r[face]; }
        case BlockType::TNT:           { int r[] = {9, 8, 10}; return r[face]; }
        case BlockType::GLOWSTONE:     return 105;
        case BlockType::REDSTONE_LAMP: return 123;
        case BlockType::OAK_PLANKS:    return 4;
        case BlockType::SPRUCE_PLANKS: return 198;
        case BlockType::BIRCH_PLANKS:  return 214;
        case BlockType::JUNGLE_PLANKS: return 199;
        case BlockType::OAK_LOG:       { int r[] = {21, 20, 21}; return r[face]; }
        case BlockType::SPRUCE_LOG:    { int r[] = {117, 116, 117}; return r[face]; }
        case BlockType::BIRCH_LOG:     return 117;
        case BlockType::JUNGLE_LOG:    return 153;
        case BlockType::OAK_LEAVES:    return 52;
        case BlockType::SPRUCE_LEAVES: return 132;
        case BlockType::BIRCH_LEAVES:  return 52;
        case BlockType::JUNGLE_LEAVES: return 52;
        case BlockType::TALL_GRASS:    return 39;
        case BlockType::ROSE:          return 12;
        case BlockType::SUGAR_CANE:    return 73;
        case BlockType::WHITE_WOOL:    return 64;
        case BlockType::ORANGE_WOOL:   return 210;
        case BlockType::MAGENTA_WOOL:  return 194;
        case BlockType::LIGHT_BLUE_WOOL: return 178;
        case BlockType::YELLOW_WOOL:   return 162;
        case BlockType::LIME_WOOL:     return 146;
        case BlockType::PINK_WOOL:     return 130;
        case BlockType::GRAY_WOOL:     return 114;
        case BlockType::LIGHT_GRAY_WOOL: return 225;
        case BlockType::CYAN_WOOL:     return 209;
        case BlockType::PURPLE_WOOL:   return 193;
        case BlockType::BLUE_WOOL:     return 177;
        case BlockType::BROWN_WOOL:    return 161;
        case BlockType::GREEN_WOOL:    return 145;
        case BlockType::RED_WOOL:      return 129;
        case BlockType::BLACK_WOOL:    return 113;
        case BlockType::CRAFTING_TABLE:{ int r[] = {43, 59, 4}; return r[face]; }
        case BlockType::NOTE_BLOCK:    return 74;
        case BlockType::JUKEBOX:       { int r[] = {75, 74, 74}; return r[face]; }
        case BlockType::SPONGE:        return 48;
        case BlockType::COBWEB:        return 11;
        case BlockType::FARMLAND:      { int r[] = {86, 2, 2}; return r[face]; }
        case BlockType::WATER:         return 205;
        // Road blocks - use gray concrete-ish tile as atlas fallback
        case BlockType::ROAD_STRAIGHT:
        case BlockType::ROAD_LEFT:
        case BlockType::ROAD_RIGHT:
        case BlockType::ROAD_LEFT_RIGHT:
        case BlockType::ROAD_T_JUNCTION:
        case BlockType::ROAD_INTERSECTION_YELLOW:
        case BlockType::ROAD_MIDDLE_LINES:
        case BlockType::ROAD_MIDDLE_LINES_YELLOW:
        case BlockType::ROAD_MIDDLE_RIGHT:
        case BlockType::ROAD_MIDDLE_RIGHT_YELLOW:
        case BlockType::ROAD_LEFT_DIAG_45:
        case BlockType::ROAD_LEFT_DIAG_45_YELLOW:
        case BlockType::ROAD_LEFT_DIAG_60:
        case BlockType::ROAD_LEFT_DIAG_60_YELLOW:
        case BlockType::ROAD_RIGHT_DIAG_60:
        case BlockType::ROAD_RIGHT_DIAG_YELLOW:
            return 1; // Stone placeholder
        case BlockType::GLAZED_TERRACOTTA:
            return 1; // Stone placeholder
        default: return 1;
    }
}

GLuint loadBlockAtlasTexture(const std::string& path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true); // Match game convention: V=0 at bottom
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load atlas: " << path << std::endl;
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    std::cout << "Loaded block atlas: " << w << "x" << h << std::endl;
    return tex;
}

// ============================================================================
// Editor Settings
// ============================================================================

void EditorSettings::addRecentFile(const std::string& path) {
    // Remove if already in list
    recentFiles.erase(
        std::remove(recentFiles.begin(), recentFiles.end(), path),
        recentFiles.end()
    );
    // Insert at front
    recentFiles.insert(recentFiles.begin(), path);
    // Trim to max
    if (recentFiles.size() > MAX_RECENT)
        recentFiles.resize(MAX_RECENT);
}

void EditorSettings::save(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return;

    f << "[EditorSettings]\n";
    f << "usePBRTextures=" << (usePBRTextures ? 1 : 0) << "\n";
    f << "showTexturesInPalette=" << (showTexturesInPalette ? 1 : 0) << "\n";
    f << "autoSaveEnabled=" << (autoSaveEnabled ? 1 : 0) << "\n";
    f << "autoSaveIntervalSec=" << autoSaveIntervalSec << "\n";

    f << "[RecentFiles]\n";
    for (size_t i = 0; i < recentFiles.size(); i++) {
        f << "recent" << i << "=" << recentFiles[i] << "\n";
    }
}

void EditorSettings::load(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return;

    std::string line;
    recentFiles.clear();

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '[') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "usePBRTextures")        usePBRTextures = (val == "1");
        else if (key == "showTexturesInPalette") showTexturesInPalette = (val == "1");
        else if (key == "autoSaveEnabled")  autoSaveEnabled = (val == "1");
        else if (key == "autoSaveIntervalSec") autoSaveIntervalSec = std::stof(val);
        else if (key.substr(0, 6) == "recent") recentFiles.push_back(val);
    }
}

// ============================================================================
// PBR Texture Loading (simplified - albedo only for editor)
// ============================================================================

GLuint loadPBRAlbedoArray(const std::string& pbrPath, std::unordered_map<std::string, int>& nameToLayer, int& texSize) {
    namespace fs = std::filesystem;
    std::string texturePath = pbrPath + "/textures/block";

    if (!fs::exists(texturePath)) {
        std::cerr << "PBR texture path not found: " << texturePath << std::endl;
        return 0;
    }

    // Collect albedo filenames (exclude _n, _s, _e suffixes)
    std::vector<std::string> albedoFiles;
    std::vector<std::string> albedoNames;

    for (const auto& entry : fs::directory_iterator(texturePath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext != ".png" && ext != ".PNG") continue;

        std::string stem = entry.path().stem().string();
        // Skip normal, specular, emissive maps
        if (stem.size() >= 2) {
            std::string suffix2 = stem.substr(stem.size() - 2);
            if (suffix2 == "_n" || suffix2 == "_s" || suffix2 == "_e") continue;
        }
        // Also skip destroy_stage textures
        if (stem.find("destroy_stage") != std::string::npos) continue;

        albedoFiles.push_back(entry.path().string());
        albedoNames.push_back(stem);
    }

    if (albedoFiles.empty()) {
        std::cerr << "No PBR albedo textures found in: " << texturePath << std::endl;
        return 0;
    }

    // Also load road textures from assets/roads/
    {
        std::string roadPath = "assets/roads";
        if (fs::exists(roadPath)) {
            auto isRoadPBR = [](const std::string& name) -> bool {
                const std::string suffixes[] = {"_ao", "_normal", "_metallic", "_metalic", "_roughness"};
                for (const auto& s : suffixes) {
                    if (name.length() > s.length() && name.substr(name.length() - s.length()) == s)
                        return true;
                }
                return false;
            };
            for (const auto& entry : fs::directory_iterator(roadPath)) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                if (ext != ".png" && ext != ".PNG") continue;
                std::string stem = entry.path().stem().string();
                if (isRoadPBR(stem)) continue;
                albedoFiles.push_back(entry.path().string());
                albedoNames.push_back(stem);
            }
        }
    }

    // Sort for deterministic ordering
    std::vector<size_t> indices(albedoFiles.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return albedoNames[a] < albedoNames[b];
    });

    // Detect texture size from first file
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true); // Match game: V=0 at bottom (OpenGL standard)
    unsigned char* probe = stbi_load(albedoFiles[indices[0]].c_str(), &w, &h, &ch, 4);
    if (!probe) {
        std::cerr << "Failed to load first PBR texture for size detection" << std::endl;
        return 0;
    }
    texSize = std::min(w, h);
    if (texSize > 128) texSize = 128;
    stbi_image_free(probe);

    // ---- Phase 1: Load all textures into CPU memory ----
    struct TexData {
        std::string name;
        std::vector<unsigned char> pixels; // texSize * texSize * 4
    };
    std::vector<TexData> allTextures;

    for (size_t idx : indices) {
        TexData td;
        td.name = albedoNames[idx];
        td.pixels.resize(texSize * texSize * 4);

        int tw, th, tc;
        unsigned char* data = stbi_load(albedoFiles[idx].c_str(), &tw, &th, &tc, 4);
        if (!data) {
            // Magenta fallback
            for (int p = 0; p < texSize * texSize; p++) {
                td.pixels[p * 4 + 0] = 255;
                td.pixels[p * 4 + 1] = 0;
                td.pixels[p * 4 + 2] = 255;
                td.pixels[p * 4 + 3] = 255;
            }
        } else {
            if (tw == texSize && th == texSize) {
                memcpy(td.pixels.data(), data, texSize * texSize * 4);
            } else {
                // Nearest-neighbor resize
                for (int y = 0; y < texSize; y++) {
                    for (int x = 0; x < texSize; x++) {
                        int sx = x * tw / texSize;
                        int sy = y * th / texSize;
                        int srcI = (sy * tw + sx) * 4;
                        int dstI = (y * texSize + x) * 4;
                        td.pixels[dstI + 0] = data[srcI + 0];
                        td.pixels[dstI + 1] = data[srcI + 1];
                        td.pixels[dstI + 2] = data[srcI + 2];
                        td.pixels[dstI + 3] = data[srcI + 3];
                    }
                }
            }
            stbi_image_free(data);
        }
        allTextures.push_back(std::move(td));
    }

    // Build name→index map for lookups
    std::unordered_map<std::string, int> tempIndex;
    for (int i = 0; i < (int)allTextures.size(); i++) {
        tempIndex[allTextures[i].name] = i;
    }

    // ---- Phase 2: Create composited grass textures (like game does) ----
    auto dirtIt = tempIndex.find("dirt");
    auto overlayIt = tempIndex.find("grass_block_side_overlay");
    auto grassTopIt = tempIndex.find("grass_block_top");

    // Composite: dirt + grass_block_side_overlay → grass_block_side_composited
    if (dirtIt != tempIndex.end() && overlayIt != tempIndex.end()) {
        TexData composited;
        composited.name = "grass_block_side_composited";
        composited.pixels.resize(texSize * texSize * 4);
        const auto& dirt = allTextures[dirtIt->second].pixels;
        const auto& overlay = allTextures[overlayIt->second].pixels;

        // Green biome tint for the overlay (matching game: 0.5, 0.85, 0.4)
        float tintR = 0.49f, tintG = 0.78f, tintB = 0.30f;

        for (int i = 0; i < texSize * texSize; i++) {
            float overlayA = overlay[i * 4 + 3] / 255.0f;
            // Tint the overlay by biome green
            float oR = (overlay[i * 4 + 0] / 255.0f) * tintR;
            float oG = (overlay[i * 4 + 1] / 255.0f) * tintG;
            float oB = (overlay[i * 4 + 2] / 255.0f) * tintB;
            // Blend: result = dirt * (1 - overlayA) + tinted_overlay * overlayA
            float dR = dirt[i * 4 + 0] / 255.0f;
            float dG = dirt[i * 4 + 1] / 255.0f;
            float dB = dirt[i * 4 + 2] / 255.0f;
            composited.pixels[i * 4 + 0] = (unsigned char)(std::min(1.0f, dR * (1.0f - overlayA) + oR * overlayA) * 255.0f);
            composited.pixels[i * 4 + 1] = (unsigned char)(std::min(1.0f, dG * (1.0f - overlayA) + oG * overlayA) * 255.0f);
            composited.pixels[i * 4 + 2] = (unsigned char)(std::min(1.0f, dB * (1.0f - overlayA) + oB * overlayA) * 255.0f);
            composited.pixels[i * 4 + 3] = 255; // Fully opaque result
        }
        allTextures.push_back(std::move(composited));
        std::cout << "Created composited grass_block_side texture" << std::endl;
    }

    // Tint grass_block_top → grass_block_top_tinted
    if (grassTopIt != tempIndex.end()) {
        TexData tinted;
        tinted.name = "grass_block_top_tinted";
        tinted.pixels.resize(texSize * texSize * 4);
        const auto& src = allTextures[grassTopIt->second].pixels;

        float tintR = 0.49f, tintG = 0.78f, tintB = 0.30f;
        for (int i = 0; i < texSize * texSize; i++) {
            tinted.pixels[i * 4 + 0] = (unsigned char)(std::min(255.0f, src[i * 4 + 0] * tintR));
            tinted.pixels[i * 4 + 1] = (unsigned char)(std::min(255.0f, src[i * 4 + 1] * tintG));
            tinted.pixels[i * 4 + 2] = (unsigned char)(std::min(255.0f, src[i * 4 + 2] * tintB));
            tinted.pixels[i * 4 + 3] = src[i * 4 + 3];
        }
        allTextures.push_back(std::move(tinted));
        std::cout << "Created tinted grass_block_top texture" << std::endl;
    }

    // ---- Phase 3: Upload everything to GL texture array ----
    int layerCount = (int)allTextures.size();
    GLuint texArray;
    glGenTextures(1, &texArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, texSize, texSize, layerCount, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    nameToLayer.clear();
    for (int i = 0; i < layerCount; i++) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, texSize, texSize, 1,
                       GL_RGBA, GL_UNSIGNED_BYTE, allTextures[i].pixels.data());
        nameToLayer[allTextures[i].name] = i;
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    std::cout << "Loaded " << layerCount << " PBR albedo textures (" << texSize << "x" << texSize << ")" << std::endl;
    return texArray;
}

// Map BlockType + face → PBR texture name
static std::string getPBRTextureName(BlockType type, int face) {
    // face: 0=top, 1=side, 2=bottom
    switch (type) {
        case BlockType::GRASS:
            return (face == 0) ? "grass_block_top_tinted" : (face == 2) ? "dirt" : "grass_block_side_composited";
        case BlockType::DIRT:          return "dirt";
        case BlockType::STONE:         return "stone";
        case BlockType::SAND:          return "sand";
        case BlockType::GRAVEL:        return "gravel";
        case BlockType::SNOW:          return "snow";
        case BlockType::ICE:           return "ice";
        case BlockType::BEDROCK:       return "bedrock";
        case BlockType::SANDSTONE:
            return (face == 1) ? "sandstone" : "sandstone_top";
        case BlockType::COBBLESTONE:   return "cobblestone";
        case BlockType::CLAY:          return "clay";
        case BlockType::COAL_ORE:      return "coal_ore";
        case BlockType::IRON_ORE:      return "iron_ore";
        case BlockType::GOLD_ORE:      return "gold_ore";
        case BlockType::DIAMOND_ORE:   return "diamond_ore";
        case BlockType::EMERALD_ORE:   return "emerald_ore";
        case BlockType::REDSTONE_ORE:  return "redstone_ore";
        case BlockType::LAPIS_ORE:     return "lapis_ore";
        case BlockType::IRON_BLOCK:    return "iron_block";
        case BlockType::GOLD_BLOCK:    return "gold_block";
        case BlockType::DIAMOND_BLOCK: return "diamond_block";
        case BlockType::EMERALD_BLOCK: return "emerald_block";
        case BlockType::REDSTONE_BLOCK:return "redstone_block";
        case BlockType::BRICKS:        return "bricks";
        case BlockType::STONE_BRICKS:  return "stone_bricks";
        case BlockType::MOSSY_STONE_BRICKS:    return "mossy_stone_bricks";
        case BlockType::CRACKED_STONE_BRICKS:  return "cracked_stone_bricks";
        case BlockType::CHISELED_STONE_BRICKS: return "chiseled_stone_bricks";
        case BlockType::MOSSY_COBBLESTONE:     return "mossy_cobblestone";
        case BlockType::GLASS:         return "glass";
        case BlockType::OBSIDIAN:      return "obsidian";
        case BlockType::BOOKSHELF:
            return (face == 1) ? "bookshelf" : "oak_planks";
        case BlockType::TNT:
            return (face == 0) ? "tnt_top" : (face == 2) ? "tnt_bottom" : "tnt_side";
        case BlockType::GLOWSTONE:     return "glowstone";
        case BlockType::REDSTONE_LAMP: return "redstone_lamp_on";
        case BlockType::OAK_PLANKS:    return "oak_planks";
        case BlockType::SPRUCE_PLANKS: return "spruce_planks";
        case BlockType::BIRCH_PLANKS:  return "birch_planks";
        case BlockType::JUNGLE_PLANKS: return "jungle_planks";
        case BlockType::OAK_LOG:
            return (face == 1) ? "oak_log" : "oak_log_top";
        case BlockType::SPRUCE_LOG:
            return (face == 1) ? "spruce_log" : "spruce_log_top";
        case BlockType::BIRCH_LOG:
            return (face == 1) ? "birch_log" : "birch_log_top";
        case BlockType::JUNGLE_LOG:
            return (face == 1) ? "jungle_log" : "jungle_log_top";
        case BlockType::OAK_LEAVES:    return "oak_leaves";
        case BlockType::SPRUCE_LEAVES: return "spruce_leaves";
        case BlockType::BIRCH_LEAVES:  return "birch_leaves";
        case BlockType::JUNGLE_LEAVES: return "jungle_leaves";
        case BlockType::TALL_GRASS:    return "grass";
        case BlockType::ROSE:          return "dandelion";
        case BlockType::SUGAR_CANE:    return "sugar_cane";
        case BlockType::WHITE_WOOL:    return "white_wool";
        case BlockType::ORANGE_WOOL:   return "orange_wool";
        case BlockType::MAGENTA_WOOL:  return "magenta_wool";
        case BlockType::LIGHT_BLUE_WOOL: return "light_blue_wool";
        case BlockType::YELLOW_WOOL:   return "white_wool"; // fallback
        case BlockType::LIME_WOOL:     return "lime_wool";
        case BlockType::PINK_WOOL:     return "pink_wool";
        case BlockType::GRAY_WOOL:     return "gray_wool";
        case BlockType::LIGHT_GRAY_WOOL: return "light_gray_wool";
        case BlockType::CYAN_WOOL:     return "cyan_wool";
        case BlockType::PURPLE_WOOL:   return "purple_wool";
        case BlockType::BLUE_WOOL:     return "blue_wool";
        case BlockType::BROWN_WOOL:    return "brown_wool";
        case BlockType::GREEN_WOOL:    return "green_wool";
        case BlockType::RED_WOOL:      return "red_wool";
        case BlockType::BLACK_WOOL:    return "black_wool";
        case BlockType::CRAFTING_TABLE:
            return (face == 0) ? "crafting_table_top" : (face == 1) ? "crafting_table_side" : "oak_planks";
        case BlockType::NOTE_BLOCK:    return "note_block";
        case BlockType::JUKEBOX:
            return (face == 0) ? "jukebox_top" : "note_block";
        case BlockType::SPONGE:        return "sponge";
        case BlockType::COBWEB:        return "cobweb";
        case BlockType::FARMLAND:
            return (face == 0) ? "farmland" : "dirt";
        case BlockType::WATER:         return "ice"; // placeholder
        // Road blocks: top = specific road texture, sides/bottom = glazed_terracotta_base
        case BlockType::GLAZED_TERRACOTTA:       return "glazed_terracotta_base";
        case BlockType::ROAD_STRAIGHT:           return (face == 0) ? "glazed_terracotta_up" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT:               return (face == 0) ? "glazed_terracotta_left" : "glazed_terracotta_base";
        case BlockType::ROAD_RIGHT:              return (face == 0) ? "glazed_terracotta_right" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT_RIGHT:         return (face == 0) ? "glazed_terracotta_left_right" : "glazed_terracotta_base";
        case BlockType::ROAD_T_JUNCTION:         return (face == 0) ? "glazed_terracotta_left_right_no_forward" : "glazed_terracotta_base";
        case BlockType::ROAD_INTERSECTION_YELLOW:return (face == 0) ? "glazed_terracotta_intersection_yellow_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_MIDDLE_LINES:       return (face == 0) ? "glazed_terracotta_m_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_MIDDLE_LINES_YELLOW:return (face == 0) ? "glazed_terracotta_m_lines_yellow" : "glazed_terracotta_base";
        case BlockType::ROAD_MIDDLE_RIGHT:       return (face == 0) ? "glazed_terracotta_m_right" : "glazed_terracotta_base";
        case BlockType::ROAD_MIDDLE_RIGHT_YELLOW:return (face == 0) ? "glazed_terracotta_m_right_yellow_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT_DIAG_45:       return (face == 0) ? "glazed_terracotta_left_diag_45_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT_DIAG_45_YELLOW:return (face == 0) ? "glazed_terracotta_left_diag_45_lines_yellow" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT_DIAG_60:       return (face == 0) ? "glazed_terracotta_left_diag_60" : "glazed_terracotta_base";
        case BlockType::ROAD_LEFT_DIAG_60_YELLOW:return (face == 0) ? "glazed_terracotta_left_diag_60_yellow_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_RIGHT_DIAG_60:      return (face == 0) ? "glazed_terracotta_right_diag_60_lines" : "glazed_terracotta_base";
        case BlockType::ROAD_RIGHT_DIAG_YELLOW:  return (face == 0) ? "glazed_terracotta_right_diag_yellow" : "glazed_terracotta_base";
        default: return "stone";
    }
}

int getPBRTextureLayer(BlockType type, int face, const std::unordered_map<std::string, int>& nameToLayer) {
    std::string name = getPBRTextureName(type, face);
    auto it = nameToLayer.find(name);
    if (it != nameToLayer.end()) return it->second;
    // Fallback: try stone
    it = nameToLayer.find("stone");
    if (it != nameToLayer.end()) return it->second;
    return 0;
}
