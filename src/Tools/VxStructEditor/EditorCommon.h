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
#include <unordered_map>
#include <numeric>
#include <stb_image.h>

// ============================================================================
// Move Axis Constraint
// ============================================================================

enum class MoveAxis { FREE, X, Y, Z };

// ============================================================================
// Rotation Axis
// ============================================================================

enum class RotationAxis { X, Y, Z };

// ============================================================================
// Rotation Pivot Mode
// ============================================================================

enum class RotationPivot { BOUNDING_CENTER, ORIGIN, CUSTOM };

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
// PBR Texture Loading (simplified - albedo only for editor)
// ============================================================================

GLuint loadPBRAlbedoArray(const std::string& pbrPath, std::unordered_map<std::string, int>& nameToLayer, int& texSize);
int getPBRTextureLayer(BlockType type, int face, const std::unordered_map<std::string, int>& nameToLayer);

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
    glm::vec2 uv;        // Atlas UV coords (atlas mode) or 0-1 face UV (PBR mode)
    float texLayer;      // PBR texture array layer index; -1 if unused
};

// Generate cube vertices with flat color only (textureMode=0)
void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset, const glm::vec3& color);

// Generate cube vertices with texture UVs (textureMode: 0=color, 1=atlas, 2=PBR)
void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset,
                          const glm::vec3& color, BlockType type, int textureMode,
                          const std::unordered_map<std::string, int>* pbrMap = nullptr);

// Generate cube vertices with texture UVs + face rotation from metadata
// metadata bits 0-1: Y-axis rotation (0=0, 1=90, 2=180, 3=270 degrees)
void generateCubeVertices(std::vector<Vertex>& vertices, const glm::vec3& offset,
                          const glm::vec3& color, BlockType type, int textureMode,
                          uint8_t metadata,
                          const std::unordered_map<std::string, int>* pbrMap = nullptr);

// Metadata helpers for block face rotation
// Face rotation is stored in the lower 2 bits of metadata (0-3 = 0/90/180/270 deg around Y)
inline uint8_t getBlockFaceRotation(uint8_t metadata) { return metadata & 0x03; }
inline uint8_t setBlockFaceRotation(uint8_t metadata, uint8_t rotation) {
    return (metadata & 0xFC) | (rotation & 0x03);
}

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
