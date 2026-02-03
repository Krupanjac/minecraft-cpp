#pragma once

#include <glm/glm.hpp>

class UIManager;
class Camera;
class ChunkManager;

class AudioSystem {
public:
    AudioSystem(UIManager& uiManager,
                Camera& camera,
                ChunkManager& chunkManager,
                bool& isUnderwater,
                bool& wasUnderwater);

    bool initialize();
    void update(float deltaTime);

private:
    void updateAmbientState();
    void updateFootsteps(float deltaTime);
    void updateSwimSounds(float deltaTime);

    UIManager& uiManager;
    Camera& camera;
    ChunkManager& chunkManager;
    bool& isUnderwater;
    bool& wasUnderwater;

    float footstepTimer = 0.0f;
    float footstepInterval = 0.4f;
    float swimTimer = 0.0f;
    float swimInterval = 0.6f;
};
