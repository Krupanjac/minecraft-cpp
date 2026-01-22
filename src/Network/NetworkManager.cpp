#include "NetworkManager.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../Model/Model.h"
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
        // velocity is set via setTargetVelocity which stores in the inherited 'velocity' member
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        std::string currentAnim = model->getCurrentAnimation();
        
        if (speed > 0.1f) {
            // Walking/Running
            if (currentAnim != "walk" && currentAnim != "run" && currentAnim.find("walk") == std::string::npos) {
                model->playAnimation("walk", true);
            }
        } else {
            // Idle
            if (currentAnim != "idle" && currentAnim.find("idle") == std::string::npos) {
                model->playAnimation("idle", true);
                // If "idle" not found, try "idle1"
                if (model->getCurrentAnimation() != "idle") {
                    model->playAnimation("idle1", true);
                }
            }
        }
        
        // Update model animation
        model->updateAnimation(deltaTime);
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
                                          const glm::vec3& velocity, bool onGround) {
    // Throttle position updates
    if (m_positionUpdateTimer < POSITION_UPDATE_INTERVAL) {
        return;
    }
    m_positionUpdateTimer = 0.0f;
    
    if (m_mode == NetworkMode::CLIENT && m_client && m_client->isConnected()) {
        m_client->sendPosition(pos, yaw, pitch, velocity, onGround);
    } else if (m_mode == NetworkMode::HOST && m_server) {
        // Host broadcasts their position to all connected clients
        m_server->updateHostPosition(pos, yaw, pitch, velocity, onGround);
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
}

} // namespace Network
