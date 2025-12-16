#pragma once
#include "Entity.h"

class PlayerEntity : public Entity {
public:
    PlayerEntity(const glm::vec3& startPos);
    ~PlayerEntity() override = default;

    void update(float deltaTime) override;
};
