#pragma once

#include "GameServer.h"
#include "GameClient.h"
#include "../Entity/Entity.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Network {

enum class NetworkMode {
    OFFLINE,
    HOST,      // Running server + local play
    CLIENT     // Connected to remote server
};

// Remote player entity for rendering
class RemotePlayerEntity : public Entity {
public:
    RemotePlayerEntity(uint32_t playerId, const std::string& name, const glm::vec3& pos);
    ~RemotePlayerEntity() override = default;
    
    void update(float deltaTime) override;
    
    void setTargetPosition(const glm::vec3& pos) { m_targetPosition = pos; }
    void setTargetRotation(float yaw, float pitch) { m_targetYaw = yaw; m_targetPitch = pitch; }
    void setTargetVelocity(const glm::vec3& vel) { velocity = vel; }
    
    uint32_t getPlayerId() const { return m_playerId; }
    const std::string& getPlayerName() const { return m_playerName; }
    
private:
    uint32_t m_playerId;
    std::string m_playerName;
    
    // Interpolation targets
    glm::vec3 m_targetPosition;
    float m_targetYaw = 0.0f;
    float m_targetPitch = 0.0f;
    
    static constexpr float INTERPOLATION_SPEED = 15.0f;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // Mode management
    bool hostGame(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& playerName);
    bool joinGame(const std::string& host, uint16_t port, const std::string& playerName);
    void disconnect();
    
    NetworkMode getMode() const { return m_mode; }
    bool isOnline() const { return m_mode != NetworkMode::OFFLINE; }
    bool isHost() const { return m_mode == NetworkMode::HOST; }
    bool isClient() const { return m_mode == NetworkMode::CLIENT; }
    
    // Call each frame
    void update(float deltaTime);
    
    // Send local player state (call frequently)
    void sendLocalPlayerState(const glm::vec3& pos, float yaw, float pitch,
                              const glm::vec3& velocity, bool onGround);
    
    // Send block change
    void sendBlockChange(int x, int y, int z, uint8_t blockType);
    
    // Send chat message
    void sendChatMessage(const std::string& message);
    
    // Get remote player entities for rendering
    std::vector<RemotePlayerEntity*> getRemotePlayerEntities();
    
    // Get connection info
    uint32_t getLocalPlayerId() const;
    int64_t getWorldSeed() const;
    glm::vec3 getSpawnPosition() const;
    size_t getPlayerCount() const;
    uint64_t getLatency() const;
    
    // Status
    bool isConnecting() const;
    bool isConnected() const;
    std::string getStatusString() const;
    
    // Callbacks for game integration
    using BlockChangeCallback = std::function<void(int x, int y, int z, uint8_t blockType)>;
    using ChatCallback = std::function<void(const std::string& playerName, const std::string& message)>;
    using DisconnectCallback = std::function<void(const std::string& reason)>;
    using ConnectedCallback = std::function<void()>;
    
    void setBlockChangeCallback(BlockChangeCallback cb) { m_onBlockChange = std::move(cb); }
    void setChatCallback(ChatCallback cb) { m_onChat = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) { m_onDisconnect = std::move(cb); }
    void setConnectedCallback(ConnectedCallback cb) { m_onConnected = std::move(cb); }
    
private:
    void updateRemotePlayerEntities();
    void setupServerCallbacks();
    void setupClientCallbacks();
    
    NetworkMode m_mode = NetworkMode::OFFLINE;
    
    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<GameClient> m_client;
    
    // Remote player entities
    std::vector<std::unique_ptr<RemotePlayerEntity>> m_remotePlayerEntities;
    std::shared_ptr<ModelSystem::Model> m_playerModel;  // Shared model for all remote players
    
    // Host-mode local player info
    std::string m_localPlayerName;
    int64_t m_worldSeed = 0;
    glm::vec3 m_spawnPos{0.0f};
    
    // Position update throttling
    float m_positionUpdateTimer = 0.0f;
    static constexpr float POSITION_UPDATE_INTERVAL = 0.05f;  // 20 Hz
    
    // Callbacks
    BlockChangeCallback m_onBlockChange;
    ChatCallback m_onChat;
    DisconnectCallback m_onDisconnect;
    ConnectedCallback m_onConnected;
};

} // namespace Network
