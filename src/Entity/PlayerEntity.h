#pragma once
#include "Entity.h"

class Camera; // Forward declaration

class PlayerEntity : public Entity {
public:
    PlayerEntity(const glm::vec3& startPos);
    ~PlayerEntity() override = default;

    void update(float deltaTime) override;
    
    // Update with camera state for jump detection
    void updateWithCamera(float deltaTime, const Camera& camera);
    
    // Reload model from settings (called when player changes model in menu)
    void loadModelFromSettings();

private:
    int currentModelIndex = -1;
    glm::vec3 rotationOffset = glm::vec3(0.0f);
    
    // Animation state tracking
    bool wasOnGround = true;
    bool isJumping = false;
    float jumpAnimTimer = 0.0f;
    
    std::string idleAnim = "Idle";
    std::string walkAnim = "Walk";
    std::string runAnim = "Run";
    std::string jumpAnim = "Jump";
    std::string jumpIdleAnim = "Jump_Idle";
    std::string jumpLandAnim = "Jump_Land";
    
    void pickAnimations();
};
