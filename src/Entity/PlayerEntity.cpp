#include "PlayerEntity.h"
#include "../Model/Model.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../Render/Camera.h"
#include <algorithm>

PlayerEntity::PlayerEntity(const glm::vec3& startPos) : Entity(startPos) {
    loadModelFromSettings();
}

void PlayerEntity::loadModelFromSettings() {
    auto& settings = Settings::instance();
    int modelIndex = settings.playerModelIndex;
    
    // Validate index
    if (modelIndex < 0 || modelIndex >= Settings::NUM_PLAYER_MODELS) {
        modelIndex = 0;
    }
    
    std::string modelPath = Settings::PLAYER_MODEL_PATHS[modelIndex];
    LOG_INFO("Loading player model: " + modelPath);
    
    auto playerModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(playerModel);
    
    // Different scales for different models
    if (modelIndex == 0) {
        // Half-Life model - needs smaller scale
        setScale(glm::vec3(0.03f));
        rotationOffset = glm::vec3(0.0f, 0.0f, 0.0f);
    } else {
        // Quaternius characters - larger scale
        setScale(glm::vec3(0.5f));
        rotationOffset = glm::vec3(0.0f, 180.0f, 0.0f);
    }
    setRotation(rotationOffset);
    
    currentModelIndex = modelIndex;
    
    // Pick animations based on model
    pickAnimations();
}

void PlayerEntity::pickAnimations() {
    if (!model) return;
    
    auto anims = model->getAnimationNames();
    
    // Default animation names (base)
    idleAnim = "Idle";
    walkAnim = "Walk";
    runAnim = "Run";
    jumpAnim = "Jump";
    jumpIdleAnim = "Jump_Idle";
    jumpLandAnim = "Jump_Land";
    
    // Hold animations (with item)
    idleHoldAnim = "Idle_Hold";
    walkHoldAnim = "Walk_Hold";
    runHoldAnim = "Run_Hold";
    
    // Attack animations
    idleAttackAnim = "Idle_Attack";
    runAttackAnim = "Run_Attack";
    punchAnim = "Punch";
    
    // Other animations
    deathAnim = "Death";
    duckAnim = "Duck";
    hitReactAnim = "HitReact";
    noAnim = "No";
    waveAnim = "Wave";
    yesAnim = "Yes";
    
    // Default hand bone for Quaternius models
    rightHandBone = "Fist.R";
    hasHoldAnimations = false;
    
    // Look for best matches (case-insensitive)
    auto toLower = [](const std::string& s) {
        std::string result = s;
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    };
    
    for (const auto& name : anims) {
        std::string lower = toLower(name);
        
        // Base animations
        if (lower == "idle") idleAnim = name;
        else if (lower == "walk") walkAnim = name;
        else if (lower == "run") runAnim = name;
        else if (lower == "jump") jumpAnim = name;
        else if (lower == "jump_idle" || lower == "jumpidle") jumpIdleAnim = name;
        else if (lower == "jump_land" || lower == "jumpland") jumpLandAnim = name;
        
        // Hold animations
        else if (lower == "idle_hold" || lower == "idlehold") { 
            idleHoldAnim = name; 
            hasHoldAnimations = true;
        }
        else if (lower == "walk_hold" || lower == "walkhold") walkHoldAnim = name;
        else if (lower == "run_hold" || lower == "runhold") runHoldAnim = name;
        
        // Attack animations
        else if (lower == "idle_attack" || lower == "idleattack") idleAttackAnim = name;
        else if (lower == "run_attack" || lower == "runattack") runAttackAnim = name;
        else if (lower == "punch") punchAnim = name;
        
        // Other animations
        else if (lower == "death") deathAnim = name;
        else if (lower == "duck") duckAnim = name;
        else if (lower == "hitreact" || lower == "hit_react") hitReactAnim = name;
        else if (lower == "no") noAnim = name;
        else if (lower == "wave") waveAnim = name;
        else if (lower == "yes") yesAnim = name;
    }
    
    // Check if model has the right hand bone
    if (model->hasNode("Fist.R")) {
        rightHandBone = "Fist.R";
    } else if (model->hasNode("Hand.R")) {
        rightHandBone = "Hand.R";
    } else if (model->hasNode("RightHand")) {
        rightHandBone = "RightHand";
    }
    
    LOG_INFO("Player animations found:");
    LOG_INFO("  Base: idle='" + idleAnim + "' walk='" + walkAnim + "' run='" + runAnim + "'");
    LOG_INFO("  Jump: jump='" + jumpAnim + "' jumpIdle='" + jumpIdleAnim + "' jumpLand='" + jumpLandAnim + "'");
    if (hasHoldAnimations) {
        LOG_INFO("  Hold: idleHold='" + idleHoldAnim + "' walkHold='" + walkHoldAnim + "' runHold='" + runHoldAnim + "'");
        LOG_INFO("  Attack: idleAttack='" + idleAttackAnim + "' runAttack='" + runAttackAnim + "' punch='" + punchAnim + "'");
    }
    LOG_INFO("  Right hand bone: '" + rightHandBone + "'");
}

static std::string toLowerPlayer(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

void PlayerEntity::update(float deltaTime) {
    // Check if model needs to be reloaded (settings changed)
    if (Settings::instance().playerModelIndex != currentModelIndex) {
        loadModelFromSettings();
    }
    
    Entity::update(deltaTime);
    
    if (model) {
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        std::string currentAnim = toLowerPlayer(model->getCurrentAnimation());
        
        // Check if holding an item (for Hold animations)
        bool isHoldingItem = (heldItem != ItemType::NONE) && hasHoldAnimations;
        
        // Update animation
        model->updateAnimation(deltaTime);
        
        // Simple state machine for animation switching (no camera info)
        if (speed > 0.1f) {
            std::string targetAnim = isHoldingItem ? walkHoldAnim : walkAnim;
            bool needsChange = (currentAnim.find("walk") == std::string::npos && 
                               currentAnim.find("run") == std::string::npos);
            if (!needsChange && isHoldingItem && currentAnim.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnim.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
            float walkSpeedRef = 5.0f;
            float animSpeed = std::clamp(speed / walkSpeedRef, 0.75f, 1.4f);
            model->setAnimationSpeed(animSpeed);
        } else {
            std::string targetAnim = isHoldingItem ? idleHoldAnim : idleAnim;
            bool needsChange = (currentAnim.find("idle") == std::string::npos);
            if (!needsChange && isHoldingItem && currentAnim.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnim.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
            model->setAnimationSpeed(1.0f);
        }
    }
}

void PlayerEntity::updateWithCamera(float deltaTime, const Camera& camera) {
    // Check if model needs to be reloaded (settings changed)
    if (Settings::instance().playerModelIndex != currentModelIndex) {
        loadModelFromSettings();
    }
    
    Entity::update(deltaTime);
    
    if (!model) return;
    
    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    std::string currentAnim = toLowerPlayer(model->getCurrentAnimation());
    bool onGround = camera.onGround;
    
    // Check if holding an item (for Hold animations)
    bool isHoldingItem = (heldItem != ItemType::NONE) && hasHoldAnimations;
    
    // Update animation time
    model->updateAnimation(deltaTime);
    
    // If dead, only play death animation
    if (isDead) {
        return;
    }
    
    // Update hit react timer
    if (isHitReacting && hitReactTimer > 0.0f) {
        hitReactTimer -= deltaTime;
        if (hitReactTimer <= 0.0f) {
            isHitReacting = false;
            hitReactTimer = 0.0f;
        } else {
            return;  // Don't interrupt hit react animation
        }
    }
    
    // Update emote timer
    if (isEmoting && emoteTimer > 0.0f) {
        emoteTimer -= deltaTime;
        if (emoteTimer <= 0.0f) {
            isEmoting = false;
            emoteTimer = 0.0f;
        } else {
            return;  // Don't interrupt emote animation
        }
    }
    
    // Update attack animation timer
    if (isAttacking && attackAnimTimer > 0.0f) {
        attackAnimTimer -= deltaTime;
        if (attackAnimTimer <= 0.0f) {
            isAttacking = false;
            attackAnimTimer = 0.0f;
        } else {
            // Still in attack animation, don't change
            return;
        }
    }
    
    // Detect jump start (transition from ground to air with upward velocity)
    if (wasOnGround && !onGround && velocity.y > 0.1f) {
        isJumping = true;
        jumpAnimTimer = 0.0f;
        model->playAnimation(jumpAnim, false);
    }
    
    // Detect landing
    if (!wasOnGround && onGround) {
        if (isJumping) {
            // Play land animation briefly
            model->playAnimation(jumpLandAnim, false);
            jumpAnimTimer = 0.35f; // Brief landing animation
        }
        isJumping = false;
    }
    
    // Update jump animation timer
    if (jumpAnimTimer > 0.0f) {
        jumpAnimTimer -= deltaTime;
        if (jumpAnimTimer <= 0.0f) {
            // Return to normal animations
            jumpAnimTimer = 0.0f;
        }
    }
    
    // Animation state machine
    if (!onGround && isJumping) {
        // In air after jump - play jump idle/loop if not already playing jump
        if (velocity.y <= 0.0f && currentAnim.find("jump") != std::string::npos && 
            currentAnim.find("idle") == std::string::npos && currentAnim.find("land") == std::string::npos) {
            // Falling - transition to jump idle (airborne loop)
            model->playAnimation(jumpIdleAnim, true);
        }
    } else if (onGround && jumpAnimTimer <= 0.0f) {
        // On ground and not in landing animation
        if (speed > 4.0f && camera.isSprinting) {
            // Running (with or without item)
            std::string targetAnim = isHoldingItem ? runHoldAnim : runAnim;
            if (currentAnim.find("run") == std::string::npos || 
                (isHoldingItem && currentAnim.find("hold") == std::string::npos) ||
                (!isHoldingItem && currentAnim.find("hold") != std::string::npos)) {
                model->playAnimation(targetAnim, true);
            }
            // Match animation speed to sprint velocity
            float runSpeedRef = 7.0f;
            float animSpeed = std::clamp(speed / runSpeedRef, 0.85f, 1.6f);
            model->setAnimationSpeed(animSpeed);
        } else if (speed > 0.1f) {
            // Walking (with or without item)
            std::string targetAnim = isHoldingItem ? walkHoldAnim : walkAnim;
            bool needsChange = (currentAnim.find("walk") == std::string::npos && currentAnim.find("run") == std::string::npos);
            // Also switch if hold state changed
            if (!needsChange && isHoldingItem && currentAnim.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnim.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
            // Match animation speed to walk velocity
            float walkSpeedRef = 5.0f;
            float animSpeed = std::clamp(speed / walkSpeedRef, 0.75f, 1.4f);
            model->setAnimationSpeed(animSpeed);
        } else {
            // Idle (with or without item)
            std::string targetAnim = isHoldingItem ? idleHoldAnim : idleAnim;
            bool needsChange = (currentAnim.find("idle") == std::string::npos || 
                                currentAnim.find("jump") != std::string::npos);
            // Also switch if hold state changed
            if (!needsChange && isHoldingItem && currentAnim.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnim.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
            model->setAnimationSpeed(1.0f);
        }
    }
    
    wasOnGround = onGround;
}

glm::mat4 PlayerEntity::getRightHandTransform() const {
    if (!model) {
        return glm::mat4(1.0f);
    }
    
    // Get the bone transform from the model
    glm::mat4 boneTransform = model->getNodeGlobalTransform(rightHandBone);
    
    // Build the entity's model matrix
    glm::mat4 entityMatrix = getModelMatrix();
    
    // Combine: entity transform * bone transform
    return entityMatrix * boneTransform;
}

void PlayerEntity::playAttackAnimation() {
    if (!model || isDead) return;
    
    isAttacking = true;
    attackAnimTimer = 0.4f; // Attack animation duration
    
    // Choose attack animation based on current state
    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    bool isHoldingItem = (heldItem != ItemType::NONE);
    
    std::string attackAnim;
    if (isHoldingItem) {
        // Use attack animations for held items
        if (speed > 4.0f) {
            attackAnim = runAttackAnim;
        } else {
            attackAnim = idleAttackAnim;
        }
    } else {
        // Punch animation when no item held
        attackAnim = punchAnim;
    }
    
    model->playAnimation(attackAnim, false);
}

void PlayerEntity::playDeathAnimation() {
    if (!model) return;
    
    isDead = true;
    isAttacking = false;
    attackAnimTimer = 0.0f;
    isHitReacting = false;
    hitReactTimer = 0.0f;
    isEmoting = false;
    emoteTimer = 0.0f;
    
    model->playAnimation(deathAnim, false);
}

void PlayerEntity::playHitReactAnimation() {
    if (!model || isDead || isAttacking) return;
    
    isHitReacting = true;
    hitReactTimer = 0.3f;
    
    model->playAnimation(hitReactAnim, false);
}

void PlayerEntity::playDuckAnimation(bool loop) {
    if (!model || isDead) return;
    
    isDuckingState = true;
    isEmoting = false;
    emoteTimer = 0.0f;
    
    model->playAnimation(duckAnim, loop);
}

void PlayerEntity::stopDuckAnimation() {
    isDuckingState = false;
    // Animation will transition back to idle/walk in update
}

void PlayerEntity::playWaveAnimation() {
    if (!model || isDead) return;
    
    isEmoting = true;
    emoteTimer = 2.0f;  // Wave animation duration
    isAttacking = false;
    attackAnimTimer = 0.0f;
    
    model->playAnimation(waveAnim, false);
}

void PlayerEntity::playYesAnimation() {
    if (!model || isDead) return;
    
    isEmoting = true;
    emoteTimer = 1.5f;  // Yes animation duration
    isAttacking = false;
    attackAnimTimer = 0.0f;
    
    model->playAnimation(yesAnim, false);
}

void PlayerEntity::playNoAnimation() {
    if (!model || isDead) return;
    
    isEmoting = true;
    emoteTimer = 1.5f;  // No animation duration
    isAttacking = false;
    attackAnimTimer = 0.0f;
    
    model->playAnimation(noAnim, false);
}
