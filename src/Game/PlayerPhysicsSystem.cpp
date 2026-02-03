#define NOMINMAX
#include "PlayerPhysicsSystem.h"

#include "../Core/Window.h"
#include "../Render/Camera.h"
#include "../World/Block.h"
#include "../World/ChunkManager.h"

#include <algorithm>
#include <cmath>

PlayerPhysicsSystem::PlayerPhysicsSystem(Camera& cameraRef, ChunkManager& chunkManagerRef)
    : camera(cameraRef),
      chunkManager(chunkManagerRef) {
}

void PlayerPhysicsSystem::update(float deltaTime, Window& window) {
    if (camera.getFlightMode()) return;

    bool inWater = false;
    glm::vec3 camPos = camera.getPosition();

    Block headBlock = chunkManager.getBlockAt(static_cast<int>(std::floor(camPos.x)), static_cast<int>(std::floor(camPos.y + 1.6f)), static_cast<int>(std::floor(camPos.z)));
    Block feetBlock = chunkManager.getBlockAt(static_cast<int>(std::floor(camPos.x)), static_cast<int>(std::floor(camPos.y)), static_cast<int>(std::floor(camPos.z)));

    if (headBlock.isWater() || feetBlock.isWater()) {
        inWater = true;
    }

    if (inWater) {
        float drag = 1.0f - (2.0f * deltaTime);
        drag = (std::max)(0.0f, drag);
        camera.velocity.x *= drag;
        camera.velocity.z *= drag;
        camera.velocity.y *= drag;

        if (window.isKeyPressed(GLFW_KEY_SPACE)) {
            camera.velocity.y += 10.0f * deltaTime;
        } else if (window.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
            camera.velocity.y -= 10.0f * deltaTime;
        }

        if (!window.isKeyPressed(GLFW_KEY_SPACE)) {
            camera.velocity.y -= 2.0f * deltaTime;
        }

        camera.velocity.y = (std::max)(-4.0f, (std::min)(4.0f, camera.velocity.y));
    } else {
        camera.velocity.y -= 32.0f * deltaTime;
        camera.velocity.y = (std::max)(-78.4f, camera.velocity.y);
    }

    glm::vec3 pos = camera.getPosition();
    glm::vec3 vel = camera.velocity * deltaTime;

    if (checkCollision(glm::vec3(pos.x + vel.x, pos.y, pos.z))) {
        vel.x = 0;
        camera.velocity.x = 0;
    }
    pos.x += vel.x;

    if (checkCollision(glm::vec3(pos.x, pos.y, pos.z + vel.z))) {
        vel.z = 0;
        camera.velocity.z = 0;
    }
    pos.z += vel.z;

    if (checkCollision(glm::vec3(pos.x, pos.y + vel.y, pos.z))) {
        if (vel.y < 0) camera.onGround = true;
        vel.y = 0;
        camera.velocity.y = 0;
    } else {
        camera.onGround = false;
    }
    pos.y += vel.y;

    camera.setPosition(pos);
}

bool PlayerPhysicsSystem::checkCollision(const glm::vec3& pos) const {
    float minX = pos.x - 0.3f;
    float maxX = pos.x + 0.3f;
    float minY = pos.y;
    float maxY = pos.y + 1.8f;
    float minZ = pos.z - 0.3f;
    float maxZ = pos.z + 0.3f;

    for (int x = static_cast<int>(std::floor(minX)); x <= static_cast<int>(std::floor(maxX)); x++) {
        for (int y = static_cast<int>(std::floor(minY)); y <= static_cast<int>(std::floor(maxY)); y++) {
            for (int z = static_cast<int>(std::floor(minZ)); z <= static_cast<int>(std::floor(maxZ)); z++) {
                Block block = chunkManager.getBlockAt(x, y, z);
                if (block.isSolid()) return true;
            }
        }
    }
    return false;
}
