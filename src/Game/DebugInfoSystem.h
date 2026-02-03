#pragma once

#include <string>

class UIManager;
class Camera;
class ChunkManager;
class Renderer;

class DebugInfoSystem {
public:
    DebugInfoSystem(UIManager& uiManager,
                    Camera& camera,
                    ChunkManager& chunkManager,
                    Renderer& renderer);

    void update(float deltaTime);

private:
    UIManager& uiManager;
    Camera& camera;
    ChunkManager& chunkManager;
    Renderer& renderer;

    float displayFPS = 0.0f;
    float fpsAccumulator = 0.0f;
    int frameAccumulator = 0;
    float fpsUpdateTimer = 0.0f;
};
