#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include "../Render/Shader.h"
#include "../World/Item.h"

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

    // Health system
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    void setHealth(float h) { health = std::max(0.0f, std::min(h, maxHealth)); }
    void setMaxHealth(float mh) { maxHealth = mh; if (health > maxHealth) health = maxHealth; }
    bool isDead() const { return health <= 0.0f; }
    
    // Damage handling
    virtual void takeDamage(float amount, const glm::vec3& knockbackDir = glm::vec3(0.0f));
    virtual void heal(float amount);
    virtual void onDeath();
    
    // Combat - damage immunity after being hit
    bool isInvulnerable() const { return invulnerabilityTimer > 0.0f; }
    float getInvulnerabilityTimer() const { return invulnerabilityTimer; }
    
    // Held item (for players and humanoid mobs)
    ItemType getHeldItem() const { return heldItem; }
    void setHeldItem(ItemType item) { heldItem = item; }
    
    // Entity ID for networking
    uint32_t getEntityId() const { return entityId; }
    void setEntityId(uint32_t id) { entityId = id; }

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
    
    // Health system
    float health = 20.0f;
    float maxHealth = 20.0f;
    float invulnerabilityTimer = 0.0f;
    static constexpr float INVULNERABILITY_DURATION = 0.5f; // Half second of immunity after damage
    
    // Damage flash effect
    float damageFlashTimer = 0.0f;
    
    // Held item
    ItemType heldItem = ItemType::NONE;
    
    // Network ID
    uint32_t entityId = 0;
    
    // Helper to build model matrix
    glm::mat4 getModelMatrix() const;
    glm::mat4 getPrevModelMatrix() const;
};
