#pragma once

#include "ChunkManager.h"
#include <string>
#include <vector>

class WorldSerializer {
public:
    static bool saveWorld(const std::string& worldName, const ChunkManager& chunkManager, const glm::vec3& playerPos, long seed);
    static bool loadWorld(const std::string& worldName, ChunkManager& chunkManager, glm::vec3& playerPos, long& seed);
    static std::vector<std::string> getAvailableWorlds();
    static bool createNewWorld(const std::string& worldName, long seed);
    
private:
    static std::string getSaveDirectory();
    static std::string getWorldDirectory(const std::string& worldName);
};
