#define NOMINMAX
#include "DebugInfoSystem.h"

#include "../Core/Time.h"
#include "../Render/Camera.h"
#include "../Render/Renderer.h"
#include "../UI/UIManager.h"
#include "../World/ChunkManager.h"

#include <glm/glm.hpp>

DebugInfoSystem::DebugInfoSystem(UIManager& uiManagerRef,
                                 Camera& cameraRef,
                                 ChunkManager& chunkManagerRef,
                                 Renderer& rendererRef)
    : uiManager(uiManagerRef),
      camera(cameraRef),
      chunkManager(chunkManagerRef),
      renderer(rendererRef) {
}

void DebugInfoSystem::update(float deltaTime) {
    fpsAccumulator += Time::instance().getFPS();
    frameAccumulator++;
    fpsUpdateTimer += deltaTime;

    if (fpsUpdateTimer >= 0.5f) {
        displayFPS = fpsAccumulator / frameAccumulator;
        fpsAccumulator = 0.0f;
        frameAccumulator = 0;
        fpsUpdateTimer = 0.0f;
    }

    std::string blockName = "None";
    glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
    auto result = chunkManager.rayCast(eyePos, camera.getFront(), 100.0f);
    if (result.hit) {
        glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
        int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
        int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
        int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;
        Block block = chunkManager.getBlockAt(x, y, z);
        blockName = uiManager.getBlockName(block.getType());
    }

    float taaMotion = 0.0f;
    float taaHistoryWeight = 0.0f;
    if (renderer.getPostProcess()) {
        taaMotion = renderer.getPostProcess()->getLastTaaMotionMag();
        taaHistoryWeight = renderer.getPostProcess()->getLastTaaBlendEstimate();
    }

    uiManager.updateDebugInfo(displayFPS, blockName, camera.getPosition(), camera.velocity, taaMotion, taaHistoryWeight);
}
