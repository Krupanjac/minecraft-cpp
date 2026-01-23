#include "GameServer.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include <chrono>
#include <algorithm>

namespace Network {

GameServer::GameServer() = default;

GameServer::~GameServer() {
    stop();
}

bool GameServer::start(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& hostName, uint8_t hostModelIndex) {
    if (m_running) {
        LOG_WARNING("Server already running");
        return false;
    }
    
    if (!initializeNetwork()) {
        LOG_ERROR("Failed to initialize network");
        return false;
    }
    
    if (!m_listenSocket.create()) {
        LOG_ERROR("Failed to create server socket");
        return false;
    }
    
    m_listenSocket.setReuseAddr(true);
    
    if (!m_listenSocket.bind(port)) {
        LOG_ERROR("Failed to bind to port " + std::to_string(port));
        m_listenSocket.close();
        return false;
    }
    
    if (!m_listenSocket.listen()) {
        LOG_ERROR("Failed to listen on socket");
        m_listenSocket.close();
        return false;
    }
    
    m_listenSocket.setNonBlocking(true);
    
    m_port = port;
    m_worldSeed = worldSeed;
    m_spawnPos = spawnPos;
    m_hostPosition = spawnPos;
    m_hostName = hostName;
    m_hostModelIndex = hostModelIndex;
    m_running = true;
    m_nextPlayerId = 1;
    
    LOG_INFO("Server started on port " + std::to_string(port));
    return true;
}

void GameServer::stop() {
    if (!m_running) return;
    
    m_running = false;
    
    // Disconnect all clients
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        
        PacketBuffer disconnectPacket;
        serializeDisconnect(disconnectPacket, "Server shutting down");
        
        for (auto& client : m_clients) {
            sendToClient(*client, disconnectPacket);
            client->socket.close();
        }
        m_clients.clear();
    }
    
    m_listenSocket.close();
    LOG_INFO("Server stopped");
}

void GameServer::update() {
    if (!m_running) return;
    
    try {
        // Accept new connections
        acceptConnections();
        
        // Process existing clients
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        
        std::vector<uint32_t> disconnectedClients;
        
        for (auto& client : m_clients) {
            if (!client->socket.isValid()) {
                disconnectedClients.push_back(client->playerId);
                continue;
            }
            
            processClient(*client);
        }
        
        // Remove disconnected clients
        for (uint32_t id : disconnectedClients) {
            removeClient(id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in GameServer::update: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in GameServer::update");
    }
}

void GameServer::acceptConnections() {
    auto newClient = m_listenSocket.accept();
    if (!newClient) return;
    
    newClient->setNonBlocking(true);
    
    auto connection = std::make_unique<ClientConnection>();
    connection->socket = std::move(*newClient);
    connection->playerId = 0;  // Not assigned yet
    connection->receiveBuffer.reserve(4096);
    
    LOG_INFO("New connection from " + connection->socket.getRemoteAddress());
    
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    m_clients.push_back(std::move(connection));
}

void GameServer::processClient(ClientConnection& client) {
    // Receive data
    uint8_t buffer[4096];
    int received = client.socket.receive(buffer, sizeof(buffer));
    
    if (received > 0) {
        client.receiveBuffer.insert(client.receiveBuffer.end(), buffer, buffer + received);
    } else if (received == 0) {
        // Connection closed
        client.socket.close();
        return;
    }
    
    // Process complete packets
    while (client.receiveBuffer.size() >= PACKET_HEADER_SIZE) {
        // Read header
        PacketType type = static_cast<PacketType>(client.receiveBuffer[0]);
        uint16_t size = (static_cast<uint16_t>(client.receiveBuffer[1]) << 8) | client.receiveBuffer[2];
        
        if (client.receiveBuffer.size() < PACKET_HEADER_SIZE + size) {
            break;  // Wait for more data
        }
        
        // Extract packet data
        PacketBuffer packetData;
        packetData.data().insert(packetData.data().end(),
            client.receiveBuffer.begin() + PACKET_HEADER_SIZE,
            client.receiveBuffer.begin() + PACKET_HEADER_SIZE + size);
        
        // Remove processed data from buffer
        client.receiveBuffer.erase(
            client.receiveBuffer.begin(),
            client.receiveBuffer.begin() + PACKET_HEADER_SIZE + size);
        
        // Handle packet
        handlePacket(client, type, packetData);
    }
}

void GameServer::handlePacket(ClientConnection& client, PacketType type, PacketBuffer& buffer) {
    try {
    switch (type) {
        case PacketType::CONNECT_REQUEST: {
            LOG_DEBUG("Processing CONNECT_REQUEST...");
            uint16_t protocolVersion = buffer.readU16();
            std::string playerName = buffer.readString(32);
            uint8_t modelIndex = buffer.readU8();
            LOG_DEBUG("Player name: " + playerName + ", protocol: " + std::to_string(protocolVersion) + ", model: " + std::to_string(modelIndex));
            
            if (protocolVersion != PROTOCOL_VERSION) {
                PacketBuffer response;
                serializeConnectResponse(response, false, 0, 0, glm::vec3(0), "Protocol version mismatch");
                sendToClient(client, response);
                client.socket.close();
                return;
            }
            
            // Assign player ID
            client.playerId = m_nextPlayerId++;
            client.playerName = playerName;
            client.position = m_spawnPos;
            client.modelIndex = modelIndex;
            
            LOG_INFO("Player '" + playerName + "' connected with ID " + std::to_string(client.playerId));
            
            // Send accept response
            PacketBuffer response;
            serializeConnectResponse(response, true, client.playerId, m_worldSeed, m_spawnPos);
            sendToClient(client, response);
            
            // Send host player info to new client (host is always ID 0)
            {
                PacketBuffer hostJoin;
                serializePlayerJoin(hostJoin, 0, m_hostName, m_hostPosition, m_hostYaw, m_hostPitch, m_hostModelIndex);
                sendToClient(client, hostJoin);
            }
            
            // Notify existing players about new player
            PacketBuffer joinPacket;
            serializePlayerJoin(joinPacket, client.playerId, client.playerName, 
                               client.position, client.yaw, client.pitch, client.modelIndex);
            broadcastToAll(joinPacket, client.playerId);
            
            // Send existing players to new client
            for (const auto& other : m_clients) {
                if (other->playerId != 0 && other->playerId != client.playerId) {
                    PacketBuffer otherJoin;
                    serializePlayerJoin(otherJoin, other->playerId, other->playerName,
                                       other->position, other->yaw, other->pitch, other->modelIndex);
                    sendToClient(client, otherJoin);
                }
            }
            
            LOG_DEBUG("About to call onPlayerJoin callback...");
            if (m_onPlayerJoin) {
                LOG_DEBUG("Calling onPlayerJoin callback");
                m_onPlayerJoin(client.playerId, client.playerName);
                LOG_DEBUG("onPlayerJoin callback completed");
            }
            LOG_DEBUG("CONNECT_REQUEST handling complete");
            break;
        }
        
        case PacketType::DISCONNECT: {
            LOG_INFO("Player " + std::to_string(client.playerId) + " disconnected");
            client.socket.close();
            break;
        }
        
        case PacketType::PING: {
            uint64_t timestamp = buffer.readU64();
            PacketBuffer pong;
            serializePong(pong, timestamp);
            sendToClient(client, pong);
            break;
        }
        
        case PacketType::PLAYER_POSITION: {
            /* uint32_t playerId = */ buffer.readU32();  // Should match client.playerId
            client.position.x = buffer.readFloat();
            client.position.y = buffer.readFloat();
            client.position.z = buffer.readFloat();
            client.yaw = buffer.readFloat();
            client.pitch = buffer.readFloat();
            client.velocity.x = buffer.readFloat();
            client.velocity.y = buffer.readFloat();
            client.velocity.z = buffer.readFloat();
            client.onGround = buffer.readU8() != 0;
            client.heldItem = buffer.readU8();
            
            // Broadcast to other clients
            PacketBuffer posPacket;
            serializePlayerPosition(posPacket, client.playerId, client.position,
                                   client.yaw, client.pitch, client.velocity, client.onGround, client.heldItem);
            broadcastToAll(posPacket, client.playerId);
            break;
        }
        
        case PacketType::BLOCK_CHANGE: {
            int32_t x = buffer.readI32();
            int32_t y = buffer.readI32();
            int32_t z = buffer.readI32();
            uint8_t blockType = buffer.readU8();
            
            // Broadcast to all clients (including sender for confirmation)
            PacketBuffer blockPacket;
            serializeBlockChange(blockPacket, x, y, z, blockType);
            broadcastToAll(blockPacket);
            
            if (m_onBlockChange) {
                m_onBlockChange(x, y, z, blockType, client.playerId);
            }
            break;
        }
        
        case PacketType::CHAT_MESSAGE: {
            /* uint32_t senderId = */ buffer.readU32();
            std::string message = buffer.readString(256);
            
            // Broadcast to all
            PacketBuffer chatPacket;
            serializeChatMessage(chatPacket, client.playerId, message);
            broadcastToAll(chatPacket);
            
            if (m_onChat) {
                m_onChat(client.playerId, message);
            }
            break;
        }
        
        default:
            LOG_WARNING("Unknown packet type: " + std::to_string(static_cast<int>(type)));
            break;
    }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in handlePacket: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in handlePacket");
    }
}

void GameServer::sendToClient(ClientConnection& client, const PacketBuffer& buffer) {
    if (!client.socket.isValid()) return;
    
    const auto& data = buffer.data();
    size_t sent = 0;
    while (sent < data.size()) {
        int result = client.socket.send(data.data() + sent, data.size() - sent);
        if (result <= 0) {
            client.socket.close();
            return;
        }
        sent += result;
    }
}

void GameServer::broadcastToAll(const PacketBuffer& buffer, uint32_t excludeId) {
    for (auto& client : m_clients) {
        if (client->playerId != 0 && client->playerId != excludeId) {
            sendToClient(*client, buffer);
        }
    }
}

void GameServer::removeClient(uint32_t playerId) {
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
        [playerId](const auto& client) { return client->playerId == playerId; });
    
    if (it != m_clients.end()) {
        std::string playerName = (*it)->playerName;
        m_clients.erase(it);
        
        // Notify other players
        PacketBuffer leavePacket;
        serializePlayerLeave(leavePacket, playerId);
        broadcastToAll(leavePacket);
        
        if (m_onPlayerLeave) {
            m_onPlayerLeave(playerId);
        }
        
        LOG_INFO("Player " + std::to_string(playerId) + " (" + playerName + ") removed");
    }
}

std::vector<RemotePlayer> GameServer::getPlayers() const {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    std::vector<RemotePlayer> players;
    for (const auto& client : m_clients) {
        if (client->playerId == 0) continue;
        
        RemotePlayer player;
        player.id = client->playerId;
        player.name = client->playerName;
        player.position = client->position;
        player.velocity = client->velocity;
        player.yaw = client->yaw;
        player.pitch = client->pitch;
        player.onGround = client->onGround;
        player.modelIndex = client->modelIndex;
        players.push_back(player);
    }
    return players;
}

void GameServer::broadcastBlockChange(int x, int y, int z, uint8_t blockType) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    serializeBlockChange(packet, x, y, z, blockType);
    broadcastToAll(packet);
}

void GameServer::broadcastChatMessage(const std::string& message) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    serializeChatMessage(packet, 0, message);  // 0 = server message
    broadcastToAll(packet);
}

void GameServer::broadcastTimeSync(float timeOfDay, bool isPaused) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    m_timeOfDay = timeOfDay;
    m_timePaused = isPaused;
    
    PacketBuffer packet;
    serializeTimeSync(packet, timeOfDay, isPaused);
    broadcastToAll(packet);
}

void GameServer::broadcastEntitySpawn(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    // Write packet header manually
    packet.writeU8(static_cast<uint8_t>(PacketType::ENTITY_SPAWN));
    packet.writeU32(entityId);
    packet.writeU8(mobType);
    packet.writeFloat(pos.x);
    packet.writeFloat(pos.y);
    packet.writeFloat(pos.z);
    packet.writeFloat(yaw);
    
    broadcastToAll(packet);
}

void GameServer::broadcastEntityDespawn(uint32_t entityId) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    packet.writeU8(static_cast<uint8_t>(PacketType::ENTITY_DESPAWN));
    packet.writeU32(entityId);
    
    broadcastToAll(packet);
}

void GameServer::broadcastEntityUpdate(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    packet.writeU8(static_cast<uint8_t>(PacketType::ENTITY_UPDATE));
    packet.writeU32(entityId);
    packet.writeFloat(pos.x);
    packet.writeFloat(pos.y);
    packet.writeFloat(pos.z);
    packet.writeFloat(vel.x);
    packet.writeFloat(vel.y);
    packet.writeFloat(vel.z);
    packet.writeFloat(yaw);
    packet.writeFloat(health);
    packet.writeU8(flags);
    
    broadcastToAll(packet);
}

size_t GameServer::getPlayerCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    return std::count_if(m_clients.begin(), m_clients.end(),
        [](const auto& client) { return client->playerId != 0; });
}

void GameServer::updateHostPosition(const glm::vec3& position, float yaw, float pitch,
                                    const glm::vec3& velocity, bool onGround, uint8_t heldItem) {
    m_hostPosition = position;
    m_hostYaw = yaw;
    m_hostPitch = pitch;
    m_hostVelocity = velocity;
    m_hostOnGround = onGround;
    m_hostHeldItem = heldItem;
    
    // Broadcast host position to all clients
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    PacketBuffer posPacket;
    serializePlayerPosition(posPacket, 0, m_hostPosition, m_hostYaw, m_hostPitch, m_hostVelocity, m_hostOnGround, m_hostHeldItem);
    broadcastToAll(posPacket);
}

} // namespace Network
