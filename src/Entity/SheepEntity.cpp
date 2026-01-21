#include "SheepEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

SheepEntity::SheepEntity(const glm::vec3& startPos) : PassiveMob(startPos) {
    loadModel();
    
    // Quaternius model scale and rotation
    setScale(glm::vec3(0.5f));
    rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);
    
    moveSpeed = 1.3f;
    fleeSpeed = 3.2f;
    health = maxHealth = 8.0f;
    
    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
    
    LOG_INFO("Sheep entity created at (" + std::to_string(startPos.x) + ", " + 
             std::to_string(startPos.y) + ", " + std::to_string(startPos.z) + ")");
}

void SheepEntity::loadModel() {
    std::string modelPath = "assets/models/Sheep/Sheep.gltf";
    auto sheepModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(sheepModel);
}
