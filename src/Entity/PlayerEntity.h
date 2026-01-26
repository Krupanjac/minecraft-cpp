#pragma once
#include "Entity.h"

class Camera; // Forward declaration

class PlayerEntity : public Entity {
public:
    PlayerEntity(const glm::vec3& startPos);
    ~PlayerEntity() override = default;

    void update(float deltaTime) override;
    
    // Update with camera state for jump detection
    void updateWithCamera(float deltaTime, const Camera& camera);
    
    // Reload model from settings (called when player changes model in menu)
    void loadModelFromSettings();
    
    // Get the global transform of the right hand bone (for attaching held items)
    glm::mat4 getRightHandTransform() const;
    
    // Check if the model supports Hold animations
    bool supportsHoldAnimations() const { return hasHoldAnimations; }
    
    // Attack animation control
    void playAttackAnimation();
    bool isPlayingAttackAnimation() const { return isAttacking; }
    
    // Death animation control
    void playDeathAnimation();
    bool isPlayingDeathAnimation() const { return isDead; }
    void resetDeathState();
    
    // Hit react animation control (when taking damage)
    void playHitReactAnimation();
    bool isPlayingHitReact() const { return isHitReacting; }
    
    // Duck animation control (crouching)
    void playDuckAnimation(bool loop = true);
    void stopDuckAnimation();
    bool isDucking() const { return isDuckingState; }
    
    // Emote animations (Wave, Yes, No)
    void playWaveAnimation();
    void playYesAnimation();
    void playNoAnimation();
    bool isPlayingEmote() const { return isEmoting; }

private:
    int currentModelIndex = -1;
    glm::vec3 rotationOffset = glm::vec3(0.0f);
    
    // Animation state tracking
    bool wasOnGround = true;
    bool isJumping = false;
    float jumpAnimTimer = 0.0f;
    
    // Base animations (without item)
    std::string idleAnim = "Idle";
    std::string walkAnim = "Walk";
    std::string runAnim = "Run";
    std::string jumpAnim = "Jump";
    std::string jumpIdleAnim = "Jump_Idle";
    std::string jumpLandAnim = "Jump_Land";
    
    // Hold animations (with item held)
    std::string idleHoldAnim = "Idle_Hold";
    std::string walkHoldAnim = "Walk_Hold";
    std::string runHoldAnim = "Run_Hold";
    
    // Attack animations
    std::string idleAttackAnim = "Idle_Attack";
    std::string runAttackAnim = "Run_Attack";
    std::string punchAnim = "Punch";
    
    // Other animations
    std::string deathAnim = "Death";
    std::string duckAnim = "Duck";
    std::string hitReactAnim = "HitReact";
    std::string noAnim = "No";
    std::string waveAnim = "Wave";
    std::string yesAnim = "Yes";
    
    // Right hand bone name for this model
    std::string rightHandBone = "Fist.R";
    
    // Feature flags
    bool hasHoldAnimations = false;
    
    // Attack state
    bool isAttacking = false;
    float attackAnimTimer = 0.0f;
    
    // Death state
    bool isDead = false;
    
    // Hit react state
    bool isHitReacting = false;
    float hitReactTimer = 0.0f;
    
    // Duck state
    bool isDuckingState = false;
    
    // Emote state
    bool isEmoting = false;
    float emoteTimer = 0.0f;
    
    void pickAnimations();
};
