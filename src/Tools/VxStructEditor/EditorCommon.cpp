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
        vertices.push_back({face.v[0] + offset, face.normal, color});
        vertices.push_back({face.v[1] + offset, face.normal, color});
        vertices.push_back({face.v[2] + offset, face.normal, color});
        vertices.push_back({face.v[0] + offset, face.normal, color});
        vertices.push_back({face.v[2] + offset, face.normal, color});
        vertices.push_back({face.v[3] + offset, face.normal, color});
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
        default: return 1;
    }
}

GLuint loadBlockAtlasTexture(const std::string& path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);
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
