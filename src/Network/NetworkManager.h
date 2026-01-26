#pragma once

#include "GameServer.h"
#include "GameClient.h"
#include "../Entity/Entity.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>

namespace Network {

enum class NetworkMode {
    OFFLINE,
    HOST,      // Running server + local play
    CLIENT     // Connected to remote server
};

// Remote player entity for rendering
class RemotePlayerEntity : public Entity {
public:
    RemotePlayerEntity(uint32_t playerId, const std::string& name, const glm::vec3& pos, uint8_t modelIndex = 0);
    ~RemotePlayerEntity() override = default;
    
    void update(float deltaTime) override;
    
    void setTargetPosition(const glm::vec3& pos) { m_targetPosition = pos; }
    void setTargetRotation(float yaw, float pitch) { m_targetYaw = yaw; m_targetPitch = pitch; }
    void setTargetVelocity(const glm::vec3& vel) { velocity = vel; }
    void setHeldItem(uint8_t item);
    
    uint32_t getPlayerId() const { return m_playerId; }
    const std::string& getPlayerName() const { return m_playerName; }
    uint8_t getModelIndex() const { return m_modelIndex; }
    uint8_t getHeldItem() const { return m_heldItem; }
    
    // Get the global transform of the right hand bone (for attaching held items)
    glm::mat4 getRightHandTransform() const;
    
    // Check if the model supports Hold animations
    bool supportsHoldAnimations() const { return m_hasHoldAnimations; }
    
    // Initialize animations after model is set
    void initializeAnimations();
    
    // Attack animation control
    void playAttackAnimation();
    bool isPlayingAttackAnimation() const { return m_isAttacking; }
    
    // Hit receive animation control (for showing damage taken)
    void playHitReceiveAnimation();
    bool isPlayingHitReceiveAnimation() const { return m_isHitReacting; }
    
    // Death animation control  
    void playDeathAnimation();
    bool isPlayingDeathAnimation() const { return m_isDead; }
    
    // Damage handling for network (override to track visual health for this remote player)
    void applyNetworkDamage(float damage, const glm::vec3& knockback);
    float getNetworkHealth() const { return m_networkHealth; }
    
private:
    uint32_t m_playerId;
    std::string m_playerName;
    uint8_t m_modelIndex;
    uint8_t m_heldItem = 0;
    
    // Interpolation targets
    glm::vec3 m_targetPosition;
    float m_targetYaw = 0.0f;
    float m_targetPitch = 0.0f;
    
    static constexpr float INTERPOLATION_SPEED = 15.0f;
    
    // Animation names
    std::string m_idleAnim = "Idle";
    std::string m_walkAnim = "Walk";
    std::string m_runAnim = "Run";
    std::string m_idleHoldAnim = "Idle_Hold";
    std::string m_walkHoldAnim = "Walk_Hold";
    std::string m_runHoldAnim = "Run_Hold";
    std::string m_idleAttackAnim = "Idle_Attack";
    std::string m_runAttackAnim = "Run_Attack";
    std::string m_punchAnim = "Punch";
    std::string m_hitReceiveAnim = "HitReceive";
    std::string m_deathAnim = "Death";
    std::string m_rightHandBone = "Fist.R";
    bool m_hasHoldAnimations = false;
    bool m_animationsInitialized = false;
    
    // Attack state
    bool m_isAttacking = false;
    float m_attackAnimTimer = 0.0f;
    
    // Hit receive state
    bool m_isHitReacting = false;
    float m_hitReactTimer = 0.0f;
    
    // Death state
    bool m_isDead = false;
    
    // Health tracking for remote player (for visual purposes)
    float m_networkHealth = 20.0f;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // Mode management
    bool hostGame(uint16_t port, int64_t worldSeed, const glm::vec3& spawnPos, const std::string& playerName, uint8_t modelIndex = 0);
    bool joinGame(const std::string& host, uint16_t port, const std::string& playerName, uint8_t modelIndex = 0);
    void disconnect();
    
    NetworkMode getMode() const { return m_mode; }
    bool isOnline() const { return m_mode != NetworkMode::OFFLINE; }
    bool isHost() const { return m_mode == NetworkMode::HOST; }
    bool isClient() const { return m_mode == NetworkMode::CLIENT; }
    
    // Call each frame
    void update(float deltaTime);
    
    // Send local player state (call frequently)
    void sendLocalPlayerState(const glm::vec3& pos, float yaw, float pitch,
                              const glm::vec3& velocity, bool onGround, uint8_t heldItem = 0);
    
    // Send block change
    void sendBlockChange(int x, int y, int z, uint8_t blockType);
    
    // Send chat message
    void sendChatMessage(const std::string& message);
    
    // Time synchronization (server-authoritative)
    void sendTimeSync(float timeOfDay, bool isPaused);
    
    // Entity sync (host broadcasts to clients)
    void broadcastEntitySpawn(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw);
    void broadcastEntityDespawn(uint32_t entityId);
    void broadcastEntityUpdate(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags);
    
    // Player damage for PvP combat
    void sendPlayerDamage(uint32_t targetPlayerId, float damage, const glm::vec3& knockback);
    
    // Player animation sync (for attack, emotes, etc.)
    void sendPlayerAnimation(uint8_t animationType);
    
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
    using TimeSyncCallback = std::function<void(float timeOfDay, bool isPaused)>;
    using EntitySpawnCallback = std::function<void(uint32_t entityId, uint8_t mobType, const glm::vec3& pos, float yaw)>;
    using EntityDespawnCallback = std::function<void(uint32_t entityId)>;
    using EntityUpdateCallback = std::function<void(uint32_t entityId, const glm::vec3& pos, const glm::vec3& vel, float yaw, float health, uint8_t flags)>;
    using PlayerJoinCallback = std::function<void(uint32_t playerId, const std::string& name)>;
    using PlayerDamageCallback = std::function<void(uint32_t attackerId, uint32_t targetId, float damage, const glm::vec3& knockback)>;
    
    void setBlockChangeCallback(BlockChangeCallback cb) { m_onBlockChange = std::move(cb); }
    void setChatCallback(ChatCallback cb) { m_onChat = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) { m_onDisconnect = std::move(cb); }
    void setConnectedCallback(ConnectedCallback cb) { m_onConnected = std::move(cb); }
    void setTimeSyncCallback(TimeSyncCallback cb) { m_onTimeSync = std::move(cb); }
    void setEntitySpawnCallback(EntitySpawnCallback cb) { m_onEntitySpawn = std::move(cb); }
    void setEntityDespawnCallback(EntityDespawnCallback cb) { m_onEntityDespawn = std::move(cb); }
    void setEntityUpdateCallback(EntityUpdateCallback cb) { m_onEntityUpdate = std::move(cb); }
    void setPlayerJoinCallback(PlayerJoinCallback cb) { m_onPlayerJoin = std::move(cb); }
    void setPlayerDamageCallback(PlayerDamageCallback cb) { m_onPlayerDamage = std::move(cb); }
    
private:
    void updateRemotePlayerEntities();
    void setupServerCallbacks();
    void setupClientCallbacks();
    
    NetworkMode m_mode = NetworkMode::OFFLINE;
    
    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<GameClient> m_client;
    
    // Remote player entities
    std::vector<std::unique_ptr<RemotePlayerEntity>> m_remotePlayerEntities;
    std::map<int, std::shared_ptr<ModelSystem::Model>> m_playerModels;  // Model cache by index
    
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
    TimeSyncCallback m_onTimeSync;
    EntitySpawnCallback m_onEntitySpawn;
    EntityDespawnCallback m_onEntityDespawn;
    EntityUpdateCallback m_onEntityUpdate;
    PlayerJoinCallback m_onPlayerJoin;
    PlayerDamageCallback m_onPlayerDamage;
};

} // namespace Network
