#include "PigEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

PigEntity::PigEntity(const glm::vec3& startPos) : PassiveMob(startPos) {
    loadModel();
    
    // Quaternius model scale and rotation
    setScale(glm::vec3(0.5f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);
    
    moveSpeed = 1.5f;
    fleeSpeed = 3.5f;
    health = maxHealth = 10.0f;
    
    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
    
    LOG_INFO("Pig entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

void PigEntity::loadModel() {
    std::string modelPath = "assets/models/Pig/Pig.gltf";
    auto pigModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(pigModel);
}
