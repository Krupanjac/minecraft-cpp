#pragma once
#include "Entity.h"

class PlayerEntity : public Entity {
public:
    PlayerEntity(const glm::vec3& startPos);
    ~PlayerEntity() override = default;

    void update(float deltaTime) override;
    
    // Reload model from settings (called when player changes model in menu)
    void loadModelFromSettings();

private:
    int currentModelIndex = -1;
    glm::vec3 rotationOffset = glm::vec3(0.0f);
};
