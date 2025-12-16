#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include "../Render/Shader.h"

namespace ModelSystem {
    class Model;
}

class Entity {
public:
    Entity(const glm::vec3& position = glm::vec3(0.0f));
    virtual ~Entity() = default;

    virtual void update(float deltaTime);
    virtual void render(Shader& shader);

    void setPosition(const glm::vec3& pos) { position = pos; }
    const glm::vec3& getPosition() const { return position; }

    void setRotation(const glm::vec3& rot) { rotation = rot; }
    const glm::vec3& getRotation() const { return rotation; }
    
    void setScale(const glm::vec3& s) { scale = s; }
    const glm::vec3& getScale() const { return scale; }

    void setVelocity(const glm::vec3& vel) { velocity = vel; }
    const glm::vec3& getVelocity() const { return velocity; }

    void setModel(std::shared_ptr<ModelSystem::Model> newModel) { this->model = newModel; }

protected:
    glm::vec3 position;
    glm::vec3 rotation; // Euler angles in degrees
    glm::vec3 scale;
    glm::vec3 velocity;

    std::shared_ptr<ModelSystem::Model> model;
    
    // Helper to build model matrix
    glm::mat4 getModelMatrix() const;
};
