// ============================================================================
// VxStruct Editor - Common Types & Utilities
// Shared types, helpers, and data used across all editor files.
// ============================================================================
#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

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
#include <chrono>
#include <stb_image.h>

// ============================================================================
// Move Axis Constraint
// ============================================================================

enum class MoveAxis { FREE, X, Y, Z };

// ============================================================================
// Editor Settings (persisted to editor_settings.ini)
// ============================================================================

struct EditorSettings {
    // Texture mode
    bool usePBRTextures = false;   // false = atlas, true = PBR
    bool showTexturesInPalette = true;

    // Auto-save
    bool autoSaveEnabled = true;
    float autoSaveIntervalSec = 60.0f; // seconds

    // Recent files
    std::vector<std::string> recentFiles; // most recent first
    static const size_t MAX_RECENT = 10;

    void addRecentFile(const std::string& path);
    void save(const std::string& filepath) const;
    void load(const std::string& filepath);
};

// ============================================================================
// Block Texture Atlas Helper
// ============================================================================

int getBlockTextureIndex(BlockType type, int face = 1); // face: 0=top,1=side,2=bottom
GLuint loadBlockAtlasTexture(const std::string& path);  // returns GL texture ID

// ============================================================================
// Block Color Database
// ============================================================================

struct BlockColorInfo {
    BlockType type;
    const char* name;
    glm::vec3 color;
    const char* category;
};

const std::vector<BlockColorInfo>& getBlockPalette();
glm::vec3 getBlockColor(BlockType type);
const char* getBlockName(BlockType type);

// ============================================================================
// Shader Helpers
// ============================================================================

GLuint compileShader(GLenum type, const std::string& source);
GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath);

// ============================================================================
// Cube Mesh Vertex
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset, const glm::vec3& color);

// ============================================================================
// Orbit Camera
// ============================================================================

struct OrbitCamera {
    glm::vec3 target = {0, 4, 0};
    float distance = 25.0f;
    float yaw = 45.0f;
    float pitch = 30.0f;
    float fov = 45.0f;

    glm::vec3 getPosition() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
};

// ============================================================================
// Raycasting
// ============================================================================

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct RayHit {
    bool hit = false;
    glm::ivec3 blockPos;
    glm::ivec3 normal;
    float distance = 1e30f;
};

Ray screenToRay(double mouseX, double mouseY, int width, int height,
                const glm::mat4& projection, const glm::mat4& view);

bool rayAABB(const Ray& ray, const glm::ivec3& blockPos, float& tMin, glm::ivec3& hitNormal);

// ============================================================================
// File Dialogs (Platform-specific)
// ============================================================================

std::string openFileDialog(const char* filter, const char* title);
std::string saveFileDialog(const char* filter, const char* title, const char* defaultExt);
