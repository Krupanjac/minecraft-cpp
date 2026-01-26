#include "NetworkManager.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../Model/Model.h"
#include "../World/Item.h"
#include <algorithm>

namespace Network {

// ============== RemotePlayerEntity ==============

RemotePlayerEntity::RemotePlayerEntity(uint32_t playerId, const std::string& name, const glm::vec3& pos, uint8_t modelIndex)
    : Entity(pos)
    , m_playerId(playerId)
    , m_playerName(name)
    , m_modelIndex(modelIndex)
    , m_targetPosition(pos) {
    
    // Scale for player model - must match PlayerEntity scales
    // Half-Life model (index 0) needs 0.03f, other Quaternius models need 0.5f
    if (modelIndex == 0) {
        scale = glm::vec3(0.03f);
    } else {
        scale = glm::vec3(0.5f);
    }
}

void RemotePlayerEntity::initializeAnimations() {
    if (m_animationsInitialized || !model) return;
    
    auto anims = model->getAnimationNames();
    
    // Default animation names
    m_idleAnim = "Idle";
    m_walkAnim = "Walk";
    m_runAnim = "Run";
    m_idleHoldAnim = "Idle_Hold";
    m_walkHoldAnim = "Walk_Hold";
    m_runHoldAnim = "Run_Hold";
    m_idleAttackAnim = "Idle_Attack";
    m_runAttackAnim = "Run_Attack";
    m_punchAnim = "Punch";
    m_deathAnim = "Death";
    m_waveAnim = "Wave";
    m_yesAnim = "Yes";
    m_noAnim = "No";
    m_rightHandBone = "Fist.R";
    m_hasHoldAnimations = false;
    
    auto toLower = [](const std::string& s) {
        std::string result = s;
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    };
    
    for (const auto& name : anims) {
        std::string lower = toLower(name);
        
        if (lower == "idle") m_idleAnim = name;
        else if (lower == "walk") m_walkAnim = name;
        else if (lower == "run") m_runAnim = name;
        else if (lower == "idle_hold" || lower == "idlehold") { 
            m_idleHoldAnim = name; 
            m_hasHoldAnimations = true;
        }
        else if (lower == "walk_hold" || lower == "walkhold") m_walkHoldAnim = name;
        else if (lower == "run_hold" || lower == "runhold") m_runHoldAnim = name;
        else if (lower == "idle_attack" || lower == "idleattack") m_idleAttackAnim = name;
        else if (lower == "run_attack" || lower == "runattack") m_runAttackAnim = name;
        else if (lower == "punch") m_punchAnim = name;
        else if (lower == "death") m_deathAnim = name;
        else if (lower == "wave") m_waveAnim = name;
        else if (lower == "yes") m_yesAnim = name;
        else if (lower == "no") m_noAnim = name;
    }
    
    // Check if model has the right hand bone
    if (model->hasNode("Fist.R")) {
        m_rightHandBone = "Fist.R";
    } else if (model->hasNode("Hand.R")) {
        m_rightHandBone = "Hand.R";
    } else if (model->hasNode("RightHand")) {
        m_rightHandBone = "RightHand";
    }
    
    m_animationsInitialized = true;
    LOG_INFO("RemotePlayer " + m_playerName + " animations initialized: hasHold=" + std::to_string(m_hasHoldAnimations));
}

void RemotePlayerEntity::setHeldItem(uint8_t item) {
    m_heldItem = item;
    // Also update the Entity's heldItem for animation logic
    heldItem = static_cast<ItemType>(item);
}

void RemotePlayerEntity::update(float deltaTime) {
    // Store previous state for motion vectors
    prevPosition = position;
    prevRotation = rotation;
    prevScale = scale;
    
    // Interpolate position smoothly
    float t = std::min(1.0f, INTERPOLATION_SPEED * deltaTime);
    position = glm::mix(position, m_targetPosition, t);
    
    // Interpolate rotation (convert from camera yaw to model rotation, matching PlayerEntity)
    // PlayerEntity uses: rotation = glm::vec3(0.0f, -camera.getYaw() + 90.0f, 0.0f)
    // m_targetYaw is the raw camera yaw, so we need to convert it the same way
    float targetModelYaw = -m_targetYaw + 90.0f;
    float currentYaw = rotation.y;
    
    // Handle yaw wraparound
    float yawDiff = targetModelYaw - currentYaw;
    if (yawDiff > 180.0f) yawDiff -= 360.0f;
    if (yawDiff < -180.0f) yawDiff += 360.0f;
    
    rotation.y = currentYaw + yawDiff * t;
    // Note: pitch is not typically applied to player body rotation (only head would rotate)
    rotation.x = 0.0f;
    
    // Update animations based on velocity (like PlayerEntity)
    if (model) {
        // Initialize animations if not done yet
        if (!m_animationsInitialized) {
            initializeAnimations();
        }
        
        // Update model animation
        model->updateAnimation(deltaTime);
        
        // If dead, only play death animation
        if (m_isDead) {
            return;
        }
        
        // Update hit reaction timer
        if (m_isHitReacting && m_hitReactTimer > 0.0f) {
            m_hitReactTimer -= deltaTime;
            if (m_hitReactTimer <= 0.0f) {
                m_isHitReacting = false;
                m_hitReactTimer = 0.0f;
            } else {
                // Still in hit reaction, don't change animation
                return;
            }
        }
        
        // Update emote timer
        if (m_isEmoting && m_emoteTimer > 0.0f) {
            m_emoteTimer -= deltaTime;
            if (m_emoteTimer <= 0.0f) {
                m_isEmoting = false;
                m_emoteTimer = 0.0f;
            } else {
                // Still in emote animation, don't change
                return;
            }
        }
        
        // Update attack animation timer
        if (m_isAttacking && m_attackAnimTimer > 0.0f) {
            m_attackAnimTimer -= deltaTime;
            if (m_attackAnimTimer <= 0.0f) {
                m_isAttacking = false;
                m_attackAnimTimer = 0.0f;
            } else {
                // Still in attack animation, don't change
                return;
            }
        }
        
        // velocity is set via setTargetVelocity which stores in the inherited 'velocity' member
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        std::string currentAnim = model->getCurrentAnimation();
        
        // Convert to lowercase for comparison
        auto toLower = [](const std::string& s) {
            std::string result = s;
            for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        };
        std::string currentAnimLower = toLower(currentAnim);
        
        // Check if holding an item
        bool isHoldingItem = (m_heldItem != 0) && m_hasHoldAnimations;
        
        if (speed > 4.0f) {
            // Running
            std::string targetAnim = isHoldingItem ? m_runHoldAnim : m_runAnim;
            bool needsChange = (currentAnimLower.find("run") == std::string::npos);
            if (!needsChange && isHoldingItem && currentAnimLower.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnimLower.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
        } else if (speed > 0.1f) {
            // Walking
            std::string targetAnim = isHoldingItem ? m_walkHoldAnim : m_walkAnim;
            bool needsChange = (currentAnimLower.find("walk") == std::string::npos && 
                               currentAnimLower.find("run") == std::string::npos);
            if (!needsChange && isHoldingItem && currentAnimLower.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnimLower.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
        } else {
            // Idle
            std::string targetAnim = isHoldingItem ? m_idleHoldAnim : m_idleAnim;
            bool needsChange = (currentAnimLower.find("idle") == std::string::npos);
            if (!needsChange && isHoldingItem && currentAnimLower.find("hold") == std::string::npos) needsChange = true;
            if (!needsChange && !isHoldingItem && currentAnimLower.find("hold") != std::string::npos) needsChange = true;
            
            if (needsChange) {
                model->playAnimation(targetAnim, true);
            }
        }
    }
}

glm::mat4 RemotePlayerEntity::getRightHandTransform() const {
    if (!model) {
        return glm::mat4(1.0f);
    }
    
    // Get the bone transform from the model
    glm::mat4 boneTransform = model->getNodeGlobalTransform(m_rightHandBone);
    
    // Build the entity's model matrix
    glm::mat4 entityMatrix = getModelMatrix();
    
    // Combine: entity transform * bone transform
    return entityMatrix * boneTransform;
}

void RemotePlayerEntity::playAttackAnimation() {
    if (!model || m_isDead) return;
    
    m_isAttacking = true;
    m_attackAnimTimer = 0.4f; // Attack animation duration
    
    // Choose attack animation based on current state
    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    bool isHoldingItem = (m_heldItem != 0);
    
    std::string attackAnim;
    if (isHoldingItem) {
        // Use attack animations for held items
        if (speed > 4.0f) {
            attackAnim = m_runAttackAnim;
        } else {
            attackAnim = m_idleAttackAnim;
        }
    } else {
        // Punch animation when no item held
        attackAnim = m_punchAnim;
    }
    
    model->playAnimation(attackAnim, false);
}

void RemotePlayerEntity::playHitReceiveAnimation() {
    if (!model || m_isDead) return;
    
    m_isHitReacting = true;
    m_hitReactTimer = 0.5f; // Hit reaction duration
    m_isAttacking = false;  // Interrupt attack if hit
    m_attackAnimTimer = 0.0f;
    
    model->playAnimation(m_hitReceiveAnim, false);
}

void RemotePlayerEntity::playDeathAnimation() {
    if (!model) return;
    
    m_isDead = true;
    m_isAttacking = false;
    m_attackAnimTimer = 0.0f;
    m_isHitReacting = false;
    m_hitReactTimer = 0.0f;
    m_isEmoting = false;
    m_emoteTimer = 0.0f;
    
    model->playAnimation(m_deathAnim, false);
}

void RemotePlayerEntity::playWaveAnimation() {
    if (!model || m_isDead) return;
    
    m_isEmoting = true;
    m_emoteTimer = 2.0f;  // Wave animation duration
    m_isAttacking = false;
    m_attackAnimTimer = 0.0f;
    
    model->playAnimation(m_waveAnim, false);
}

void RemotePlayerEntity::playYesAnimation() {
    if (!model || m_isDead) return;
    
    m_isEmoting = true;
    m_emoteTimer = 1.5f;  // Yes animation duration
    m_isAttacking = false;
    m_attackAnimTimer = 0.0f;
    
    model->playAnimation(m_yesAnim, false);
}

void RemotePlayerEntity::playNoAnimation() {
    if (!model || m_isDead) return;
    
    m_isEmoting = true;
    m_emoteTimer = 1.5f;  // No animation duration
    m_isAttacking = false;
    m_attackAnimTimer = 0.0f;
    
    model->playAnimation(m_noAnim, false);
}

void RemotePlayerEntity::applyNetworkDamage(float damage, const glm::vec3& knockback) {
    if (m_isDead) return;
    
    m_networkHealth -= damage;
    
    // Apply knockback to velocity for visual effect
    if (glm::length(knockback) > 0.001f) {
        velocity += knockback;
    }
    
    if (m_networkHealth <= 0.0f) {
        m_networkHealth = 0.0f;
        playDeathAnimation();
    } else {
        playHitReceiveAnimation();
    }
}
// ============== NetworkManager ==============

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    disconnect();
}

bool NetworkManager::hostGame(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& playerName, uint8_t modelIndex) {
    if (m_mode != NetworkMode::OFFLINE) {
        LOG_WARNING("Already in a game");
        return false;
    }
    
    m_server = std::make_unique<GameServer>();
    
    if (!m_server->start(port, worldSeed, spawnPos, playerName, modelIndex)) {
        LOG_ERROR("Failed to start server");
        m_server.reset();
        return false;
    }
    
    setupServerCallbacks();
    
    m_mode = NetworkMode::HOST;
    m_localPlayerName = playerName;
    m_worldSeed = worldSeed;
    m_spawnPos = spawnPos;
    
    LOG_INFO("Hosting game on port " + std::to_string(port));
    
    if (m_onConnected) {
        m_onConnected();
    }
    
    return true;
}

bool NetworkManager::joinGame(const std::string& host, uint16_t port, const std::string& playerName, uint8_t modelIndex) {
    if (m_mode != NetworkMode::OFFLINE) {
        LOG_WARNING("Already in a game");
        return false;
    }
    
    m_client = std::make_unique<GameClient>();
    
    if (!m_client->connect(host, port, playerName, modelIndex)) {
        LOG_ERROR("Failed to connect to server");
        m_client.reset();
        return false;
    }
    
    setupClientCallbacks();
    
    m_mode = NetworkMode::CLIENT;
    m_localPlayerName = playerName;
    
    LOG_INFO("Connecting to " + host + ":" + std::to_string(port));
    return true;
}

void NetworkManager::disconnect() {
    if (m_mode == NetworkMode::OFFLINE) return;
    
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    
    if (m_client) {
        m_client->disconnect();
        m_client.reset();
    }
    
    m_remotePlayerEntities.clear();
    m_mode = NetworkMode::OFFLINE;
    
    LOG_INFO("Disconnected from network");
}

void NetworkManager::update(float deltaTime) {
    if (m_mode == NetworkMode::OFFLINE) return;
    
    if (m_server) {
        m_server->update();
    }
    
    if (m_client) {
        m_client->update();
        
        // Check if connection was lost
        if (m_client->getState() == ConnectionState::DISCONNECTED ||
            m_client->getState() == ConnectionState::FAILED) {
            disconnect();
            return;
        }
    }
    
    // Update remote player entities
    updateRemotePlayerEntities();
    
    // Update entity interpolation
    for (auto& entity : m_remotePlayerEntities) {
        entity->update(deltaTime);
    }
    
    m_positionUpdateTimer += deltaTime;
}

void NetworkManager::updateRemotePlayerEntities() {
    std::vector<RemotePlayer> remotePlayers;
    
    if (m_mode == NetworkMode::HOST && m_server) {
        remotePlayers = m_server->getPlayers();
    } else if (m_mode == NetworkMode::CLIENT && m_client) {
        remotePlayers = m_client->getRemotePlayers();
    }
    
    // Update or create entities for each remote player
    for (const auto& player : remotePlayers) {
        auto it = std::find_if(m_remotePlayerEntities.begin(), m_remotePlayerEntities.end(),
            [&player](const auto& entity) { return entity->getPlayerId() == player.id; });
        
        if (it != m_remotePlayerEntities.end()) {
            // Update existing entity
            (*it)->setTargetPosition(player.position);
            (*it)->setTargetRotation(player.yaw, player.pitch);
            (*it)->setTargetVelocity(player.velocity);
            (*it)->setHeldItem(player.heldItem);
        } else {
            // Create new entity with the player's model index
            auto entity = std::make_unique<RemotePlayerEntity>(player.id, player.name, player.position, player.modelIndex);
            entity->setTargetRotation(player.yaw, player.pitch);
            
            // Get or load the correct player model for this player's modelIndex
            int modelIdx = player.modelIndex;
            if (modelIdx < 0 || modelIdx >= Settings::NUM_PLAYER_MODELS) {
                modelIdx = 0;  // Default to first model
            }
            
            // Check if model is already cached
            auto modelIt = m_playerModels.find(modelIdx);
            if (modelIt == m_playerModels.end()) {
                // Load model for this index
                try {
                    auto newModel = std::make_shared<ModelSystem::Model>(Settings::PLAYER_MODEL_PATHS[modelIdx]);
                    m_playerModels[modelIdx] = newModel;
                    entity->setModel(newModel);
                } catch (...) {
                    LOG_ERROR("Failed to load model for player: " + player.name);
                }
            } else {
                entity->setModel(modelIt->second);
            }
            
            LOG_INFO("Created entity for player: " + player.name + " with model " + std::to_string(modelIdx));
            m_remotePlayerEntities.push_back(std::move(entity));
        }
    }
    
    // Remove entities for players that left
    m_remotePlayerEntities.erase(
        std::remove_if(m_remotePlayerEntities.begin(), m_remotePlayerEntities.end(),
            [&remotePlayers](const auto& entity) {
                return std::none_of(remotePlayers.begin(), remotePlayers.end(),
                    [&entity](const auto& player) { return player.id == entity->getPlayerId(); });
            }),
        m_remotePlayerEntities.end());
}

void NetworkManager::sendLocalPlayerState(const glm::vec3& pos, float yaw, float pitch,
                                          const glm::vec3& velocity, bool onGround, uint8_t heldItem) {
    // Throttle position updates
    if (m_positionUpdateTimer < POSITION_UPDATE_INTERVAL) {
        return;
    }
    m_positionUpdateTimer = 0.0f;
    
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendPosition(pos, yaw, pitch, velocity, onGround, heldItem);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        // Host broadcasts their position to all connected clients
        m_server->updateHostPosition(pos, yaw, pitch, velocity, onGround, heldItem);
    }
}

void NetworkManager::sendBlockChange(int x, int y, int z, uint8_t blockType) {
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendBlockChange(x, y, z, blockType);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        // Host broadcasts directly
        m_server->broadcastBlockChange(x, y, z, blockType);
    }
}

void NetworkManager::sendChatMessage(const std::string& message) {
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendChatMessage(message);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        m_server->broadcastChatMessage(message);
    }
}

void NetworkManager::sendTimeSync(float timeOfDay, bool isPaused) {
    // Only host/server can sync time
    if (m_mode == NetworkMode::HOST && m_server) {
        m_server->broadcastTimeSync(timeOfDay, isPaused);
    }
}

void NetworkManager::broadcastEntitySpawn(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw) {
    if (m_mode == NetworkMode::HOST && m_server) {
        m_server->broadcastEntitySpawn(entityId, mobType, pos, yaw);
    }
}

void NetworkManager::broadcastEntityDespawn(uint32_t entityId) {
    if (m_mode == NetworkMode::HOST && m_server) {
        m_server->broadcastEntityDespawn(entityId);
    }
}

void NetworkManager::broadcastEntityUpdate(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags) {
    if (m_mode == NetworkMode::HOST && m_server) {
        m_server->broadcastEntityUpdate(entityId, pos, vel, yaw, health, flags);
    }
}

void NetworkManager::sendPlayerDamage(uint32_t targetPlayerId, float damage, const glm::vec3& knockback) {
    uint32_t attackerId = getLocalPlayerId();
    
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendPlayerDamage(attackerId, targetPlayerId, damage, knockback);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        // Host broadcasts directly
        m_server->broadcastPlayerDamage(attackerId, targetPlayerId, damage, knockback);
    }
}

void NetworkManager::sendPlayerAnimation(uint8_t animationType) {
    uint32_t playerId = getLocalPlayerId();
    
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendPlayerAnimation(animationType);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        // Host broadcasts directly
        m_server->broadcastPlayerAnimation(playerId, animationType);
    }
}

std::vector<RemotePlayerEntity*> NetworkManager::getRemotePlayerEntities() {
    std::vector<RemotePlayerEntity*> entities;
    entities.reserve(m_remotePlayerEntities.size());
    for (auto& entity : m_remotePlayerEntities) {
        entities.push_back(entity.get());
    }
    return entities;
}

uint32_t NetworkManager::getLocalPlayerId() const {
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getLocalPlayerId();
    }
    return 0;  // Host is always ID 0
}

int64_t NetworkManager::getWorldSeed() const {
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getWorldSeed();
    }
    return m_worldSeed;
}

glm::vec3 NetworkManager::getSpawnPosition() const {
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getSpawnPosition();
    }
    return m_spawnPos;
}

size_t NetworkManager::getPlayerCount() const {
    if (m_mode == NetworkMode::HOST && m_server) {
        return m_server->getPlayerCount() + 1;  // +1 for host
    } else if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getRemotePlayers().size() + 1;  // +1 for local player
    }
    return 1;
}

uint64_t NetworkManager::getLatency() const {
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getLatency();
    }
    return 0;
}

bool NetworkManager::isConnecting() const {
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->getState() == ConnectionState::CONNECTING;
    }
    return false;
}

bool NetworkManager::isConnected() const {
    if (m_mode == NetworkMode::HOST) {
        return true;  // Host is always "connected"
    }
    if (m_mode == NetworkMode::CLIENT && m_client) {
        return m_client->isConnected();
    }
    return false;
}

std::string NetworkManager::getStatusString() const {
    switch (m_mode) {
        case NetworkMode::OFFLINE:
            return "Offline";
        case NetworkMode::HOST:
            return "Hosting (" + std::to_string(getPlayerCount()) + " players)";
        case NetworkMode::CLIENT:
            if (isConnecting()) return "Connecting...";
            if (isConnected()) return "Connected (ping: " + std::to_string(getLatency()) + "ms)";
            return "Disconnected";
    }
    return "Unknown";
}

void NetworkManager::setupServerCallbacks() {
    m_server->setBlockChangeCallback([this](int x, int y, int z, uint8_t blockType, uint32_t /*playerId*/) {
        if (m_onBlockChange) {
            m_onBlockChange(x, y, z, blockType);
        }
    });
    
    m_server->setPlayerJoinCallback([this](uint32_t playerId, const std::string& name) {
        LOG_INFO("Player joined server: " + name + " (ID: " + std::to_string(playerId) + ")");
        if (m_onChat) {
            m_onChat("Server", name + " joined the game");
        }
        // Call player join callback so host can send entity states
        if (m_onPlayerJoin) {
            m_onPlayerJoin(playerId, name);
        }
    });
    
    m_server->setPlayerLeaveCallback([this](uint32_t playerId) {
        LOG_INFO("Player left server: ID " + std::to_string(playerId));
    });
    
    m_server->setChatCallback([this](uint32_t senderId, const std::string& message) {
        // Find player name
        std::string playerName = "Player " + std::to_string(senderId);
        auto players = m_server->getPlayers();
        for (const auto& p : players) {
            if (p.id == senderId) {
                playerName = p.name;
                break;
            }
        }
        
        if (m_onChat) {
            m_onChat(playerName, message);
        }
    });
    
    m_server->setPlayerDamageCallback([this](uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback) {
        if (m_onPlayerDamage) {
            m_onPlayerDamage(attackerId, targetId, damage, knockback);
        }
    });
    
    m_server->setPlayerAnimationCallback([this](uint32_t playerId, uint8_t animationType) {
        // Find the remote player entity and play the animation
        for (auto& entity : m_remotePlayerEntities) {
            if (entity && entity->getPlayerId() == playerId) {
                if (animationType == PlayerAnimationPacket::ANIM_ATTACK || 
                    animationType == PlayerAnimationPacket::ANIM_PUNCH) {
                    entity->playAttackAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_HIT_REACT) {
                    entity->playHitReceiveAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_DEATH) {
                    entity->playDeathAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_WAVE) {
                    entity->playWaveAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_YES) {
                    entity->playYesAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_NO) {
                    entity->playNoAnimation();
                }
                break;
            }
        }
    });
}

void NetworkManager::setupClientCallbacks() {
    m_client->setBlockChangeCallback([this](int x, int y, int z, uint8_t blockType) {
        if (m_onBlockChange) {
            m_onBlockChange(x, y, z, blockType);
        }
    });
    
    m_client->setPlayerJoinCallback([this](uint32_t playerId, const std::string& name, const glm::vec3& /*pos*/) {
        LOG_INFO("Player joined: " + name + " (ID: " + std::to_string(playerId) + ")");
        if (m_onChat) {
            m_onChat("Server", name + " joined the game");
        }
    });
    
    m_client->setPlayerLeaveCallback([this](uint32_t playerId) {
        LOG_INFO("Player left: ID " + std::to_string(playerId));
    });
    
    m_client->setChatCallback([this](uint32_t senderId, const std::string& message) {
        std::string playerName = "Player " + std::to_string(senderId);
        if (senderId == 0) {
            playerName = "Server";
        } else {
            auto players = m_client->getRemotePlayers();
            for (const auto& p : players) {
                if (p.id == senderId) {
                    playerName = p.name;
                    break;
                }
            }
            if (senderId == m_client->getLocalPlayerId()) {
                playerName = m_localPlayerName;
            }
        }
        
        if (m_onChat) {
            m_onChat(playerName, message);
        }
    });
    
    m_client->setDisconnectCallback([this](const std::string& reason) {
        LOG_INFO("Disconnected: " + reason);
        if (m_onDisconnect) {
            m_onDisconnect(reason);
        }
    });
    
    m_client->setConnectedCallback([this]() {
        LOG_INFO("Successfully connected to server");
        if (m_onConnected) {
            m_onConnected();
        }
    });
    
    m_client->setTimeSyncCallback([this](float timeOfDay, bool isPaused) {
        if (m_onTimeSync) {
            m_onTimeSync(timeOfDay, isPaused);
        }
    });
    
    m_client->setEntitySpawnCallback([this](uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw) {
        if (m_onEntitySpawn) {
            m_onEntitySpawn(entityId, mobType, pos, yaw);
        }
    });
    
    m_client->setEntityDespawnCallback([this](uint32_t entityId) {
        if (m_onEntityDespawn) {
            m_onEntityDespawn(entityId);
        }
    });
    
    m_client->setEntityUpdateCallback([this](uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags) {
        if (m_onEntityUpdate) {
            m_onEntityUpdate(entityId, pos, vel, yaw, health, flags);
        }
    });
    
    m_client->setPlayerDamageCallback([this](uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback) {
        if (m_onPlayerDamage) {
            m_onPlayerDamage(attackerId, targetId, damage, knockback);
        }
    });
    
    m_client->setPlayerAnimationCallback([this](uint32_t playerId, uint8_t animationType) {
        // Find the remote player entity and play the animation
        for (auto& entity : m_remotePlayerEntities) {
            if (entity && entity->getPlayerId() == playerId) {
                if (animationType == PlayerAnimationPacket::ANIM_ATTACK || 
                    animationType == PlayerAnimationPacket::ANIM_PUNCH) {
                    entity->playAttackAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_HIT_REACT) {
                    entity->playHitReceiveAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_DEATH) {
                    entity->playDeathAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_WAVE) {
                    entity->playWaveAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_YES) {
                    entity->playYesAnimation();
                } else if (animationType == PlayerAnimationPacket::ANIM_NO) {
                    entity->playNoAnimation();
                }
                break;
            }
        }
    });
}

} // namespace Network
