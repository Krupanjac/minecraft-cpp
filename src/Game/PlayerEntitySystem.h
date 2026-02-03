#pragma once

#include <memory>

#include "../World/Item.h"

class Camera;
class PlayerEntity;

class PlayerEntitySystem {
public:
    PlayerEntitySystem(Camera& camera, std::unique_ptr<PlayerEntity>& playerEntity);

    void syncHeldItem(ItemType currentHeldItem);
    void updateWithCamera(float deltaTime);

private:
    Camera& camera;
    std::unique_ptr<PlayerEntity>& playerEntity;
};
