#include "PlayerEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

PlayerEntity::PlayerEntity(const glm::vec3& startPos) : Entity(startPos) {
    // Load player model
    // Path is relative to executable (or asset root)
    std::string modelPath = "assets/models/Player/scene.gltf";
    
    // In a real engine, we'd use a ResourceManager to avoid multiple loads
    // For now, load directly
    auto playerModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(playerModel);
    
    // Scale down if needed? Minecraft chars are ~1.8m tall. 
    // Assuming model is in meters or units.
    setScale(glm::vec3(0.03f)); 
}

void PlayerEntity::update(float deltaTime) {
    Entity::update(deltaTime);
    
    if (model) {
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        std::string currentAnim = model->getCurrentAnimation();
        
        // Simple state machine
        if (speed > 0.1f) {
            // Check if we are already playing a walk/run animation
            if (currentAnim != "walk" && currentAnim != "run" && currentAnim.find("walk") == std::string::npos) {
                // Try to find a walk animation. 
                // Using "walk" as a guess, assuming the model has one.
                model->playAnimation("walk", true);
            }
        } else {
             // Idle
             if (currentAnim != "idle" && currentAnim.find("idle") == std::string::npos) {
                  model->playAnimation("idle", true);
                  // If "idle" not found, try "idle1" based on user list
                  if (model->getCurrentAnimation() != "idle") model->playAnimation("idle1", true);
             }
        }
    }
}
