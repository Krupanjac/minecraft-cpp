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

static std::string toLowerPlayer(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

void PlayerEntity::update(float deltaTime) {
    // Check if model needs to be reloaded (settings changed)
    if (Settings::instance().playerModelIndex != currentModelIndex) {
        loadModelFromSettings();
    }
    
    Entity::update(deltaTime);
    
    if (model) {
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        std::string currentAnim = toLowerPlayer(model->getCurrentAnimation());
        
        // Update animation
        model->updateAnimation(deltaTime);
        
        // Simple state machine for animation switching
        if (speed > 0.1f) {
            // Walking/Running
            if (currentAnim.find("walk") == std::string::npos && 
                currentAnim.find("run") == std::string::npos) {
                model->playAnimation("Walk", true);
            }
        } else {
            // Idle
            if (currentAnim.find("idle") == std::string::npos) {
                model->playAnimation("Idle", true);
            }
        }
    }
}
