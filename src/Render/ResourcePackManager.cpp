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

unsigned char* ResourcePackManager::compositeTextures(unsigned char* base, unsigned char* overlay, 
                                                       int width, int height, 
                                                       float tintR, float tintG, float tintB) {
    // Composite overlay on top of base using alpha blending
    // The overlay is tinted with the given color before blending (for biome colors)
    unsigned char* result = new unsigned char[width * height * 4];
    
    for (int i = 0; i < width * height; i++) {
        // Get base color
        float baseR = base[i * 4 + 0] / 255.0f;
        float baseG = base[i * 4 + 1] / 255.0f;
        float baseB = base[i * 4 + 2] / 255.0f;
        float baseA = base[i * 4 + 3] / 255.0f;
        
        // Get overlay color and apply tint (overlay is grayscale, tint gives it color)
        float overlayGray = overlay[i * 4 + 0] / 255.0f;  // Grayscale value
        float overlayR = overlayGray * tintR;
        float overlayG = overlayGray * tintG;
        float overlayB = overlayGray * tintB;
        float overlayA = overlay[i * 4 + 3] / 255.0f;
        
        // Alpha blending: result = overlay * overlayA + base * (1 - overlayA)
        float outR = overlayR * overlayA + baseR * (1.0f - overlayA);
        float outG = overlayG * overlayA + baseG * (1.0f - overlayA);
        float outB = overlayB * overlayA + baseB * (1.0f - overlayA);
        float outA = overlayA + baseA * (1.0f - overlayA);
        
        result[i * 4 + 0] = static_cast<unsigned char>(outR * 255.0f);
        result[i * 4 + 1] = static_cast<unsigned char>(outG * 255.0f);
        result[i * 4 + 2] = static_cast<unsigned char>(outB * 255.0f);
        result[i * 4 + 3] = static_cast<unsigned char>(outA * 255.0f);
    }
    
    return result;
}

unsigned char* ResourcePackManager::tintTexture(unsigned char* src, int width, int height,
                                                 float tintR, float tintG, float tintB) {
    // Apply color tint to a grayscale texture
    unsigned char* result = new unsigned char[width * height * 4];
    
    for (int i = 0; i < width * height; i++) {
        float gray = src[i * 4 + 0] / 255.0f;  // Assume grayscale (R=G=B)
        result[i * 4 + 0] = static_cast<unsigned char>(gray * tintR * 255.0f);
        result[i * 4 + 1] = static_cast<unsigned char>(gray * tintG * 255.0f);
        result[i * 4 + 2] = static_cast<unsigned char>(gray * tintB * 255.0f);
        result[i * 4 + 3] = src[i * 4 + 3];  // Preserve alpha
    }
    
    return result;
}

void ResourcePackManager::fixTransparentPixels(unsigned char* data, int width, int height) {
    // Fix transparent/semi-transparent pixels to prevent edge bleeding when filtering/mipmapping
    // For pixels with low alpha, copy RGB from nearest opaque neighbor (alpha > 128)
    // This prevents white halos around vegetation edges
    
    // First pass: collect all low-alpha pixel positions
    std::vector<std::pair<int, int>> lowAlphaPixels;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            if (data[idx + 3] < 128) {  // Also fix semi-transparent pixels
                lowAlphaPixels.push_back({x, y});
            }
        }
    }
    
    // For each low-alpha pixel, find nearest opaque pixel and copy its color
    for (const auto& [px, py] : lowAlphaPixels) {
        // Search in expanding squares for nearest opaque pixel
        int maxDist = 16;  // Limit search distance for performance
        bool found = false;
        
        for (int dist = 1; dist < maxDist && !found; dist++) {
            // Check pixels at this manhattan distance
            for (int dx = -dist; dx <= dist && !found; dx++) {
                for (int dy = -dist; dy <= dist && !found; dy++) {
                    if (std::abs(dx) != dist && std::abs(dy) != dist) continue;
                    
                    int nx = px + dx;
                    int ny = py + dy;
                    
                    // Skip out of bounds (don't wrap for vegetation)
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    
                    int nidx = (ny * width + nx) * 4;
                    if (data[nidx + 3] >= 128) {  // Found opaque enough pixel
                        // Copy its RGB to the low-alpha pixel
                        int pidx = (py * width + px) * 4;
                        data[pidx + 0] = data[nidx + 0];
                        data[pidx + 1] = data[nidx + 1];
                        data[pidx + 2] = data[nidx + 2];
                        // Keep original alpha
                        found = true;
                    }
                }
            }
        }
        
        // If no opaque neighbor found, set to black (prevents white)
        if (!found) {
            int pidx = (py * width + px) * 4;
            data[pidx + 0] = 0;
            data[pidx + 1] = 0;
            data[pidx + 2] = 0;
        }
    }
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
        
        // Fix transparent pixels to prevent white edge bleeding during mipmapping
        // This copies RGB from nearest opaque pixel into transparent pixels
        fixTransparentPixels(albedo, w, h);
        
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
    
    // Post-processing: Create composited grass_block_side texture (dirt + grass_block_side_overlay)
    // This combines the dirt texture with the grass overlay to create a proper grass side
    auto dirtIt = albedoTextureMap.find("dirt");
    auto overlayIt = albedoTextureMap.find("grass_block_side_overlay");
    auto overlayNIt = normalTextureMap.find("grass_block_side_overlay");
    auto overlaySIt = specularTextureMap.find("grass_block_side_overlay");
    auto dirtNIt = normalTextureMap.find("dirt");
    auto dirtSIt = specularTextureMap.find("dirt");
    
    if (dirtIt != albedoTextureMap.end() && overlayIt != albedoTextureMap.end()) {
        // Create composited grass side texture (dirt + tinted grass overlay)
        // Green tint for the grass overlay: RGB(0.5, 0.85, 0.4) = vibrant grass green
        unsigned char* grassSide = compositeTextures(
            albedoData[dirtIt->second], 
            albedoData[overlayIt->second],
            textureSize, textureSize,
            0.5f, 0.85f, 0.4f  // Green biome tint
        );
        
        // Add the composited texture to the arrays
        int grassSideIndex = static_cast<int>(albedoData.size());
        albedoTextureMap["grass_block_side_composited"] = grassSideIndex;
        albedoData.push_back(grassSide);
        albedoNames.push_back("grass_block_side_composited");
        
        // For normal map, composite dirt normal with overlay normal based on overlay alpha
        // This ensures the dirt portion looks consistent with standalone dirt blocks
        unsigned char* grassSideNormal = nullptr;
        if (dirtNIt != normalTextureMap.end()) {
            grassSideNormal = new unsigned char[textureSize * textureSize * 4];
            // Start with dirt normal
            memcpy(grassSideNormal, normalData[dirtNIt->second], textureSize * textureSize * 4);
            
            // If overlay normal exists, blend it based on overlay alpha
            if (overlayNIt != normalTextureMap.end() && overlayIt != albedoTextureMap.end()) {
                unsigned char* overlayNormal = normalData[overlayNIt->second];
                unsigned char* overlayAlbedo = albedoData[overlayIt->second];
                
                for (int i = 0; i < textureSize * textureSize; i++) {
                    float overlayAlpha = overlayAlbedo[i * 4 + 3] / 255.0f;
                    // Blend normal maps based on overlay alpha
                    for (int c = 0; c < 4; c++) {
                        float dirtVal = grassSideNormal[i * 4 + c] / 255.0f;
                        float overlayVal = overlayNormal[i * 4 + c] / 255.0f;
                        float blended = dirtVal * (1.0f - overlayAlpha) + overlayVal * overlayAlpha;
                        grassSideNormal[i * 4 + c] = static_cast<unsigned char>(blended * 255.0f);
                    }
                }
            }
        } else if (overlayNIt != normalTextureMap.end()) {
            // Fallback to overlay normal only
            grassSideNormal = new unsigned char[textureSize * textureSize * 4];
            memcpy(grassSideNormal, normalData[overlayNIt->second], textureSize * textureSize * 4);
        } else {
            grassSideNormal = createDefaultNormalMap();
        }
        normalTextureMap["grass_block_side_composited"] = grassSideIndex;
        normalData.push_back(grassSideNormal);
        normalNames.push_back("grass_block_side_composited");
        normalFromStbiFlags.push_back(false);  // We used new[]
        
        // For specular map, composite dirt specular with overlay specular
        unsigned char* grassSideSpec = nullptr;
        if (dirtSIt != specularTextureMap.end()) {
            grassSideSpec = new unsigned char[textureSize * textureSize * 4];
            memcpy(grassSideSpec, specularData[dirtSIt->second], textureSize * textureSize * 4);
            
            // Blend with overlay specular based on overlay alpha
            if (overlaySIt != specularTextureMap.end() && overlayIt != albedoTextureMap.end()) {
                unsigned char* overlaySpec = specularData[overlaySIt->second];
                unsigned char* overlayAlbedo = albedoData[overlayIt->second];
                
                for (int i = 0; i < textureSize * textureSize; i++) {
                    float overlayAlpha = overlayAlbedo[i * 4 + 3] / 255.0f;
                    for (int c = 0; c < 4; c++) {
                        float dirtVal = grassSideSpec[i * 4 + c] / 255.0f;
                        float overlayVal = overlaySpec[i * 4 + c] / 255.0f;
                        float blended = dirtVal * (1.0f - overlayAlpha) + overlayVal * overlayAlpha;
                        grassSideSpec[i * 4 + c] = static_cast<unsigned char>(blended * 255.0f);
                    }
                }
            }
        } else if (overlaySIt != specularTextureMap.end()) {
            grassSideSpec = new unsigned char[textureSize * textureSize * 4];
            memcpy(grassSideSpec, specularData[overlaySIt->second], textureSize * textureSize * 4);
        } else {
            grassSideSpec = createDefaultSpecularMap();
        }
        specularTextureMap["grass_block_side_composited"] = grassSideIndex;
        specularData.push_back(grassSideSpec);
        specularNames.push_back("grass_block_side_composited");
        specularFromStbiFlags.push_back(false);
        
        textureCount++;
        LOG_INFO("Created composited grass_block_side texture at index " + std::to_string(grassSideIndex));
    }
    
    // Also create a pre-tinted grass_block_top for better appearance
    auto grassTopIt = albedoTextureMap.find("grass_block_top");
    auto grassTopNIt = normalTextureMap.find("grass_block_top");
    auto grassTopSIt = specularTextureMap.find("grass_block_top");
    
    if (grassTopIt != albedoTextureMap.end()) {
        // Create tinted grass top (green biome tint)
        unsigned char* grassTopTinted = tintTexture(
            albedoData[grassTopIt->second],
            textureSize, textureSize,
            0.5f, 0.85f, 0.4f  // Green biome tint
        );
        
        int grassTopTintedIndex = static_cast<int>(albedoData.size());
        albedoTextureMap["grass_block_top_tinted"] = grassTopTintedIndex;
        albedoData.push_back(grassTopTinted);
        albedoNames.push_back("grass_block_top_tinted");
        
        // Copy normal map
        unsigned char* grassTopNormal = nullptr;
        if (grassTopNIt != normalTextureMap.end()) {
            grassTopNormal = new unsigned char[textureSize * textureSize * 4];
            memcpy(grassTopNormal, normalData[grassTopNIt->second], textureSize * textureSize * 4);
        } else {
            grassTopNormal = createDefaultNormalMap();
        }
        normalTextureMap["grass_block_top_tinted"] = grassTopTintedIndex;
        normalData.push_back(grassTopNormal);
        normalNames.push_back("grass_block_top_tinted");
        normalFromStbiFlags.push_back(false);
        
        // Copy specular map
        unsigned char* grassTopSpec = nullptr;
        if (grassTopSIt != specularTextureMap.end()) {
            grassTopSpec = new unsigned char[textureSize * textureSize * 4];
            memcpy(grassTopSpec, specularData[grassTopSIt->second], textureSize * textureSize * 4);
        } else {
            grassTopSpec = createDefaultSpecularMap();
        }
        specularTextureMap["grass_block_top_tinted"] = grassTopTintedIndex;
        specularData.push_back(grassTopSpec);
        specularNames.push_back("grass_block_top_tinted");
        specularFromStbiFlags.push_back(false);
        
        textureCount++;
        LOG_INFO("Created tinted grass_block_top texture at index " + std::to_string(grassTopTintedIndex));
    }
    
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
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Add anisotropic filtering for better quality at angles/distance
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAniso, 8.0f));
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
    
    // GRASS block - use pre-tinted/composited textures for proper appearance
    {
        BlockTextureMapping mapping;
        // Use the pre-tinted grass top (already has green biome color baked in)
        int topIdx = findFirstTex({"grass_block_top_tinted", "grass_block_top"});
        int dirtIdx = findTex("dirt");
        // Use the composited grass side (dirt + grass overlay with green tint)
        int sideIdx = findFirstTex({"grass_block_side_composited", "dirt"});
        
        mapping.top.albedoIndex = (topIdx >= 0) ? topIdx : dirtIdx;
        mapping.bottom.albedoIndex = dirtIdx;
        mapping.side.albedoIndex = (sideIdx >= 0) ? sideIdx : dirtIdx;
        
        LOG_INFO("GRASS: top=" + std::to_string(mapping.top.albedoIndex) + 
                 " bottom=" + std::to_string(mapping.bottom.albedoIndex) +
                 " side=" + std::to_string(mapping.side.albedoIndex) +
                 " dirtIdx=" + std::to_string(dirtIdx));
        blockMappings[BlockType::GRASS] = mapping;
        
        // Store dirt index for use by DIRT block
        grassDirtIndex = dirtIdx;
    }
    
    // DIRT block - MUST use same dirt texture as grass block bottom for consistency
    {
        BlockTextureMapping mapping;
        int idx = grassDirtIndex;  // Use the same index as grass block's dirt
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        LOG_INFO("DIRT: all faces=" + std::to_string(idx));
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
    
    // WATER - use ice as placeholder
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"ice"});
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
        int idx = findFirstTex({"oak_leaves", "birch_leaves", "spruce_leaves", "jungle_leaves"});
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
        int idx = findFirstTex({"ice"});
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
    
    // Collect grass/vegetation variants for randomization in shader
    grassVariants.clear();
    tallGrassVariants.clear();
    flowerVariants.clear();
    
    // Collect all grass variants
    for (const std::string& name : {"grass", "grass1", "grass2", "tall_grass_bottom", "tall_grass_top"}) {
        auto it = albedoTextureMap.find(name);
        if (it != albedoTextureMap.end()) {
            grassVariants.push_back(it->second);
            LOG_INFO("Added grass variant: " + name + " at index " + std::to_string(it->second));
        }
    }
    
    // Collect flower variants
    for (const std::string& name : {"dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", 
                                      "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
                                      "oxeye_daisy", "cornflower", "lily_of_the_valley"}) {
        auto it = albedoTextureMap.find(name);
        if (it != albedoTextureMap.end()) {
            flowerVariants.push_back(it->second);
            LOG_INFO("Added flower variant: " + name + " at index " + std::to_string(it->second));
        }
    }
    
    LOG_INFO("Collected " + std::to_string(grassVariants.size()) + " grass variants and " + 
             std::to_string(flowerVariants.size()) + " flower variants");
    
    // TALL_GRASS - use the first grass variant (shader will randomize)
    {
        BlockTextureMapping mapping;
        int idx = grassVariants.empty() ? findFirstTex({"grass", "grass_block_foliage"}) : grassVariants[0];
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        LOG_INFO("TALL_GRASS: idx=" + std::to_string(idx));
        blockMappings[BlockType::TALL_GRASS] = mapping;
    }
    
    // ROSE (Flower) - use dandelion as default (shader will randomize)
    {
        BlockTextureMapping mapping;
        int idx = flowerVariants.empty() ? findFirstTex({"dandelion", "grass"}) : flowerVariants[0];
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
    
    // === Additional blocks with full PBR support ===
    
    // COBBLESTONE - has full PBR (cobblestone.png, _n, _s)
    {
        BlockTextureMapping mapping;
        int idx = findTex("cobblestone");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::COBBLESTONE] = mapping;
        LOG_INFO("COBBLESTONE: idx=" + std::to_string(idx));
    }
    
    // COAL_ORE - has full PBR
    {
        BlockTextureMapping mapping;
        int idx = findTex("coal_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::COAL_ORE] = mapping;
        LOG_INFO("COAL_ORE: idx=" + std::to_string(idx));
    }
    
    // IRON_ORE - has full PBR  
    {
        BlockTextureMapping mapping;
        int idx = findTex("iron_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::IRON_ORE] = mapping;
        LOG_INFO("IRON_ORE: idx=" + std::to_string(idx));
    }
    
    // GOLD_ORE - has full PBR
    {
        BlockTextureMapping mapping;
        int idx = findTex("gold_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GOLD_ORE] = mapping;
        LOG_INFO("GOLD_ORE: idx=" + std::to_string(idx));
    }
    
    // DIAMOND_ORE - has full PBR
    {
        BlockTextureMapping mapping;
        int idx = findTex("diamond_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::DIAMOND_ORE] = mapping;
        LOG_INFO("DIAMOND_ORE: idx=" + std::to_string(idx));
    }
    
    // EMERALD_ORE
    {
        BlockTextureMapping mapping;
        int idx = findTex("emerald_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::EMERALD_ORE] = mapping;
    }
    
    // REDSTONE_ORE
    {
        BlockTextureMapping mapping;
        int idx = findTex("redstone_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::REDSTONE_ORE] = mapping;
    }
    
    // LAPIS_ORE
    {
        BlockTextureMapping mapping;
        int idx = findTex("lapis_ore");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::LAPIS_ORE] = mapping;
    }
    
    
    // MOSSY_COBBLESTONE
    {
        BlockTextureMapping mapping;
        int idx = findTex("mossy_cobblestone");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::MOSSY_COBBLESTONE] = mapping;
    }
    
    // STONE_BRICKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("stone_bricks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::STONE_BRICKS] = mapping;
    }
    
    // MOSSY_STONE_BRICKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("mossy_stone_bricks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::MOSSY_STONE_BRICKS] = mapping;
    }
    
    // CRACKED_STONE_BRICKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("cracked_stone_bricks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::CRACKED_STONE_BRICKS] = mapping;
    }
    
    // CHISELED_STONE_BRICKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("chiseled_stone_bricks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::CHISELED_STONE_BRICKS] = mapping;
    }
    
    
    // IRON_BLOCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("iron_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::IRON_BLOCK] = mapping;
    }
    
    // GOLD_BLOCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("gold_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GOLD_BLOCK] = mapping;
    }
    
    // DIAMOND_BLOCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("diamond_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::DIAMOND_BLOCK] = mapping;
    }
    
    // EMERALD_BLOCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("emerald_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::EMERALD_BLOCK] = mapping;
    }
    
    // REDSTONE_BLOCK
    {
        BlockTextureMapping mapping;
        int idx = findTex("redstone_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::REDSTONE_BLOCK] = mapping;
    }
    
    // BRICKS - has full PBR
    {
        BlockTextureMapping mapping;
        int idx = findTex("bricks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BRICKS] = mapping;
        LOG_INFO("BRICKS: idx=" + std::to_string(idx));
    }
    
    // OBSIDIAN
    {
        BlockTextureMapping mapping;
        int idx = findTex("obsidian");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::OBSIDIAN] = mapping;
    }
    
    
    // GLASS
    {
        BlockTextureMapping mapping;
        int idx = findTex("glass");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GLASS] = mapping;
    }
    
    // BOOKSHELF
    {
        BlockTextureMapping mapping;
        int sideIdx = findTex("bookshelf");
        int plankIdx = findFirstTex({"oak_planks", "birch_planks"});
        mapping.top.albedoIndex = plankIdx;
        mapping.bottom.albedoIndex = plankIdx;
        mapping.side.albedoIndex = sideIdx;
        blockMappings[BlockType::BOOKSHELF] = mapping;
    }
    
    // TNT
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("tnt_top");
        mapping.bottom.albedoIndex = findTex("tnt_bottom");
        mapping.side.albedoIndex = findTex("tnt_side");
        blockMappings[BlockType::TNT] = mapping;
    }
    
    // GLOWSTONE
    {
        BlockTextureMapping mapping;
        int idx = findTex("glowstone");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GLOWSTONE] = mapping;
    }
    
    
    // REDSTONE_LAMP
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"redstone_lamp_on", "redstone_lamp"});
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::REDSTONE_LAMP] = mapping;
    }
    
    // OAK_PLANKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("oak_planks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::OAK_PLANKS] = mapping;
    }
    
    // SPRUCE_PLANKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("spruce_planks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SPRUCE_PLANKS] = mapping;
    }
    
    // BIRCH_PLANKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("birch_planks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BIRCH_PLANKS] = mapping;
    }
    
    // JUNGLE_PLANKS
    {
        BlockTextureMapping mapping;
        int idx = findTex("jungle_planks");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::JUNGLE_PLANKS] = mapping;
    }
    
    
    // OAK_LOG
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("oak_log_top");
        mapping.bottom.albedoIndex = findTex("oak_log_top");
        mapping.side.albedoIndex = findTex("oak_log");
        blockMappings[BlockType::OAK_LOG] = mapping;
    }
    
    // SPRUCE_LOG
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("spruce_log_top");
        mapping.bottom.albedoIndex = findTex("spruce_log_top");
        mapping.side.albedoIndex = findTex("spruce_log");
        blockMappings[BlockType::SPRUCE_LOG] = mapping;
    }
    
    // BIRCH_LOG
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("birch_log_top");
        mapping.bottom.albedoIndex = findTex("birch_log_top");
        mapping.side.albedoIndex = findTex("birch_log");
        blockMappings[BlockType::BIRCH_LOG] = mapping;
    }
    
    // JUNGLE_LOG
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("jungle_log_top");
        mapping.bottom.albedoIndex = findTex("jungle_log_top");
        mapping.side.albedoIndex = findTex("jungle_log");
        blockMappings[BlockType::JUNGLE_LOG] = mapping;
    }
    
    
    // OAK_LEAVES
    {
        BlockTextureMapping mapping;
        int idx = findTex("oak_leaves");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::OAK_LEAVES] = mapping;
    }
    
    // SPRUCE_LEAVES
    {
        BlockTextureMapping mapping;
        int idx = findTex("spruce_leaves");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SPRUCE_LEAVES] = mapping;
    }
    
    // BIRCH_LEAVES
    {
        BlockTextureMapping mapping;
        int idx = findTex("birch_leaves");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BIRCH_LEAVES] = mapping;
    }
    
    // JUNGLE_LEAVES
    {
        BlockTextureMapping mapping;
        int idx = findTex("jungle_leaves");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::JUNGLE_LEAVES] = mapping;
    }
    
    
    // WOOL - All 16 colors
    {
        BlockTextureMapping mapping;
        int idx = findTex("white_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::WHITE_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("orange_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::ORANGE_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("magenta_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::MAGENTA_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("light_blue_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::LIGHT_BLUE_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findFirstTex({"yellow_wool", "white_wool"});
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::YELLOW_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("lime_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::LIME_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("pink_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::PINK_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("gray_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GRAY_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("light_gray_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::LIGHT_GRAY_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("cyan_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::CYAN_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("purple_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::PURPLE_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("blue_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BLUE_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("brown_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BROWN_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("green_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::GREEN_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("red_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::RED_WOOL] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("black_wool");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::BLACK_WOOL] = mapping;
    }
    
    
    // Sand variants
    {
        BlockTextureMapping mapping;
        int idx = findTex("chiseled_sandstone");
        mapping.top.albedoIndex = findTex("sandstone_top");
        mapping.bottom.albedoIndex = findTex("sandstone_top");
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::CHISELED_SANDSTONE] = mapping;
    }
    
    // Misc blocks
    {
        BlockTextureMapping mapping;
        int idx = findTex("clay");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::CLAY] = mapping;
        LOG_INFO("CLAY: idx=" + std::to_string(idx));
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("sponge");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SPONGE] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("cobweb");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::COBWEB] = mapping;
    }
    {
        BlockTextureMapping mapping;
        mapping.top.albedoIndex = findTex("crafting_table_top");
        mapping.bottom.albedoIndex = findFirstTex({"oak_planks", "birch_planks"});
        mapping.side.albedoIndex = findTex("crafting_table_side");
        blockMappings[BlockType::CRAFTING_TABLE] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("note_block");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::NOTE_BLOCK] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int topIdx = findTex("jukebox_top");
        int sideIdx = findTex("note_block");  // Use note block for sides
        mapping.top.albedoIndex = topIdx;
        mapping.bottom.albedoIndex = sideIdx;
        mapping.side.albedoIndex = sideIdx;
        blockMappings[BlockType::JUKEBOX] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("farmland");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = findTex("dirt");
        mapping.side.albedoIndex = findTex("dirt");
        blockMappings[BlockType::FARMLAND] = mapping;
    }
    {
        BlockTextureMapping mapping;
        int idx = findTex("sugar_cane");
        mapping.top.albedoIndex = idx;
        mapping.bottom.albedoIndex = idx;
        mapping.side.albedoIndex = idx;
        blockMappings[BlockType::SUGAR_CANE] = mapping;
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
