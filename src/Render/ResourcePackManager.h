#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <glad/glad.h>
#include "../World/Block.h"

// Structure to hold PBR texture set for a block face
struct PBRTextureSet {
    int albedoIndex = -1;     // Diffuse/color texture index in array
    int normalIndex = -1;     // Normal map index (_n suffix)
    int specularIndex = -1;   // Specular/roughness map index (_s suffix)
    bool hasNormal = false;
    bool hasSpecular = false;
};

// Structure to map block faces to textures
struct BlockTextureMapping {
    PBRTextureSet top;
    PBRTextureSet bottom;
    PBRTextureSet side;
    PBRTextureSet north;  // For directional blocks
    PBRTextureSet south;
    PBRTextureSet east;
    PBRTextureSet west;
    bool useDirectional = false;  // If true, use individual face textures
};

class ResourcePackManager {
public:
    static ResourcePackManager& instance() {
        static ResourcePackManager instance;
        return instance;
    }

    // Initialize and load resource pack textures
    bool initialize(const std::string& resourcePackPath = "assets/PBRANDPOM");
    void cleanup();

    // Check if resource pack is loaded and available
    bool isLoaded() const { return loaded; }
    bool isEnabled() const { return enabled; }
    void setEnabled(bool value) { enabled = value; }

    // Get OpenGL texture IDs
    GLuint getAlbedoTextureArray() const { return albedoTextureArray; }
    GLuint getNormalTextureArray() const { return normalTextureArray; }
    GLuint getSpecularTextureArray() const { return specularTextureArray; }

    // Get texture layer index for a block type and face
    // face: 0=top, 1=bottom, 2=side (or north/south/east/west)
    // normalDirection: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    int getTextureIndex(BlockType type, int normalDirection) const;
    int getNormalMapIndex(BlockType type, int normalDirection) const;
    int getSpecularMapIndex(BlockType type, int normalDirection) const;
    
    // Get water-specific textures
    int getWaterStillIndex() const { return waterStillIndex; }
    int getWaterFlowIndex() const { return waterFlowIndex; }
    int getIceIndex() const { return iceIndex; }

    // Texture array dimensions
    int getTextureSize() const { return textureSize; }
    int getTextureCount() const { return textureCount; }

private:
    ResourcePackManager() = default;
    ~ResourcePackManager() { cleanup(); }
    ResourcePackManager(const ResourcePackManager&) = delete;
    ResourcePackManager& operator=(const ResourcePackManager&) = delete;

    bool loaded = false;
    bool enabled = false;
    
    GLuint albedoTextureArray = 0;
    GLuint normalTextureArray = 0;
    GLuint specularTextureArray = 0;
    
    int textureSize = 16;  // Default texture resolution (will detect from files)
    int textureCount = 0;
    
    // Maps texture filename (without extension) to array layer index
    std::unordered_map<std::string, int> albedoTextureMap;
    std::unordered_map<std::string, int> normalTextureMap;
    std::unordered_map<std::string, int> specularTextureMap;
    
    // Block type to texture mapping
    std::unordered_map<BlockType, BlockTextureMapping> blockMappings;
    
    // Special texture indices
    int waterStillIndex = -1;
    int waterFlowIndex = -1;
    int iceIndex = -1;

    // Helper functions
    bool loadTextures(const std::string& texturePath);
    void setupBlockMappings();
    int findOrLoadTexture(const std::string& name, 
                         std::unordered_map<std::string, int>& textureMap,
                         std::vector<unsigned char*>& textureData,
                         std::vector<std::string>& textureNames,
                         const std::string& basePath,
                         const std::string& suffix = "");
    bool createTextureArrays(const std::vector<unsigned char*>& albedoData,
                            const std::vector<unsigned char*>& normalData,
                            const std::vector<unsigned char*>& specularData);
    unsigned char* loadImage(const std::string& path, int& width, int& height);
    unsigned char* createDefaultNormalMap();
    unsigned char* createDefaultSpecularMap();
    unsigned char* resizeTexture(unsigned char* src, int srcW, int srcH, int dstW, int dstH);
    unsigned char* compositeTextures(unsigned char* base, unsigned char* overlay, 
                                      int width, int height,
                                      float tintR = 1.0f, float tintG = 1.0f, float tintB = 1.0f);
    unsigned char* tintTexture(unsigned char* src, int width, int height,
                               float tintR, float tintG, float tintB);
};
