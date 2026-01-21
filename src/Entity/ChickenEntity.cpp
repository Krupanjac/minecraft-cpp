#include "ChickenEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

// Original constructor - loads model internally (causes stutter)
ChickenEntity::ChickenEntity(const glm::vec3& startPos) : PassiveMob(startPos) {
    loadModel();
    initializeCommon();
    
    LOG_INFO("Chicken entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

// New constructor with pre-loaded model (no stutter)
ChickenEntity::ChickenEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id)
    : PassiveMob(startPos, cachedModel, id) {
    initializeCommon();
}

void ChickenEntity::initializeCommon() {
    // Quaternius model scale and rotation - chickens are smaller
    setScale(glm::vec3(0.35f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);
    
    moveSpeed = 1.2f;
    fleeSpeed = 3.0f;
    health = maxHealth = 4.0f;
    
    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
}

void ChickenEntity::loadModel() {
    std::string modelPath = "assets/models/Chicken/Chicken.gltf";
    auto chickenModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(chickenModel);
}
