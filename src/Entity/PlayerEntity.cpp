#include "PlayerEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../Render/Camera.h"
#include <algorithm>

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
    
    // Pick animations based on model
    pickAnimations();
}

void PlayerEntity::pickAnimations() {
    if (!model) return;
    
    auto anims = model->getAnimationNames();
    
    // Default animation names
    idleAnim = "Idle";
    walkAnim = "Walk";
    runAnim = "Run";
    jumpAnim = "Jump";
    jumpIdleAnim = "Jump_Idle";
    jumpLandAnim = "Jump_Land";
    
    // Look for best matches (case-insensitive)
    auto toLower = [](const std::string& s) {
        std::string result = s;
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    };
    
    for (const auto& name : anims) {
        std::string lower = toLower(name);
        if (lower == "idle") idleAnim = name;
        else if (lower == "walk") walkAnim = name;
        else if (lower == "run") runAnim = name;
        else if (lower == "jump") jumpAnim = name;
        else if (lower == "jump_idle" || lower == "jumpidle") jumpIdleAnim = name;
        else if (lower == "jump_land" || lower == "jumpland") jumpLandAnim = name;
    }
    
    LOG_INFO("Player animations: idle='" + idleAnim + "' walk='" + walkAnim + "' run='" + runAnim + 
             "' jump='" + jumpAnim + "' jumpIdle='" + jumpIdleAnim + "' jumpLand='" + jumpLandAnim + "'");
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
        
        // Simple state machine for animation switching (no camera info)
        if (speed > 0.1f) {
            if (currentAnim.find("walk") == std::string::npos && 
                currentAnim.find("run") == std::string::npos) {
                model->playAnimation(walkAnim, true);
            }
            float walkSpeedRef = 5.0f;
            float animSpeed = std::clamp(speed / walkSpeedRef, 0.75f, 1.4f);
            model->setAnimationSpeed(animSpeed);
        } else {
            if (currentAnim.find("idle") == std::string::npos) {
                model->playAnimation(idleAnim, true);
            }
            model->setAnimationSpeed(1.0f);
        }
    }
}

void PlayerEntity::updateWithCamera(float deltaTime, const Camera& camera) {
    // Check if model needs to be reloaded (settings changed)
    if (Settings::instance().playerModelIndex != currentModelIndex) {
        loadModelFromSettings();
    }
    
    Entity::update(deltaTime);
    
    if (!model) return;
    
    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    std::string currentAnim = toLowerPlayer(model->getCurrentAnimation());
    bool onGround = camera.onGround;
    
    // Update animation time
    model->updateAnimation(deltaTime);
    
    // Detect jump start (transition from ground to air with upward velocity)
    if (wasOnGround && !onGround && velocity.y > 0.1f) {
        isJumping = true;
        jumpAnimTimer = 0.0f;
        model->playAnimation(jumpAnim, false);
    }
    
    // Detect landing
    if (!wasOnGround && onGround) {
        if (isJumping) {
            // Play land animation briefly
            model->playAnimation(jumpLandAnim, false);
            jumpAnimTimer = 0.35f; // Brief landing animation
        }
        isJumping = false;
    }
    
    // Update jump animation timer
    if (jumpAnimTimer > 0.0f) {
        jumpAnimTimer -= deltaTime;
        if (jumpAnimTimer <= 0.0f) {
            // Return to normal animations
            jumpAnimTimer = 0.0f;
        }
    }
    
    // Animation state machine
    if (!onGround && isJumping) {
        // In air after jump - play jump idle/loop if not already playing jump
        if (velocity.y <= 0.0f && currentAnim.find("jump") != std::string::npos && 
            currentAnim.find("idle") == std::string::npos && currentAnim.find("land") == std::string::npos) {
            // Falling - transition to jump idle (airborne loop)
            model->playAnimation(jumpIdleAnim, true);
        }
    } else if (onGround && jumpAnimTimer <= 0.0f) {
        // On ground and not in landing animation
        if (speed > 4.0f && camera.isSprinting) {
            // Running
            if (currentAnim.find("run") == std::string::npos) {
                model->playAnimation(runAnim, true);
            }
            // Match animation speed to sprint velocity
            float runSpeedRef = 7.0f;
            float animSpeed = std::clamp(speed / runSpeedRef, 0.85f, 1.6f);
            model->setAnimationSpeed(animSpeed);
        } else if (speed > 0.1f) {
            // Walking
            if (currentAnim.find("walk") == std::string::npos && currentAnim.find("run") == std::string::npos) {
                model->playAnimation(walkAnim, true);
            }
            // Match animation speed to walk velocity
            float walkSpeedRef = 5.0f;
            float animSpeed = std::clamp(speed / walkSpeedRef, 0.75f, 1.4f);
            model->setAnimationSpeed(animSpeed);
        } else {
            // Idle
            if (currentAnim.find("idle") == std::string::npos || 
                currentAnim.find("jump") != std::string::npos) {
                model->playAnimation(idleAnim, true);
            }
            model->setAnimationSpeed(1.0f);
        }
    }
    
    wasOnGround = onGround;
}
