#include "GameServer.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"
#include <chrono>
#include <algorithm>

namespace Network {

GameServer::GameServer() = default;

GameServer::~GameServer() {
    stop();
}

bool GameServer::start(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& hostName) {
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
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        
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
    
    // Accept new connections
    acceptConnections();
    
    // Process existing clients
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
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
    
    std::lock_guard<std::mutex> lock(m_clientsMutex);
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
    switch (type) {
        case PacketType::CONNECT_REQUEST: {
            uint16_t protocolVersion = buffer.readU16();
            std::string playerName = buffer.readString(32);
            
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
            
            LOG_INFO("Player '" + playerName + "' connected with ID " + std::to_string(client.playerId));
            
            // Send accept response
            PacketBuffer response;
            serializeConnectResponse(response, true, client.playerId, m_worldSeed, m_spawnPos);
            sendToClient(client, response);
            
            // Send host player info to new client (host is always ID 0)
            {
                PacketBuffer hostJoin;
                serializePlayerJoin(hostJoin, 0, m_hostName, m_hostPosition, m_hostYaw, m_hostPitch);
                sendToClient(client, hostJoin);
            }
            
            // Notify existing players about new player
            PacketBuffer joinPacket;
            serializePlayerJoin(joinPacket, client.playerId, client.playerName, 
                               client.position, client.yaw, client.pitch);
            broadcastToAll(joinPacket, client.playerId);
            
            // Send existing players to new client
            for (const auto& other : m_clients) {
                if (other->playerId != 0 && other->playerId != client.playerId) {
                    PacketBuffer otherJoin;
                    serializePlayerJoin(otherJoin, other->playerId, other->playerName,
                                       other->position, other->yaw, other->pitch);
                    sendToClient(client, otherJoin);
                }
            }
            
            if (m_onPlayerJoin) {
                m_onPlayerJoin(client.playerId, client.playerName);
            }
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
            
            // Broadcast to other clients
            PacketBuffer posPacket;
            serializePlayerPosition(posPacket, client.playerId, client.position,
                                   client.yaw, client.pitch, client.velocity, client.onGround);
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
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
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
        players.push_back(player);
    }
    return players;
}

void GameServer::broadcastBlockChange(int x, int y, int z, uint8_t blockType) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    serializeBlockChange(packet, x, y, z, blockType);
    broadcastToAll(packet);
}

void GameServer::broadcastChatMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    PacketBuffer packet;
    serializeChatMessage(packet, 0, message);  // 0 = server message
    broadcastToAll(packet);
}

void GameServer::broadcastTimeSync(float timeOfDay, bool isPaused) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    m_timeOfDay = timeOfDay;
    m_timePaused = isPaused;
    
    PacketBuffer packet;
    serializeTimeSync(packet, timeOfDay, isPaused);
    broadcastToAll(packet);
}

size_t GameServer::getPlayerCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return std::count_if(m_clients.begin(), m_clients.end(),
        [](const auto& client) { return client->playerId != 0; });
}

void GameServer::updateHostPosition(const glm::vec3& position, float yaw, float pitch,
                                    const glm::vec3& velocity, bool onGround) {
    m_hostPosition = position;
    m_hostYaw = yaw;
    m_hostPitch = pitch;
    m_hostVelocity = velocity;
    m_hostOnGround = onGround;
    
    // Broadcast host position to all clients
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    PacketBuffer posPacket;
    serializePlayerPosition(posPacket, 0, m_hostPosition, m_hostYaw, m_hostPitch, m_hostVelocity, m_hostOnGround);
    broadcastToAll(posPacket);
}

} // namespace Network
