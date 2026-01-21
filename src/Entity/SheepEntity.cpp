#include "SheepEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

// Original constructor - loads model internally (causes stutter)
SheepEntity::SheepEntity(const glm::vec3& startPos) : PassiveMob(startPos) {
    loadModel();
    initializeCommon();
    
    LOG_INFO("Sheep entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

// New constructor with pre-loaded model (no stutter)
SheepEntity::SheepEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id)
    : PassiveMob(startPos, cachedModel, id) {
    initializeCommon();
}

void SheepEntity::initializeCommon() {
    // Quaternius model scale and rotation
    setScale(glm::vec3(0.5f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);
    
    moveSpeed = 1.3f;
    fleeSpeed = 3.2f;
    health = maxHealth = 8.0f;
    
    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
}

void SheepEntity::loadModel() {
    std::string modelPath = "assets/models/Sheep/Sheep.gltf";
    auto sheepModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(sheepModel);
}
