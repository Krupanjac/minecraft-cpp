#include "NetworkManager.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include "../Model/Model.h"
#include <algorithm>

namespace Network {

// ============== RemotePlayerEntity ==============

RemotePlayerEntity::RemotePlayerEntity(uint32_t playerId, const std::string& name, const glm::vec3& pos)
    : Entity(pos)
    , m_playerId(playerId)
    , m_playerName(name)
    , m_targetPosition(pos) {
    
    // Scale for player model
    scale = glm::vec3(1.0f);
}

void RemotePlayerEntity::update(float deltaTime) {
    // Store previous state for motion vectors
    prevPosition = position;
    prevRotation = rotation;
    prevScale = scale;
    
    // Interpolate position smoothly
    float t = std::min(1.0f, INTERPOLATION_SPEED * deltaTime);
    position = glm::mix(position, m_targetPosition, t);
    
    // Interpolate rotation
    float currentYaw = rotation.y;
    float targetYaw = m_targetYaw;
    
    // Handle yaw wraparound
    float yawDiff = targetYaw - currentYaw;
    if (yawDiff > 180.0f) yawDiff -= 360.0f;
    if (yawDiff < -180.0f) yawDiff += 360.0f;
    
    rotation.y = currentYaw + yawDiff * t;
    rotation.x = glm::mix(rotation.x, m_targetPitch, t);
}

// ============== NetworkManager ==============

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    disconnect();
}

bool NetworkManager::hostGame(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& playerName) {
    if (m_mode != NetworkMode::OFFLINE) {
        LOG_WARNING("Already in a game");
        return false;
    }
    
    m_server = std::make_unique<GameServer>();
    
    if (!m_server->start(port, worldSeed, spawnPos, playerName)) {
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

bool NetworkManager::joinGame(const std::string& host, uint16_t port, const std::string& playerName) {
    if (m_mode != NetworkMode::OFFLINE) {
        LOG_WARNING("Already in a game");
        return false;
    }
    
    m_client = std::make_unique<GameClient>();
    
    if (!m_client->connect(host, port, playerName)) {
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
            // Create new entity
            auto entity = std::make_unique<RemotePlayerEntity>(player.id, player.name, player.position);
            entity->setTargetRotation(player.yaw, player.pitch);
            
            // Try to load player model if not already loaded
            if (!m_playerModel) {
                try {
                    m_playerModel = std::make_shared<ModelSystem::Model>("assets/models/Player/scene.gltf");
                } catch (...) {
                    // Model loading may fail, that's okay
                }
            }
            
            if (m_playerModel) {
                entity->setModel(m_playerModel);
            }
            
            LOG_INFO("Created entity for player: " + player.name);
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
}

} // namespace Network
