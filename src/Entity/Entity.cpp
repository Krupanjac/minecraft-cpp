#include "Entity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"

Entity::Entity(const glm::vec3& position)
    : position(position),
      rotation(0.0f),
      scale(1.0f),
      velocity(0.0f),
      prevPosition(position),
      prevRotation(0.0f),
      prevScale(1.0f) {
}

void Entity::update(float deltaTime) {
    // Capture previous transform for temporal effects (TAA velocity, etc.)
    prevPosition = position;
    prevRotation = rotation;
    prevScale = scale;

    // Basic physics integration could go here, or be handled by subclasses/physics engine
    position += velocity * deltaTime;
    
    // Update invulnerability timer
    if (invulnerabilityTimer > 0.0f) {
        invulnerabilityTimer -= deltaTime;
        if (invulnerabilityTimer < 0.0f) {
            invulnerabilityTimer = 0.0f;
        }
    }
    
    // Update damage flash timer
    if (damageFlashTimer > 0.0f) {
        damageFlashTimer -= deltaTime;
        if (damageFlashTimer < 0.0f) {
            damageFlashTimer = 0.0f;
        }
    }
    
    // Update model animations if applicable
    if (model) {
        model->updateAnimation(deltaTime);
    }
}

void Entity::takeDamage(float amount, const glm::vec3& knockbackDir) {
    if (isInvulnerable() || isDead()) {
        return;
    }
    
    health -= amount;
    invulnerabilityTimer = INVULNERABILITY_DURATION;
    damageFlashTimer = 0.3f; // Red flash duration
    
    LOG_INFO("Entity took " + std::to_string(amount) + " damage, health now: " + std::to_string(health));
    
    // Apply knockback
    if (glm::length(knockbackDir) > 0.001f) {
        glm::vec3 normalizedKnockback = glm::normalize(knockbackDir);
        velocity += normalizedKnockback * 8.0f; // Knockback strength
        velocity.y += 4.0f; // Add upward component
    }
    
    if (health <= 0.0f) {
        health = 0.0f;
        onDeath();
    }
}

void Entity::heal(float amount) {
    health = std::min(health + amount, maxHealth);
}

void Entity::onDeath() {
    // Base implementation - subclasses can override for death effects
    LOG_INFO("Entity died at position: " + std::to_string(position.x) + ", " + 
             std::to_string(position.y) + ", " + std::to_string(position.z));
}

void Entity::render(Shader& shader) {
    if (model) {
        glm::mat4 modelMatrix = getModelMatrix();
        shader.setMat4("uModel", modelMatrix);
        // Fallback: assume previous == current when called this way
        shader.setMat4("uPrevModel", modelMatrix);
        model->draw(shader, modelMatrix, modelMatrix);
    }
}

void Entity::renderWithMatrices(Shader& shader, const glm::mat4& currentModel, const glm::mat4& prevModel) {
    if (model) {
        shader.setMat4("uModel", currentModel);
        shader.setMat4("uPrevModel", prevModel);
        model->draw(shader, currentModel, prevModel);
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

glm::mat4 Entity::getPrevModelMatrix() const {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, prevPosition);
    m = glm::rotate(m, glm::radians(prevRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(prevRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(prevRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, prevScale);
    return m;
}
