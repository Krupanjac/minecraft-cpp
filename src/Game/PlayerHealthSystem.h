#pragma once

#include <functional>
#include <glm/glm.hpp>

class UIManager;
class Camera;

class PlayerHealthSystem {
public:
    PlayerHealthSystem(UIManager& uiManager, Camera& camera);

    void update(float deltaTime);

    float getHealth() const;
    float getMaxHealth() const;

    void setOnDeathCallback(const std::function<void()>& callback);
    void setHealthSilent(float newHealth);
    void resetToMax();

    void takeDamage(float amount,
                    const glm::vec3& knockbackDir = glm::vec3(0.0f),
                    bool playHurtSound = true);

private:
    void syncUI();

    UIManager& uiManager;
    Camera& camera;
    std::function<void()> onDeath;

    float health = 20.0f;
    float maxHealth = 20.0f;
    float invulnerabilityTimer = 0.0f;
};
