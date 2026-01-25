#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Network {

// Protocol version for compatibility checks
constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr uint16_t DEFAULT_PORT = 25565;

// Packet types
enum class PacketType : uint8_t {
    // Connection
    CONNECT_REQUEST = 0x01,
    CONNECT_RESPONSE = 0x02,
    DISCONNECT = 0x03,
    PING = 0x04,
    PONG = 0x05,
    
    // Player
    PLAYER_JOIN = 0x10,
    PLAYER_LEAVE = 0x11,
    PLAYER_POSITION = 0x12,
    PLAYER_STATE = 0x13,
    
    // World
    BLOCK_CHANGE = 0x20,
    CHUNK_DATA = 0x21,
    TIME_SYNC = 0x22,
    
    // Chat
    CHAT_MESSAGE = 0x30,
    
    // Server
    SERVER_INFO = 0x40,
    
    // Entity/Mob sync (new)
    ENTITY_SPAWN = 0x50,
    ENTITY_DESPAWN = 0x51,
    ENTITY_UPDATE = 0x52,
    ENTITY_BATCH_UPDATE = 0x53,  // Efficient batch sync
    
    // Player damage/combat
    PLAYER_DAMAGE = 0x60,
};

// Packet header (sent before every packet)
struct PacketHeader {
    uint8_t type;
    uint16_t size;  // Size of payload (excluding header)
};

constexpr size_t PACKET_HEADER_SIZE = 3;
constexpr size_t MAX_PACKET_SIZE = 65535;

// ============== Packet Structures ==============

struct ConnectRequestPacket {
    uint16_t protocolVersion;
    char playerName[32];
    uint8_t modelIndex;  // Player's selected model
    
    static constexpr PacketType TYPE = PacketType::CONNECT_REQUEST;
};

struct ConnectResponsePacket {
    uint8_t accepted;  // 1 = accepted, 0 = rejected
    uint32_t playerId;
    int64_t worldSeed;
    float spawnX, spawnY, spawnZ;
    char rejectReason[64];
    
    static constexpr PacketType TYPE = PacketType::CONNECT_RESPONSE;
};

struct DisconnectPacket {
    char reason[64];
    
    static constexpr PacketType TYPE = PacketType::DISCONNECT;
};

struct PingPacket {
    uint64_t timestamp;
    
    static constexpr PacketType TYPE = PacketType::PING;
};

struct PongPacket {
    uint64_t timestamp;
    
    static constexpr PacketType TYPE = PacketType::PONG;
};

struct PlayerJoinPacket {
    uint32_t playerId;
    char playerName[32];
    float x, y, z;
    float yaw, pitch;
    uint8_t modelIndex;  // Player's selected model
    
    static constexpr PacketType TYPE = PacketType::PLAYER_JOIN;
};

struct PlayerLeavePacket {
    uint32_t playerId;
    
    static constexpr PacketType TYPE = PacketType::PLAYER_LEAVE;
};

struct PlayerPositionPacket {
    uint32_t playerId;
    float x, y, z;
    float yaw, pitch;
    float velocityX, velocityY, velocityZ;
    uint8_t onGround;
    uint8_t heldItemType;  // ItemType enum value for tool/weapon being held
    
    static constexpr PacketType TYPE = PacketType::PLAYER_POSITION;
};

struct PlayerStatePacket {
    uint32_t playerId;
    uint8_t isFlying;
    uint8_t isSprinting;
    uint8_t isSneaking;
    uint8_t animationState;  // Animation state for player sync
    
    // Animation state constants (same as EntityUpdatePacket)
    static constexpr uint8_t ANIM_IDLE = 0;
    static constexpr uint8_t ANIM_WALK = 1;
    static constexpr uint8_t ANIM_RUN = 2;
    static constexpr uint8_t ANIM_ATTACK = 3;
    static constexpr uint8_t ANIM_DEATH = 4;
    static constexpr uint8_t ANIM_HIT_REACT = 5;
    static constexpr uint8_t ANIM_JUMP = 6;
    static constexpr uint8_t ANIM_DUCK = 7;
    static constexpr uint8_t ANIM_WAVE = 8;
    static constexpr uint8_t ANIM_YES = 9;
    static constexpr uint8_t ANIM_NO = 10;
    
    static constexpr PacketType TYPE = PacketType::PLAYER_STATE;
};

struct BlockChangePacket {
    int32_t x, y, z;
    uint8_t blockType;
    
    static constexpr PacketType TYPE = PacketType::BLOCK_CHANGE;
};

struct ChatMessagePacket {
    uint32_t senderId;  // 0 = server
    char message[256];
    
    static constexpr PacketType TYPE = PacketType::CHAT_MESSAGE;
};

struct TimeSyncPacket {
    float timeOfDay;       // Current time of day (0-2400)
    uint8_t isPaused;      // Whether day/night cycle is paused
    
    static constexpr PacketType TYPE = PacketType::TIME_SYNC;
};

// Entity/Mob packets
struct EntitySpawnPacket {
    uint32_t entityId;
    uint8_t mobType;  // MobType enum value
    float x, y, z;
    float yaw;
    
    static constexpr PacketType TYPE = PacketType::ENTITY_SPAWN;
};

struct EntityDespawnPacket {
    uint32_t entityId;
    
    static constexpr PacketType TYPE = PacketType::ENTITY_DESPAWN;
};

struct EntityUpdatePacket {
    uint32_t entityId;
    float x, y, z;
    float velocityX, velocityY, velocityZ;
    float yaw;
    float health;
    uint8_t flags;  // Bit flags: isDead (0x01), isAttacking (0x02), isHitReacting (0x04), isEmoting (0x08)
    uint8_t animationId;  // Animation being played (for network sync)
    
    // Animation ID constants
    static constexpr uint8_t ANIM_IDLE = 0;
    static constexpr uint8_t ANIM_WALK = 1;
    static constexpr uint8_t ANIM_RUN = 2;
    static constexpr uint8_t ANIM_ATTACK = 3;
    static constexpr uint8_t ANIM_DEATH = 4;
    static constexpr uint8_t ANIM_HIT_REACT = 5;
    static constexpr uint8_t ANIM_JUMP = 6;
    static constexpr uint8_t ANIM_IDLE_EATING = 7;
    static constexpr uint8_t ANIM_IDLE_PECK = 8;
    static constexpr uint8_t ANIM_WAVE = 9;
    static constexpr uint8_t ANIM_YES = 10;
    static constexpr uint8_t ANIM_NO = 11;
    static constexpr uint8_t ANIM_DUCK = 12;
    
    // Flag bit masks
    static constexpr uint8_t FLAG_DEAD = 0x01;
    static constexpr uint8_t FLAG_ATTACKING = 0x02;
    static constexpr uint8_t FLAG_HIT_REACTING = 0x04;
    static constexpr uint8_t FLAG_EMOTING = 0x08;
    static constexpr uint8_t FLAG_FLEEING = 0x10;
    
    static constexpr PacketType TYPE = PacketType::ENTITY_UPDATE;
};

// Player damage packet for PvP combat
struct PlayerDamagePacket {
    uint32_t attackerId;    // Player who dealt the damage
    uint32_t targetId;      // Player who received the damage
    float damage;           // Amount of damage dealt
    float knockbackX, knockbackY, knockbackZ;  // Knockback direction and force
    
    static constexpr PacketType TYPE = PacketType::PLAYER_DAMAGE;
};

// ============== Packet Serialization ==============

class PacketBuffer {
public:
    PacketBuffer() = default;
    explicit PacketBuffer(size_t reserve) { m_data.reserve(reserve); }
    
    // Write operations
    void writeU8(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeU64(uint64_t value);
    void writeI32(int32_t value);
    void writeI64(int64_t value);
    void writeFloat(float value);
    void writeString(const std::string& str, size_t maxLen);
    void writeBytes(const void* data, size_t size);
    
    // Read operations
    uint8_t readU8();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();
    int32_t readI32();
    int64_t readI64();
    float readFloat();
    std::string readString(size_t maxLen);
    void readBytes(void* buffer, size_t size);
    
    // Utilities
    const std::vector<uint8_t>& data() const { return m_data; }
    std::vector<uint8_t>& data() { return m_data; }
    size_t size() const { return m_data.size(); }
    size_t readPos() const { return m_readPos; }
    void setReadPos(size_t pos) { m_readPos = pos; }
    void clear() { m_data.clear(); m_readPos = 0; }
    bool hasData() const { return m_readPos < m_data.size(); }
    
    // Write packet with header
    template<typename T>
    void writePacket(const T& packet);
    
private:
    std::vector<uint8_t> m_data;
    size_t m_readPos = 0;
};

// Helper to serialize common packet types
void serializeConnectRequest(PacketBuffer& buffer, const std::string& playerName, uint8_t modelIndex = 0);
void serializeConnectResponse(PacketBuffer& buffer, bool accepted, uint32_t playerId, 
                              int64_t seed, const glm::vec3& spawn, const std::string& reason = "");
void serializePlayerPosition(PacketBuffer& buffer, uint32_t playerId, const glm::vec3& pos,
                             float yaw, float pitch, const glm::vec3& velocity, bool onGround, 
                             uint8_t heldItem = 0);
void serializeBlockChange(PacketBuffer& buffer, int x, int y, int z, uint8_t blockType);
void serializePlayerJoin(PacketBuffer& buffer, uint32_t playerId, const std::string& name,
                         const glm::vec3& pos, float yaw, float pitch, uint8_t modelIndex = 0);
void serializePlayerLeave(PacketBuffer& buffer, uint32_t playerId);
void serializeChatMessage(PacketBuffer& buffer, uint32_t senderId, const std::string& message);
void serializePing(PacketBuffer& buffer, uint64_t timestamp);
void serializePong(PacketBuffer& buffer, uint64_t timestamp);
void serializeDisconnect(PacketBuffer& buffer, const std::string& reason);
void serializeTimeSync(PacketBuffer& buffer, float timeOfDay, bool isPaused);
void serializePlayerDamage(PacketBuffer& buffer, uint32_t attackerId, uint32_t targetId,
                           float damage, const glm::vec3& knockback);

} // namespace Network
