#include "PigEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

// Original constructor - loads model internally (causes stutter)
PigEntity::PigEntity(const glm::vec3& startPos) : PassiveMob(startPos) {
    loadModel();
    initializeCommon();
    
    LOG_INFO("Pig entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

// New constructor with pre-loaded model (no stutter)
PigEntity::PigEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id)
    : PassiveMob(startPos, cachedModel, id) {
    initializeCommon();
}

void PigEntity::initializeCommon() {
    // Quaternius model scale and rotation
    setScale(glm::vec3(0.5f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);
    
    moveSpeed = 1.5f;
    fleeSpeed = 3.5f;
    health = maxHealth = 10.0f;
    
    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
}

void PigEntity::loadModel() {
    std::string modelPath = "assets/models/Pig/Pig.gltf";
    auto pigModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(pigModel);
}
