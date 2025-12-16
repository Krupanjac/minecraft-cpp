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
}
