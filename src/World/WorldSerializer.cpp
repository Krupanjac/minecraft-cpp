#include "WorldSerializer.h"
#include "../Core/Logger.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace fs = std::filesystem;

std::string WorldSerializer::getSaveDirectory() {
    return "saves";
}

std::string WorldSerializer::getWorldDirectory(const std::string& worldName) {
    return getSaveDirectory() + "/" + worldName;
}

bool WorldSerializer::saveWorld(const std::string& worldName, const ChunkManager& chunkManager, const glm::vec3& playerPos, long seed) {
    std::string worldDir = getWorldDirectory(worldName);
    
    if (!fs::exists(worldDir)) {
        fs::create_directories(worldDir);
    }
    
    // Save level.dat
    std::ofstream levelFile(worldDir + "/level.dat", std::ios::binary);
    if (!levelFile.is_open()) {
        LOG_ERROR("Failed to open level.dat for writing");
        return false;
    }
    
    levelFile.write(reinterpret_cast<const char*>(&playerPos), sizeof(glm::vec3));
    levelFile.write(reinterpret_cast<const char*>(&seed), sizeof(long));
    
    levelFile.close();
    
    // Save chunks to a single file
    std::string chunksFile = worldDir + "/chunks.dat";
    std::ofstream file(chunksFile, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open chunks.dat for writing");
        return false;
    }

    const auto& chunks = chunkManager.getChunks();
    int savedCount = 0;
    
    // Count modified chunks first
    for (const auto& [pos, chunk] : chunks) {
        if (chunk->isModified()) {
            savedCount++;
        }
    }
    
    // Write header: Magic, Version, Count
    uint32_t magic = 0x4D434350; // MCCP (Minecraft C++ Project)
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&savedCount), sizeof(int));
    
    for (const auto& [pos, chunk] : chunks) {
        if (chunk->isModified()) {
            // Write ChunkPos
            file.write(reinterpret_cast<const char*>(&pos), sizeof(ChunkPos));
            
            // Write Block Data (Fixed size 4096 * sizeof(Block))
            const auto& blocks = chunk->getBlocks();
            file.write(reinterpret_cast<const char*>(blocks.data()), blocks.size() * sizeof(Block));
        }
    }
    
    file.close();
    
    LOG_INFO("Saved world '" + worldName + "' with " + std::to_string(savedCount) + " modified chunks.");
    return true;
}

bool WorldSerializer::loadWorld(const std::string& worldName, ChunkManager& chunkManager, glm::vec3& playerPos, long& seed) {
    std::string worldDir = getWorldDirectory(worldName);
    if (!fs::exists(worldDir)) return false;
    
    // Load level.dat
    std::ifstream levelFile(worldDir + "/level.dat", std::ios::binary);
    if (levelFile.is_open()) {
        levelFile.read(reinterpret_cast<char*>(&playerPos), sizeof(glm::vec3));
        if (levelFile.peek() != EOF) {
             levelFile.read(reinterpret_cast<char*>(&seed), sizeof(long));
        }
        levelFile.close();
    }
    
    // Load chunks from single file
    std::string chunksFile = worldDir + "/chunks.dat";
    if (fs::exists(chunksFile)) {
        std::ifstream file(chunksFile, std::ios::binary);
        if (file.is_open()) {
            uint32_t magic;
            uint32_t version;
            int count;
            
            file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
            file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
            file.read(reinterpret_cast<char*>(&count), sizeof(int));
            
            if (magic == 0x4D434350 && version == 1) {
                for (int i = 0; i < count; i++) {
                    ChunkPos pos;
                    file.read(reinterpret_cast<char*>(&pos), sizeof(ChunkPos));
                    
                    std::vector<Block> blocks(CHUNK_VOLUME);
                    file.read(reinterpret_cast<char*>(blocks.data()), CHUNK_VOLUME * sizeof(Block));
                    
                    chunkManager.preloadChunkData(pos, blocks);
                }
                LOG_INFO("Loaded " + std::to_string(count) + " chunks from disk.");
            }
            file.close();
        }
    }
    
    return true;
}

std::vector<std::string> WorldSerializer::getAvailableWorlds() {
    std::vector<std::string> worlds;
    std::string saveDir = getSaveDirectory();
    
    if (fs::exists(saveDir)) {
        for (const auto& entry : fs::directory_iterator(saveDir)) {
            if (entry.is_directory()) {
                worlds.push_back(entry.path().filename().string());
            }
        }
    }
    
    return worlds;
}

bool WorldSerializer::createNewWorld(const std::string& worldName, long seed) {
    std::string worldDir = getWorldDirectory(worldName);
    if (fs::exists(worldDir)) return false; // Already exists
    
    fs::create_directories(worldDir);
    
    // Save initial level.dat with seed
    std::ofstream levelFile(worldDir + "/level.dat", std::ios::binary);
    if (levelFile.is_open()) {
        glm::vec3 defaultPos(0, 80, 0);
        levelFile.write(reinterpret_cast<const char*>(&defaultPos), sizeof(glm::vec3));
        levelFile.write(reinterpret_cast<const char*>(&seed), sizeof(long));
        levelFile.close();
        return true;
    }
    return false;
}

bool WorldSerializer::saveScreenshot(const std::string& worldName, const unsigned char* pixels, int width, int height) {
    std::string worldDir = getWorldDirectory(worldName);
    
    if (!fs::exists(worldDir)) {
        fs::create_directories(worldDir);
    }
    
    std::string screenshotPath = worldDir + "/preview.png";
    
    // Flip the image vertically (OpenGL has origin at bottom-left)
    std::vector<unsigned char> flipped(width * height * 3);
    for (int y = 0; y < height; y++) {
        memcpy(&flipped[y * width * 3], &pixels[(height - 1 - y) * width * 3], width * 3);
    }
    
    // Save as PNG using stb_image_write
    int result = stbi_write_png(screenshotPath.c_str(), width, height, 3, flipped.data(), width * 3);
    
    if (result) {
        LOG_INFO("Saved world preview screenshot: " + screenshotPath);
        return true;
    } else {
        LOG_ERROR("Failed to save world preview screenshot");
        return false;
    }
}

std::string WorldSerializer::getScreenshotPath(const std::string& worldName) {
    return getWorldDirectory(worldName) + "/preview.png";
}

bool WorldSerializer::hasScreenshot(const std::string& worldName) {
    return fs::exists(getScreenshotPath(worldName));
}
