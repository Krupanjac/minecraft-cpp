#pragma once

#include "Socket.h"
#include "Protocol.h"
#include "GameServer.h"  // For RemotePlayer struct
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <glm/glm.hpp>

namespace Network {

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class GameClient {
public:
    GameClient();
    ~GameClient();
    
    // Connection lifecycle
    bool connect(const std::string& host, uint16_t port, const std::string& playerName, uint8_t modelIndex = 0);
    void disconnect();
    bool isConnected() const { return m_state == ConnectionState::CONNECTED; }
    ConnectionState getState() const { return m_state; }
    
    // Call each frame to process network events
    void update();
    
    // Get remote players (for rendering)
    std::vector<RemotePlayer> getRemotePlayers() const;
    RemotePlayer* getPlayer(uint32_t id);
    
    // Send local player state
    void sendPosition(const glm::vec3& pos, float yaw, float pitch, 
                      const glm::vec3& velocity, bool onGround);
    void sendBlockChange(int x, int y, int z, uint8_t blockType);
    void sendChatMessage(const std::string& message);
    
    // Getters
    uint32_t getLocalPlayerId() const { return m_localPlayerId; }
    int64_t getWorldSeed() const { return m_worldSeed; }
    glm::vec3 getSpawnPosition() const { return m_spawnPos; }
    const std::string& getPlayerName() const { return m_playerName; }
    uint64_t getLatency() const { return m_latency; }
    
    // Callbacks
    using BlockChangeCallback = std::function<void(int x, int y, int z, uint8_t blockType)>;
    using PlayerJoinCallback = std::function<void(uint32_t playerId, const std::string& name, const glm::vec3& pos)>;
    using PlayerLeaveCallback = std::function<void(uint32_t playerId)>;
    using ChatCallback = std::function<void(uint32_t senderId, const std::string& message)>;
    using DisconnectCallback = std::function<void(const std::string& reason)>;
    using ConnectedCallback = std::function<void()>;
    using TimeSyncCallback = std::function<void(float timeOfDay, bool isPaused)>;
    using EntitySpawnCallback = std::function<void(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw)>;
    using EntityDespawnCallback = std::function<void(uint32_t entityId)>;
    using EntityUpdateCallback = std::function<void(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags)>;
    
    void setBlockChangeCallback(BlockChangeCallback cb) { m_onBlockChange = std::move(cb); }
    void setPlayerJoinCallback(PlayerJoinCallback cb) { m_onPlayerJoin = std::move(cb); }
    void setPlayerLeaveCallback(PlayerLeaveCallback cb) { m_onPlayerLeave = std::move(cb); }
    void setChatCallback(ChatCallback cb) { m_onChat = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) { m_onDisconnect = std::move(cb); }
    void setConnectedCallback(ConnectedCallback cb) { m_onConnected = std::move(cb); }
    void setTimeSyncCallback(TimeSyncCallback cb) { m_onTimeSync = std::move(cb); }
    void setEntitySpawnCallback(EntitySpawnCallback cb) { m_onEntitySpawn = std::move(cb); }
    void setEntityDespawnCallback(EntityDespawnCallback cb) { m_onEntityDespawn = std::move(cb); }
    void setEntityUpdateCallback(EntityUpdateCallback cb) { m_onEntityUpdate = std::move(cb); }
    
private:
    void processPackets();
    void handlePacket(PacketType type, PacketBuffer& buffer);
    void sendPacket(const PacketBuffer& buffer);
    void sendPing();
    
    Socket m_socket;
    std::vector<uint8_t> m_receiveBuffer;
    
    ConnectionState m_state = ConnectionState::DISCONNECTED;
    std::string m_playerName;
    uint32_t m_localPlayerId = 0;
    int64_t m_worldSeed = 0;
    glm::vec3 m_spawnPos{0.0f};
    
    // Remote players
    std::unordered_map<uint32_t, RemotePlayer> m_remotePlayers;
    mutable std::mutex m_playersMutex;
    
    // Ping/latency tracking
    uint64_t m_lastPingTime = 0;
    uint64_t m_latency = 0;
    float m_pingTimer = 0.0f;
    
    // Position update throttling
    float m_positionUpdateTimer = 0.0f;
    static constexpr float POSITION_UPDATE_INTERVAL = 0.05f;  // 20 updates per second
    
    // Callbacks
    BlockChangeCallback m_onBlockChange;
    PlayerJoinCallback m_onPlayerJoin;
    PlayerLeaveCallback m_onPlayerLeave;
    ChatCallback m_onChat;
    DisconnectCallback m_onDisconnect;
    ConnectedCallback m_onConnected;
    TimeSyncCallback m_onTimeSync;
    EntitySpawnCallback m_onEntitySpawn;
    EntityDespawnCallback m_onEntityDespawn;
    EntityUpdateCallback m_onEntityUpdate;
};

} // namespace Network
