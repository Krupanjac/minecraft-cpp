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
    // Render with externally computed transforms (used for camera-relative rendering + correct motion vectors)
    void renderWithMatrices(Shader& shader, const glm::mat4& currentModel, const glm::mat4& prevModel);

    void setPosition(const glm::vec3& pos) { position = pos; }
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getPrevPosition() const { return prevPosition; }

    void setRotation(const glm::vec3& rot) { rotation = rot; }
    const glm::vec3& getRotation() const { return rotation; }
    const glm::vec3& getPrevRotation() const { return prevRotation; }
    
    void setScale(const glm::vec3& s) { scale = s; }
    const glm::vec3& getScale() const { return scale; }
    const glm::vec3& getPrevScale() const { return prevScale; }

    void setVelocity(const glm::vec3& vel) { velocity = vel; }
    const glm::vec3& getVelocity() const { return velocity; }

    void setModel(std::shared_ptr<ModelSystem::Model> newModel) { this->model = newModel; }

protected:
    glm::vec3 position;
    glm::vec3 rotation; // Euler angles in degrees
    glm::vec3 scale;
    glm::vec3 velocity;

    // Previous-frame transform for motion vectors / temporal stability
    glm::vec3 prevPosition;
    glm::vec3 prevRotation;
    glm::vec3 prevScale;

    std::shared_ptr<ModelSystem::Model> model;
    
    // Helper to build model matrix
    glm::mat4 getModelMatrix() const;
    glm::mat4 getPrevModelMatrix() const;
};
