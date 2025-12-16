#include "Entity.h"
#include "../Model/Model.h"

Entity::Entity(const glm::vec3& position)
    : position(position),
      rotation(0.0f),
      scale(1.0f),
      velocity(0.0f) {
}

void Entity::update(float deltaTime) {
    // Basic physics integration could go here, or be handled by subclasses/physics engine
    position += velocity * deltaTime;
    
    // Update model animations if applicable
    if (model) {
        model->updateAnimation(deltaTime);
    }
}

void Entity::render(Shader& shader) {
    if (model) {
        glm::mat4 modelMatrix = getModelMatrix();
        shader.setMat4("uModel", modelMatrix);
        model->draw(shader, modelMatrix);
    }
}

glm::mat4 Entity::getModelMatrix() const {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, position);
    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, scale);
    return m;
}
