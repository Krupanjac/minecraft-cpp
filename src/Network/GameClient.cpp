#include "GameClient.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include <chrono>

namespace Network {

GameClient::GameClient() {
    m_receiveBuffer.reserve(8192);
}

GameClient::~GameClient() {
    disconnect();
}

bool GameClient::connect(const std::string& host, uint16_t port, const std::string& playerName, uint8_t modelIndex) {
    if (m_state == ConnectionState::CONNECTED || m_state == ConnectionState::CONNECTING) {
        LOG_WARNING("Already connected or connecting");
        return false;
    }
    
    if (!initializeNetwork()) {
        LOG_ERROR("Failed to initialize network");
        m_state = ConnectionState::FAILED;
        return false;
    }
    
    m_playerName = playerName;
    m_state = ConnectionState::CONNECTING;
    
    if (!m_socket.create()) {
        LOG_ERROR("Failed to create client socket");
        m_state = ConnectionState::FAILED;
        return false;
    }
    
    LOG_INFO("Connecting to " + host + ":" + std::to_string(port));
    
    if (!m_socket.connect(host, port)) {
        LOG_ERROR("Failed to connect to server");
        m_socket.close();
        m_state = ConnectionState::FAILED;
        return false;
    }
    
    m_socket.setNonBlocking(true);
    
    // Send connect request with model index
    PacketBuffer connectPacket;
    serializeConnectRequest(connectPacket, playerName, modelIndex);
    sendPacket(connectPacket);
    
    LOG_INFO("Connection request sent, waiting for response...");
    return true;
}

void GameClient::disconnect() {
    if (m_state == ConnectionState::DISCONNECTED) return;
    
    if (m_socket.isValid()) {
        PacketBuffer disconnectPacket;
        serializeDisconnect(disconnectPacket, "Client disconnecting");
        sendPacket(disconnectPacket);
        m_socket.close();
    }
    
    {
        std::lock_guard<std::mutex> lock(m_playersMutex);
        m_remotePlayers.clear();
    }
    
    m_state = ConnectionState::DISCONNECTED;
    m_localPlayerId = 0;
    m_receiveBuffer.clear();
    
    LOG_INFO("Disconnected from server");
}

void GameClient::update() {
    if (m_state == ConnectionState::DISCONNECTED || m_state == ConnectionState::FAILED) {
        return;
    }
    
    // Receive data
    uint8_t buffer[4096];
    int received = m_socket.receive(buffer, sizeof(buffer));
    
    if (received > 0) {
        m_receiveBuffer.insert(m_receiveBuffer.end(), buffer, buffer + received);
    } else if (received == 0) {
        // Connection closed by server
        LOG_INFO("Connection closed by server");
        m_state = ConnectionState::DISCONNECTED;
        if (m_onDisconnect) {
            m_onDisconnect("Connection closed");
        }
        return;
    }
    
    // Process packets
    processPackets();
    
    // Send periodic pings (every 2 seconds)
    if (m_state == ConnectionState::CONNECTED) {
        m_pingTimer += 0.016f;  // Approximate frame time
        if (m_pingTimer >= 2.0f) {
            sendPing();
            m_pingTimer = 0.0f;
        }
    }
}

void GameClient::processPackets() {
    while (m_receiveBuffer.size() >= PACKET_HEADER_SIZE) {
        PacketType type = static_cast<PacketType>(m_receiveBuffer[0]);
        uint16_t size = (static_cast<uint16_t>(m_receiveBuffer[1]) << 8) | m_receiveBuffer[2];
        
        if (m_receiveBuffer.size() < PACKET_HEADER_SIZE + size) {
            break;  // Wait for more data
        }
        
        // Extract packet data
        PacketBuffer packetData;
        packetData.data().insert(packetData.data().end(),
            m_receiveBuffer.begin() + PACKET_HEADER_SIZE,
            m_receiveBuffer.begin() + PACKET_HEADER_SIZE + size);
        
        // Remove processed data from buffer
        m_receiveBuffer.erase(
            m_receiveBuffer.begin(),
            m_receiveBuffer.begin() + PACKET_HEADER_SIZE + size);
        
        // Handle packet
        handlePacket(type, packetData);
    }
}

void GameClient::handlePacket(PacketType type, PacketBuffer& buffer) {
    switch (type) {
        case PacketType::CONNECT_RESPONSE: {
            bool accepted = buffer.readU8() != 0;
            uint32_t playerId = buffer.readU32();
            int64_t seed = buffer.readI64();
            float spawnX = buffer.readFloat();
            float spawnY = buffer.readFloat();
            float spawnZ = buffer.readFloat();
            std::string reason = buffer.readString(64);
            
            if (accepted) {
                m_localPlayerId = playerId;
                m_worldSeed = seed;
                m_spawnPos = glm::vec3(spawnX, spawnY, spawnZ);
                m_state = ConnectionState::CONNECTED;
                
                LOG_INFO("Connected to server! Player ID: " + std::to_string(playerId) + 
                        ", Seed: " + std::to_string(seed));
                
                if (m_onConnected) {
                    m_onConnected();
                }
            } else {
                LOG_ERROR("Connection rejected: " + reason);
                m_state = ConnectionState::FAILED;
                m_socket.close();
                
                if (m_onDisconnect) {
                    m_onDisconnect(reason);
                }
            }
            break;
        }
        
        case PacketType::DISCONNECT: {
            std::string reason = buffer.readString(64);
            LOG_INFO("Disconnected by server: " + reason);
            m_state = ConnectionState::DISCONNECTED;
            m_socket.close();
            
            if (m_onDisconnect) {
                m_onDisconnect(reason);
            }
            break;
        }
        
        case PacketType::PONG: {
            uint64_t timestamp = buffer.readU64();
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            m_latency = currentTime - timestamp;
            break;
        }
        
        case PacketType::PLAYER_JOIN: {
            uint32_t playerId = buffer.readU32();
            std::string name = buffer.readString(32);
            float x = buffer.readFloat();
            float y = buffer.readFloat();
            float z = buffer.readFloat();
            float yaw = buffer.readFloat();
            float pitch = buffer.readFloat();
            uint8_t modelIndex = buffer.readU8();
            
            if (playerId != m_localPlayerId) {
                std::lock_guard<std::mutex> lock(m_playersMutex);
                RemotePlayer player;
                player.id = playerId;
                player.name = name;
                player.position = glm::vec3(x, y, z);
                player.yaw = yaw;
                player.pitch = pitch;
                player.modelIndex = modelIndex;
                m_remotePlayers[playerId] = player;
                
                LOG_INFO("Player joined: " + name + " (ID: " + std::to_string(playerId) + ", model: " + std::to_string(modelIndex) + ")");
                
                if (m_onPlayerJoin) {
                    m_onPlayerJoin(playerId, name, player.position);
                }
            }
            break;
        }
        
        case PacketType::PLAYER_LEAVE: {
            uint32_t playerId = buffer.readU32();
            
            {
                std::lock_guard<std::mutex> lock(m_playersMutex);
                auto it = m_remotePlayers.find(playerId);
                if (it != m_remotePlayers.end()) {
                    LOG_INFO("Player left: " + it->second.name);
                    m_remotePlayers.erase(it);
                }
            }
            
            if (m_onPlayerLeave) {
                m_onPlayerLeave(playerId);
            }
            break;
        }
        
        case PacketType::PLAYER_POSITION: {
            uint32_t playerId = buffer.readU32();
            float x = buffer.readFloat();
            float y = buffer.readFloat();
            float z = buffer.readFloat();
            float yaw = buffer.readFloat();
            float pitch = buffer.readFloat();
            float velX = buffer.readFloat();
            float velY = buffer.readFloat();
            float velZ = buffer.readFloat();
            bool onGround = buffer.readU8() != 0;
            uint8_t heldItem = buffer.readU8();
            
            if (playerId != m_localPlayerId) {
                std::lock_guard<std::mutex> lock(m_playersMutex);
                auto it = m_remotePlayers.find(playerId);
                if (it != m_remotePlayers.end()) {
                    it->second.position = glm::vec3(x, y, z);
                    it->second.velocity = glm::vec3(velX, velY, velZ);
                    it->second.yaw = yaw;
                    it->second.pitch = pitch;
                    it->second.onGround = onGround;
                    it->second.heldItem = heldItem;
                }
            }
            break;
        }
        
        case PacketType::BLOCK_CHANGE: {
            int32_t x = buffer.readI32();
            int32_t y = buffer.readI32();
            int32_t z = buffer.readI32();
            uint8_t blockType = buffer.readU8();
            
            if (m_onBlockChange) {
                m_onBlockChange(x, y, z, blockType);
            }
            break;
        }
        
        case PacketType::CHAT_MESSAGE: {
            uint32_t senderId = buffer.readU32();
            std::string message = buffer.readString(256);
            
            if (m_onChat) {
                m_onChat(senderId, message);
            }
            break;
        }
        
        case PacketType::TIME_SYNC: {
            float timeOfDay = buffer.readFloat();
            bool isPaused = buffer.readU8() != 0;
            
            if (m_onTimeSync) {
                m_onTimeSync(timeOfDay, isPaused);
            }
            break;
        }
        
        case PacketType::ENTITY_SPAWN: {
            uint32_t entityId = buffer.readU32();
            uint8_t mobType = buffer.readU8();
            float x = buffer.readFloat();
            float y = buffer.readFloat();
            float z = buffer.readFloat();
            float yaw = buffer.readFloat();
            
            if (m_onEntitySpawn) {
                m_onEntitySpawn(entityId, mobType, glm::vec3(x, y, z), yaw);
            }
            break;
        }
        
        case PacketType::ENTITY_DESPAWN: {
            uint32_t entityId = buffer.readU32();
            
            if (m_onEntityDespawn) {
                m_onEntityDespawn(entityId);
            }
            break;
        }
        
        case PacketType::ENTITY_UPDATE: {
            uint32_t entityId = buffer.readU32();
            float x = buffer.readFloat();
            float y = buffer.readFloat();
            float z = buffer.readFloat();
            float velX = buffer.readFloat();
            float velY = buffer.readFloat();
            float velZ = buffer.readFloat();
            float yaw = buffer.readFloat();
            float health = buffer.readFloat();
            uint8_t flags = buffer.readU8();
            
            if (m_onEntityUpdate) {
                m_onEntityUpdate(entityId, glm::vec3(x, y, z), glm::vec3(velX, velY, velZ), yaw, health, flags);
            }
            break;
        }
        
        case PacketType::PLAYER_DAMAGE: {
            uint32_t attackerId = buffer.readU32();
            uint32_t targetId = buffer.readU32();
            float damage = buffer.readFloat();
            float knockbackX = buffer.readFloat();
            float knockbackY = buffer.readFloat();
            float knockbackZ = buffer.readFloat();
            
            if (m_onPlayerDamage) {
                m_onPlayerDamage(attackerId, targetId, damage, glm::vec3(knockbackX, knockbackY, knockbackZ));
            }
            break;
        }
        
        default:
            LOG_WARNING("Unknown packet type: " + std::to_string(static_cast<int>(type)));
            break;
    }
}

void GameClient::sendPacket(const PacketBuffer& buffer) {
    if (!m_socket.isValid()) return;
    
    const auto& data = buffer.data();
    size_t sent = 0;
    while (sent < data.size()) {
        int result = m_socket.send(data.data() + sent, data.size() - sent);
        if (result <= 0) {
            LOG_ERROR("Failed to send packet");
            return;
        }
        sent += result;
    }
}

void GameClient::sendPing() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    
    PacketBuffer ping;
    serializePing(ping, timestamp);
    sendPacket(ping);
}

void GameClient::sendPosition(const glm::vec3& pos, float yaw, float pitch,
                              const glm::vec3& velocity, bool onGround, uint8_t heldItem) {
    if (!isConnected()) return;
    
    PacketBuffer packet;
    serializePlayerPosition(packet, m_localPlayerId, pos, yaw, pitch, velocity, onGround, heldItem);
    sendPacket(packet);
}

void GameClient::sendBlockChange(int x, int y, int z, uint8_t blockType) {
    if (!isConnected()) return;
    
    PacketBuffer packet;
    serializeBlockChange(packet, x, y, z, blockType);
    sendPacket(packet);
}

void GameClient::sendChatMessage(const std::string& message) {
    if (!isConnected()) return;
    
    PacketBuffer packet;
    serializeChatMessage(packet, m_localPlayerId, message);
    sendPacket(packet);
}

void GameClient::sendPlayerDamage(uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback) {
    if (!isConnected()) return;
    
    PacketBuffer packet;
    serializePlayerDamage(packet, attackerId, targetId, damage, knockback);
    sendPacket(packet);
}

std::vector<RemotePlayer> GameClient::getRemotePlayers() const {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    
    std::vector<RemotePlayer> players;
    players.reserve(m_remotePlayers.size());
    for (const auto& [id, player] : m_remotePlayers) {
        players.push_back(player);
    }
    return players;
}

RemotePlayer* GameClient::getPlayer(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    auto it = m_remotePlayers.find(id);
    if (it != m_remotePlayers.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace Network
