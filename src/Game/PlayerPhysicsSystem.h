#pragma once

#include <glm/glm.hpp>

class Camera;
class ChunkManager;
class Window;

class PlayerPhysicsSystem {
public:
    PlayerPhysicsSystem(Camera& camera, ChunkManager& chunkManager);

    void update(float deltaTime, Window& window);

private:
    bool checkCollision(const glm::vec3& pos) const;

    Camera& camera;
    ChunkManager& chunkManager;
};
