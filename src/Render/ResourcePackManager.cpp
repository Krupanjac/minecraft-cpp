#include "ResourcePackManager.h"
#include "../Core/Logger.h"
#include <stb_image.h>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

bool ResourcePackManager::initialize(const std::string& resourcePackPath) {
    if (loaded) {
        LOG_WARNING("ResourcePackManager already initialized");
        return true;
    }

    std::string texturePath = resourcePackPath + "/textures/block";
    
    if (!fs::exists(texturePath)) {
        LOG_ERROR("Resource pack texture path not found: " + texturePath);
        return false;
    }

    if (!loadTextures(texturePath)) {
        LOG_ERROR("Failed to load resource pack textures");
        return false;
    }

    setupBlockMappings();
    loaded = true;
    enabled = false;  // Disabled by default until user enables it
    
    LOG_INFO("ResourcePackManager initialized with " + std::to_string(textureCount) + " textures");
    return true;
}

void ResourcePackManager::cleanup() {
    if (albedoTextureArray) {
        glDeleteTextures(1, &albedoTextureArray);
        albedoTextureArray = 0;
    }
    if (normalTextureArray) {
        glDeleteTextures(1, &normalTextureArray);
        normalTextureArray = 0;
    }
    if (specularTextureArray) {
        glDeleteTextures(1, &specularTextureArray);
        specularTextureArray = 0;
    }
    
    albedoTextureMap.clear();
    normalTextureMap.clear();
    specularTextureMap.clear();
    blockMappings.clear();
    
    loaded = false;
    textureCount = 0;
}

unsigned char* ResourcePackManager::loadImage(const std::string& path, int& width, int& height) {
    int channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    return data;
}

unsigned char* ResourcePackManager::createDefaultNormalMap() {
    // Create a flat normal map (pointing straight up in tangent space)
    unsigned char* data = new unsigned char[textureSize * textureSize * 4];
    for (int i = 0; i < textureSize * textureSize; i++) {
        data[i * 4 + 0] = 128;  // R = 0.5 (X normal = 0)
        data[i * 4 + 1] = 128;  // G = 0.5 (Y normal = 0)
        data[i * 4 + 2] = 255;  // B = 1.0 (Z normal = 1)
        data[i * 4 + 3] = 255;  // A = 1.0
    }
    return data;
}

unsigned char* ResourcePackManager::createDefaultSpecularMap() {
    // Create default specular map (mid roughness, no metallic)
    unsigned char* data = new unsigned char[textureSize * textureSize * 4];
    for (int i = 0; i < textureSize * textureSize; i++) {
        data[i * 4 + 0] = 128;  // R = roughness
        data[i * 4 + 1] = 0;    // G = metallic
        data[i * 4 + 2] = 255;  // B = AO
        data[i * 4 + 3] = 255;  // A
    }
    return data;
}

unsigned char* ResourcePackManager::resizeTexture(unsigned char* src, int srcW, int srcH, int dstW, int dstH) {
    // Simple bilinear downscale from srcW x srcH to dstW x dstH
    unsigned char* dst = new unsigned char[dstW * dstH * 4];
    
    float scaleX = static_cast<float>(srcW) / dstW;
    float scaleY = static_cast<float>(srcH) / dstH;
    
    for (int y = 0; y < dstH; y++) {
        for (int x = 0; x < dstW; x++) {
            // Map destination pixel to source position
            float srcX = (x + 0.5f) * scaleX - 0.5f;
            float srcY = (y + 0.5f) * scaleY - 0.5f;
            
            int x0 = static_cast<int>(srcX);
            int y0 = static_cast<int>(srcY);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            
            // Clamp to valid range
            x0 = (x0 < 0) ? 0 : (x0 >= srcW ? srcW - 1 : x0);
            y0 = (y0 < 0) ? 0 : (y0 >= srcH ? srcH - 1 : y0);
            x1 = (x1 < 0) ? 0 : (x1 >= srcW ? srcW - 1 : x1);
            y1 = (y1 < 0) ? 0 : (y1 >= srcH ? srcH - 1 : y1);
            
            float fx = srcX - static_cast<int>(srcX);
            float fy = srcY - static_cast<int>(srcY);
            if (fx < 0) fx = 0;
            if (fy < 0) fy = 0;
            
            // Bilinear interpolation for each channel
            for (int c = 0; c < 4; c++) {
                float v00 = src[(y0 * srcW + x0) * 4 + c];
                float v10 = src[(y0 * srcW + x1) * 4 + c];
                float v01 = src[(y1 * srcW + x0) * 4 + c];
                float v11 = src[(y1 * srcW + x1) * 4 + c];
                
                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                float v = v0 * (1 - fy) + v1 * fy;
                
                dst[(y * dstW + x) * 4 + c] = static_cast<unsigned char>(v);
            }
        }
    }
    
    return dst;
}

bool ResourcePackManager::loadTextures(const std::string& texturePath) {
    std::vector<unsigned char*> albedoData;
    std::vector<unsigned char*> normalData;
    std::vector<unsigned char*> specularData;
    std::vector<std::string> albedoNames;
    std::vector<std::string> normalNames;
    std::vector<std::string> specularNames;
    std::vector<bool> normalFromStbiFlags;
    std::vector<bool> specularFromStbiFlags;
    
    // First pass: scan all PNG files and categorize them
    std::vector<std::string> baseTextures;
    
    for (const auto& entry : fs::directory_iterator(texturePath)) {
        if (entry.path().extension() != ".png") continue;
        
        std::string filename = entry.path().stem().string();
        
        // Skip normal and specular maps in base texture list
        if (filename.length() >= 2) {
            std::string suffix = filename.substr(filename.length() - 2);
            if (suffix == "_n" || suffix == "_s") continue;
        }
        
        baseTextures.push_back(filename);
    }
    
    // Sort for consistent ordering
    std::sort(baseTextures.begin(), baseTextures.end());
    
    // Detect texture size from first valid texture
    if (!baseTextures.empty()) {
        std::string firstTexPath = texturePath + "/" + baseTextures[0] + ".png";
        int w, h;
        unsigned char* testData = loadImage(firstTexPath, w, h);
        if (testData) {
            textureSize = w;  // Assume square textures
            stbi_image_free(testData);
            LOG_INFO("Detected texture size: " + std::to_string(textureSize) + "x" + std::to_string(textureSize));
        }
    }
    
    // Load all textures
    for (const auto& baseName : baseTextures) {
        std::string albedoPath = texturePath + "/" + baseName + ".png";
        std::string normalPath = texturePath + "/" + baseName + "_n.png";
        std::string specularPath = texturePath + "/" + baseName + "_s.png";
        
        int w, h;
        unsigned char* albedo = loadImage(albedoPath, w, h);
        
        if (!albedo) continue;
        
        // Resize if needed to match our standard size (128x128)
        if (w != textureSize || h != textureSize) {
            // Resize texture to standard size
            if (w == 256 && h == 256 && textureSize == 128) {
                unsigned char* resized = resizeTexture(albedo, w, h, textureSize, textureSize);
                stbi_image_free(albedo);
                albedo = resized;
                w = textureSize;
                h = textureSize;
                LOG_DEBUG("Resized texture: " + baseName + " from 256x256 to 128x128");
            } else {
                // Skip other non-standard sizes
                stbi_image_free(albedo);
                continue;
            }
        }
        
        int index = static_cast<int>(albedoData.size());
        albedoTextureMap[baseName] = index;
        albedoData.push_back(albedo);
        albedoNames.push_back(baseName);
        
        // Try to load normal map
        unsigned char* normal = nullptr;
        bool normalFromStbi = false;
        if (fs::exists(normalPath)) {
            normal = loadImage(normalPath, w, h);
            if (normal && (w != textureSize || h != textureSize)) {
                // Try to resize 256x256 normal maps
                if (w == 256 && h == 256 && textureSize == 128) {
                    unsigned char* resized = resizeTexture(normal, w, h, textureSize, textureSize);
                    stbi_image_free(normal);
                    normal = resized;
                    normalFromStbi = false;  // resized uses new[], not stbi
                } else {
                    stbi_image_free(normal);
                    normal = nullptr;
                }
            } else if (normal) {
                normalFromStbi = true;
            }
        }
        if (!normal) {
            normal = createDefaultNormalMap();
            normalFromStbi = false;
        }
        normalTextureMap[baseName] = index;
        normalData.push_back(normal);
        normalNames.push_back(baseName);
        normalFromStbiFlags.push_back(normalFromStbi);
        
        // Try to load specular map
        unsigned char* specular = nullptr;
        bool specularFromStbi = false;
        if (fs::exists(specularPath)) {
            specular = loadImage(specularPath, w, h);
            if (specular && (w != textureSize || h != textureSize)) {
                // Try to resize 256x256 specular maps
                if (w == 256 && h == 256 && textureSize == 128) {
                    unsigned char* resized = resizeTexture(specular, w, h, textureSize, textureSize);
                    stbi_image_free(specular);
                    specular = resized;
                    specularFromStbi = false;  // resized uses new[], not stbi
                } else {
                    stbi_image_free(specular);
                    specular = nullptr;
                }
            } else if (specular) {
                specularFromStbi = true;
            }
        }
        if (!specular) {
            specular = createDefaultSpecularMap();
            specularFromStbi = false;
        }
        specularTextureMap[baseName] = index;
        specularData.push_back(specular);
        specularNames.push_back(baseName);
        specularFromStbiFlags.push_back(specularFromStbi);
    }
    
    textureCount = static_cast<int>(albedoData.size());
    
    if (textureCount == 0) {
        LOG_ERROR("No valid textures found in resource pack");
        return false;
    }
    
    LOG_INFO("Loaded " + std::to_string(textureCount) + " textures from resource pack");
    
    // Create texture arrays
    bool success = createTextureArrays(albedoData, normalData, specularData);
    
    // Free CPU-side texture data with correct deallocation method
    for (auto* data : albedoData) stbi_image_free(data);
    for (size_t i = 0; i < normalData.size(); i++) {
        if (normalFromStbiFlags[i]) stbi_image_free(normalData[i]);
        else delete[] normalData[i];
    }
    for (size_t i = 0; i < specularData.size(); i++) {
        if (specularFromStbiFlags[i]) stbi_image_free(specularData[i]);
        else delete[] specularData[i];
    }
    
    return success;
}

bool ResourcePackManager::createTextureArrays(const std::vector<unsigned char*>& albedoData,
                                              const std::vector<unsigned char*>& normalData,
                                              const std::vector<unsigned char*>& specularData) {
    // Create albedo texture array
    glGenTextures(1, &albedoTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, albedoTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, textureSize, textureSize, textureCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    for (int i = 0; i < textureCount; i++) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, textureSize, textureSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, albedoData[i]);
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    
    // Create normal texture array
    glGenTextures(1, &normalTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, normalTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, textureSize, textureSize, textureCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    for (int i = 0; i < textureCount; i++) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, textureSize, textureSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, normalData[i]);
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    
    // Create specular texture array
    glGenTextures(1, &specularTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, specularTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, textureSize, textureSize, textureCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    for (int i = 0; i < textureCount; i++) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, textureSize, textureSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, specularData[i]);
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    return true;
}

void ResourcePackManager::setupBlockMappings() {
    // Helper lambda to find texture index with fallback
    auto findTex = [this](const std::string& name) -> int {
        auto it = albedoTextureMap.find(name);
        if (it != albedoTextureMap.end()) {
            LOG_DEBUG("Found texture: " + name + " at index " + std::to_string(it->second));
            return it->second;
        }
        LOG_WARNING("Texture not found: " + name);
        return -1;
    };
    
    // Helper to find first available texture from a list
    auto findFirstTex = [this](std::initializer_list<std::string> names) -> int {
        for (const auto& name : names) {
            auto it = albedoTextureMap.find(name);
            if (it != albedoTextureMap.end()) {
                LOG_DEBUG("Found texture (from list): " + name + " at index " + std::to_string(it->second));
                return it->second;
            }
        }
        std::string tried;
        for (const auto& name : names) tried += name + ", ";
        LOG_WARNING("No texture found from list: " + tried);
        return -1;
    };
    
    // GRASS block - grass_block_top for top, dirt for bottom, grass_block_side_overlay or dirt for sides
    {
        BlockTextureMapping mapping;
        int topIdx = findFirstTex({"grass_block_top"});
        int dirtIdx = findTex("dirt");
        // Grass block side overlay has transparency - we need dirt underneath
        // For now just use dirt for sides, the overlay is incomplete without blending
        int sideIdx = dirtIdx; // TODO: implement multi-layer blending for side overlay
        
        mapping.top.albedoIndex = (topIdx >= 0) ? topIdx : dirtIdx;
        mapping.bottom.albedoIndex = dirtIdx;
        mapping.side.albedoIndex = sideIdx;
        
        LOG_INFO("GRASS: top=" + std::to_string(mapping.top.albedoIndex) + 
                 " bottom=" + std::to_string(mapping.bottom.albedoIndex) +
                 " side=" + std::to_string(mapping.side.albedoIndex));
        blockMappings[BlockType::GRASS] = mapping;
    }
    
    // DIRT block
    {
        BlockTextureMapping mapping;
        int idx = findTex("dirt");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::DIRT] = mapping;
    }
    
    // STONE block
    {
        BlockTextureMapping mapping;
        int idx = findTex("stone");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::STONE] = mapping;
    }
    
    // SAND block
    {
        BlockTextureMapping mapping;
        int idx = findTex("sand");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SAND] = mapping;
    }
    
    // WATER - use blue ice as placeholder
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"blue_ice", "ice", "packed_ice"});
        waterStillIndex = idx;
        waterFlowIndex = idx;
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::WATER] = mapping;
    }
    
    // WOOD (Planks)
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"oak_planks", "birch_planks", "spruce_planks"});
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::WOOD] = mapping;
    }
    
    // LEAVES
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"oak_leaves", "birch_leaves", "spruce_leaves", "acacia_leaves"});
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::LEAVES] = mapping;
    }
    
    // SNOW
    {
        BlockTextureMapping mapping;
        int idx = findTex("snow");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SNOW] = mapping;
    }
    
    // ICE
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"ice", "blue_ice", "packed_ice"});
        iceIndex = idx;
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::ICE] = mapping;
    }
    
    // GRAVEL
    {
        BlockTextureMapping mapping;
        int idx = findTex("gravel");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GRAVEL] = mapping;
    }
    
    // SANDSTONE
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findFirstTex({"sandstone_top", "sandstone"});
        mapping.bottom.albedoIndex = findFirstTex({"sandstone_top", "sandstone"});
        mapping.side.albedoIndex = findTex("sandstone");
        blockMappings[BlockType::SANDSTONE] = mapping;
    }
    
    // LOG
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findFirstTex({"oak_log_top", "birch_log_top", "spruce_log_top"});
        mapping.bottom.albedoIndex = findFirstTex({"oak_log_top", "birch_log_top", "spruce_log_top"});
        mapping.side.albedoIndex = findFirstTex({"oak_log", "birch_log", "spruce_log"});
        blockMappings[BlockType::LOG] = mapping;
    }
    
    // TALL_GRASS - use the actual grass/tall_grass textures (now resized from 256x256 to 128x128)
    {
        BlockTextureMapping mapping;
        // Try tall_grass_bottom first, then grass, grass1, grass2
        int idx = findFirstTex({"tall_grass_bottom", "grass", "grass1", "grass2", "grass_block_foliage"});
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        LOG_INFO("TALL_GRASS: idx=" + std::to_string(idx));
        blockMappings[BlockType::TALL_GRASS] = mapping;
    }
    
    // ROSE (Flower) - use dandelion or poppy (these have transparency for the flower shape)
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"dandelion", "poppy", "blue_orchid", "allium"});
        if (idx < 0) {
            idx = findFirstTex({"grass", "tall_grass_bottom"});  // fallback to grass
        }
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        LOG_INFO("ROSE: idx=" + std::to_string(idx));
        blockMappings[BlockType::ROSE] = mapping;
    }
    
    // BEDROCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("bedrock");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BEDROCK] = mapping;
    }
    
    // AIR - no texture needed but add for safety
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = 0;
        mapping.bottom.albedoIndex = 0;
        mapping.side.albedoIndex = 0;
        blockMappings[BlockType::AIR] = mapping;
    }
    
    // Log loaded textures for debugging
    LOG_INFO("Block texture mappings created. Total textures: " + std::to_string(textureCount));
}

int ResourcePackManager::getTextureIndex(BlockType type, int normalDirection) const {
    // Get stone index for fallback
    auto stoneIt = albedoTextureMap.find("stone");
    int fallbackIdx = (stoneIt != albedoTextureMap.end()) ? stoneIt->second : 0;
    
    auto it = blockMappings.find(type);
    if (it == blockMappings.end()) {
        return fallbackIdx;
    }
    
    const auto& mapping = it->second;
    
    // normalDirection: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    switch (normalDirection) {
        case 2:  // +Y (top)
            return mapping.top.albedoIndex >= 0 ? mapping.top.albedoIndex : fallbackIdx;
        case 3:  // -Y (bottom)
            return mapping.bottom.albedoIndex >= 0 ? mapping.bottom.albedoIndex : fallbackIdx;
        default: // sides
            return mapping.side.albedoIndex >= 0 ? mapping.side.albedoIndex : fallbackIdx;
    }
}

int ResourcePackManager::getNormalMapIndex(BlockType type, int normalDirection) const {
    // For now, normal maps use the same index as albedo
    // The actual normal texture is in the normal texture array at that index
    return getTextureIndex(type, normalDirection);
}

int ResourcePackManager::getSpecularMapIndex(BlockType type, int normalDirection) const {
    // For now, specular maps use the same index as albedo
    return getTextureIndex(type, normalDirection);
}
