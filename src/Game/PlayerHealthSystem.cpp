#include "PlayerHealthSystem.h"
#include "../UI/UIManager.h"
#include "../Render/Camera.h"
#include "../Audio/AudioManager.h"

#include <algorithm>

PlayerHealthSystem::PlayerHealthSystem(UIManager& uiManagerRef, Camera& cameraRef)
    : uiManager(uiManagerRef),
      camera(cameraRef) {
    syncUI();
}

void PlayerHealthSystem::setOnDeathCallback(const std::function<void()>& callback) {
    onDeath = callback;
}

void PlayerHealthSystem::update(float deltaTime) {
    if (invulnerabilityTimer > 0.0f) {
        invulnerabilityTimer -= deltaTime;
        if (invulnerabilityTimer < 0.0f) {
            invulnerabilityTimer = 0.0f;
        }
    }
    syncUI();
}

float PlayerHealthSystem::getHealth() const {
    return health;
}

float PlayerHealthSystem::getMaxHealth() const {
    return maxHealth;
}

void PlayerHealthSystem::setHealthSilent(float newHealth) {
    health = std::max(0.0f, newHealth);
    syncUI();
}

void PlayerHealthSystem::resetToMax() {
    health = maxHealth;
    invulnerabilityTimer = 0.0f;
    syncUI();
}

void PlayerHealthSystem::takeDamage(float amount, const glm::vec3& knockbackDir, bool playHurtSound) {
    if (uiManager.isCreativeMode) {
        return;
    }
    if (invulnerabilityTimer > 0.0f || health <= 0.0f) {
        return;
    }

    health -= amount;
    invulnerabilityTimer = 0.5f; // Half second of immunity

    if (playHurtSound) {
        Audio::AudioManager::instance().playSound(Audio::SoundType::PLAYER_HURT, 0.8f);
    }

    if (glm::length(knockbackDir) > 0.001f) {
        glm::vec3 normalizedKnockback = glm::normalize(knockbackDir);
        camera.velocity += normalizedKnockback * 8.0f;
        camera.velocity.y += 4.0f;
    }

    if (health <= 0.0f) {
        health = 0.0f;
        if (onDeath) {
            onDeath();
        }
    }

    syncUI();
}

void PlayerHealthSystem::syncUI() {
    uiManager.playerHealth = static_cast<int>(health);
}
