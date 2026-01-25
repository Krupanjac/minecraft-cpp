#pragma once

#include "Socket.h"
#include "Protocol.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <glm/glm.hpp>

namespace Network {

// Remote player state
struct RemotePlayer {
    uint32_t id = 0;
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = false;
    bool isFlying = false;
    bool isSprinting = false;
    bool isSneaking = false;
    uint8_t modelIndex = 0;  // Player's selected character model
    uint8_t heldItem = 0;    // Currently held item type
};

// Connected client info (server-side)
struct ClientConnection {
    Socket socket;
    uint32_t playerId = 0;
    std::string playerName;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = false;
    uint8_t modelIndex = 0;  // Player's selected character model
    uint8_t heldItem = 0;    // Currently held item type
    std::vector<uint8_t> receiveBuffer;
    uint64_t lastPingTime = 0;
    uint64_t latency = 0;
};

class GameServer {
public:
    GameServer();
    ~GameServer();
    
    // Server lifecycle
    bool start(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& hostName = "Host", uint8_t hostModelIndex = 0);
    void stop();
    bool isRunning() const { return m_running; }
    
    // Call each frame to process network events
    void update();
    
    // Get connected players (for rendering)
    std::vector<RemotePlayer> getPlayers() const;
    
    // Broadcast events
    void broadcastBlockChange(int x, int y, int z, uint8_t blockType);
    void broadcastChatMessage(const std::string& message);
    void broadcastTimeSync(float timeOfDay, bool isPaused);
    
    // Entity sync - call from host to sync mobs to clients
    void broadcastEntitySpawn(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw);
    void broadcastEntityDespawn(uint32_t entityId);
    void broadcastEntityUpdate(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags);
    
    // Player damage for PvP
    void broadcastPlayerDamage(uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback);
    
    // Update host player position (broadcasts to all clients)
    void updateHostPosition(const glm::vec3& position, float yaw, float pitch,
                           const glm::vec3& velocity, bool onGround, uint8_t heldItem = 0);
    
    // Time management (server-authoritative)
    void setTimeOfDay(float time) { m_timeOfDay = time; }
    float getTimeOfDay() const { return m_timeOfDay; }
    void setTimePaused(bool paused) { m_timePaused = paused; }
    bool isTimePaused() const { return m_timePaused; }
    
    // Callbacks
    using BlockChangeCallback = std::function<void(int x, int y, int z, uint8_t blockType, uint32_t playerId)>;
    using PlayerJoinCallback = std::function<void(uint32_t playerId, const std::string& name)>;
    using PlayerLeaveCallback = std::function<void(uint32_t playerId)>;
    using ChatCallback = std::function<void(uint32_t senderId, const std::string& message)>;
    using PlayerDamageCallback = std::function<void(uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback)>;
    
    void setBlockChangeCallback(BlockChangeCallback cb) { m_onBlockChange = std::move(cb); }
    void setPlayerJoinCallback(PlayerJoinCallback cb) { m_onPlayerJoin = std::move(cb); }
    void setPlayerLeaveCallback(PlayerLeaveCallback cb) { m_onPlayerLeave = std::move(cb); }
    void setChatCallback(ChatCallback cb) { m_onChat = std::move(cb); }
    void setPlayerDamageCallback(PlayerDamageCallback cb) { m_onPlayerDamage = std::move(cb); }
    
    uint16_t getPort() const { return m_port; }
    size_t getPlayerCount() const;
    
private:
    void acceptConnections();
    void processClient(ClientConnection& client);
    void handlePacket(ClientConnection& client, PacketType type, PacketBuffer& buffer);
    void sendToClient(ClientConnection& client, const PacketBuffer& buffer);
    void broadcastToAll(const PacketBuffer& buffer, uint32_t excludeId = 0);
    void removeClient(uint32_t playerId);
    
    Socket m_listenSocket;
    std::vector<std::unique_ptr<ClientConnection>> m_clients;
    mutable std::recursive_mutex m_clientsMutex;
    
    std::atomic<bool> m_running{false};
    uint16_t m_port = 0;
    int64_t m_worldSeed = 0;
    glm::vec3 m_spawnPos{0.0f};
    
    uint32_t m_nextPlayerId = 1;
    
    // Host player state (ID 0)
    glm::vec3 m_hostPosition{0.0f};
    glm::vec3 m_hostVelocity{0.0f};
    float m_hostYaw = 0.0f;
    float m_hostPitch = 0.0f;
    bool m_hostOnGround = false;
    uint8_t m_hostHeldItem = 0;
    std::string m_hostName = "Host";
    uint8_t m_hostModelIndex = 0;
    
    // Server-authoritative time
    float m_timeOfDay = 600.0f;  // Start at noon
    bool m_timePaused = false;
    float m_timeSyncTimer = 0.0f;
    
    // Callbacks
    BlockChangeCallback m_onBlockChange;
    PlayerJoinCallback m_onPlayerJoin;
    PlayerLeaveCallback m_onPlayerLeave;
    ChatCallback m_onChat;
    PlayerDamageCallback m_onPlayerDamage;
};

} // namespace Network
