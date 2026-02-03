#include "PlayerEntitySystem.h"

#include "../Render/Camera.h"
#include "../Entity/PlayerEntity.h"
#include "../World/Item.h"

PlayerEntitySystem::PlayerEntitySystem(Camera& cameraRef, std::unique_ptr<PlayerEntity>& playerEntityRef)
    : camera(cameraRef),
      playerEntity(playerEntityRef) {
}

void PlayerEntitySystem::syncHeldItem(ItemType currentHeldItem) {
    if (playerEntity) {
        playerEntity->setHeldItem(currentHeldItem);
    }
}

void PlayerEntitySystem::updateWithCamera(float deltaTime) {
    if (!playerEntity) return;

    playerEntity->setVelocity(camera.velocity);
    playerEntity->updateWithCamera(deltaTime, camera);
}
