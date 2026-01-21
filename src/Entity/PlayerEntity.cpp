#include "PlayerEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"

PlayerEntity::PlayerEntity(const glm::vec3& startPos) : Entity(startPos) {
    loadModelFromSettings();
}

void PlayerEntity::loadModelFromSettings() {
    auto& settings = Settings::instance();
    int modelIndex = settings.playerModelIndex;
    
    // Validate index
    if (modelIndex < 0 || modelIndex >= Settings::NUM_PLAYER_MODELS) {
        modelIndex = 0;
    }
    
    std::string modelPath = Settings::PLAYER_MODEL_PATHS[modelIndex];
    LOG_INFO("Loading player model: " + modelPath);
    
    auto playerModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(playerModel);
    
    // Different scales for different models
    if (modelIndex == 0) {
        // Half-Life model - needs smaller scale
        setScale(glm::vec3(0.03f));
        rotationOffset = glm::vec3(0.0f, 0.0f, 0.0f);
    } else {
        // Quaternius characters - larger scale
        setScale(glm::vec3(0.5f));
        rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    }
    setRotation(rotationOffset);
    
    currentModelIndex = modelIndex;
}

void PlayerEntity::update(float deltaTime) {
    // Check if model needs to be reloaded (settings changed)
    if (Settings::instance().playerModelIndex != currentModelIndex) {
        loadModelFromSettings();
    }
    
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
