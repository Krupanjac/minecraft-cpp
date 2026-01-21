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
    
    static constexpr PacketType TYPE = PacketType::PLAYER_POSITION;
};

struct PlayerStatePacket {
    uint32_t playerId;
    uint8_t isFlying;
    uint8_t isSprinting;
    uint8_t isSneaking;
    
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
void serializeConnectRequest(PacketBuffer& buffer, const std::string& playerName);
void serializeConnectResponse(PacketBuffer& buffer, bool accepted, uint32_t playerId, 
                              int64_t seed, const glm::vec3& spawn, const std::string& reason = "");
void serializePlayerPosition(PacketBuffer& buffer, uint32_t playerId, const glm::vec3& pos,
                             float yaw, float pitch, const glm::vec3& velocity, bool onGround);
void serializeBlockChange(PacketBuffer& buffer, int x, int y, int z, uint8_t blockType);
void serializePlayerJoin(PacketBuffer& buffer, uint32_t playerId, const std::string& name,
                         const glm::vec3& pos, float yaw, float pitch);
void serializePlayerLeave(PacketBuffer& buffer, uint32_t playerId);
void serializeChatMessage(PacketBuffer& buffer, uint32_t senderId, const std::string& message);
void serializePing(PacketBuffer& buffer, uint64_t timestamp);
void serializePong(PacketBuffer& buffer, uint64_t timestamp);
void serializeDisconnect(PacketBuffer& buffer, const std::string& reason);
void serializeTimeSync(PacketBuffer& buffer, float timeOfDay, bool isPaused);

} // namespace Network
