#include "Protocol.h"
#include <cstring>
#include <algorithm>

namespace Network {

// ============== PacketBuffer Implementation ==============

void PacketBuffer::writeU8(uint8_t value) {
    m_data.push_back(value);
}

void PacketBuffer::writeU16(uint16_t value) {
    m_data.push_back(static_cast<uint8_t>(value >> 8));
    m_data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PacketBuffer::writeU32(uint32_t value) {
    m_data.push_back(static_cast<uint8_t>(value >> 24));
    m_data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    m_data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    m_data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PacketBuffer::writeU64(uint64_t value) {
    writeU32(static_cast<uint32_t>(value >> 32));
    writeU32(static_cast<uint32_t>(value & 0xFFFFFFFF));
}

void PacketBuffer::writeI32(int32_t value) {
    writeU32(static_cast<uint32_t>(value));
}

void PacketBuffer::writeI64(int64_t value) {
    writeU64(static_cast<uint64_t>(value));
}

void PacketBuffer::writeFloat(float value) {
    uint32_t intVal;
    std::memcpy(&intVal, &value, sizeof(float));
    writeU32(intVal);
}

void PacketBuffer::writeString(const std::string& str, size_t maxLen) {
    size_t len = std::min(str.size(), maxLen - 1);
    for (size_t i = 0; i < maxLen; ++i) {
        m_data.push_back(i < len ? static_cast<uint8_t>(str[i]) : 0);
    }
}

void PacketBuffer::writeBytes(const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    m_data.insert(m_data.end(), bytes, bytes + size);
}

uint8_t PacketBuffer::readU8() {
    if (m_readPos >= m_data.size()) return 0;
    return m_data[m_readPos++];
}

uint16_t PacketBuffer::readU16() {
    uint16_t value = static_cast<uint16_t>(readU8()) << 8;
    value |= readU8();
    return value;
}

uint32_t PacketBuffer::readU32() {
    uint32_t value = static_cast<uint32_t>(readU8()) << 24;
    value |= static_cast<uint32_t>(readU8()) << 16;
    value |= static_cast<uint32_t>(readU8()) << 8;
    value |= readU8();
    return value;
}

uint64_t PacketBuffer::readU64() {
    uint64_t high = readU32();
    uint64_t low = readU32();
    return (high << 32) | low;
}

int32_t PacketBuffer::readI32() {
    return static_cast<int32_t>(readU32());
}

int64_t PacketBuffer::readI64() {
    return static_cast<int64_t>(readU64());
}

float PacketBuffer::readFloat() {
    uint32_t intVal = readU32();
    float value;
    std::memcpy(&value, &intVal, sizeof(float));
    return value;
}

std::string PacketBuffer::readString(size_t maxLen) {
    std::string result;
    result.reserve(maxLen);
    for (size_t i = 0; i < maxLen && m_readPos < m_data.size(); ++i) {
        char c = static_cast<char>(m_data[m_readPos++]);
        if (c != 0) result.push_back(c);
    }
    return result;
}

void PacketBuffer::readBytes(void* buffer, size_t size) {
    auto* bytes = static_cast<uint8_t*>(buffer);
    for (size_t i = 0; i < size && m_readPos < m_data.size(); ++i) {
        bytes[i] = m_data[m_readPos++];
    }
}

// ============== Serialization Helpers ==============

void serializeConnectRequest(PacketBuffer& buffer, const std::string& playerName, uint8_t modelIndex) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::CONNECT_REQUEST));
    buffer.writeU16(2 + 32 + 1);  // protocolVersion + playerName + modelIndex
    
    // Payload
    buffer.writeU16(PROTOCOL_VERSION);
    buffer.writeString(playerName, 32);
    buffer.writeU8(modelIndex);
}

void serializeConnectResponse(PacketBuffer& buffer, bool accepted, uint32_t playerId,
                              int64_t seed, const glm::vec3& spawn, const std::string& reason) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::CONNECT_RESPONSE));
    buffer.writeU16(1 + 4 + 8 + 12 + 64);  // accepted + playerId + seed + spawn(xyz) + reason
    
    // Payload
    buffer.writeU8(accepted ? 1 : 0);
    buffer.writeU32(playerId);
    buffer.writeI64(seed);
    buffer.writeFloat(spawn.x);
    buffer.writeFloat(spawn.y);
    buffer.writeFloat(spawn.z);
    buffer.writeString(reason, 64);
}

void serializePlayerPosition(PacketBuffer& buffer, uint32_t playerId, const glm::vec3& pos,
                             float yaw, float pitch, const glm::vec3& velocity, bool onGround,
                             uint8_t heldItem) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::PLAYER_POSITION));
    buffer.writeU16(4 + 12 + 8 + 12 + 1 + 1);  // id + pos + angles + vel + onGround + heldItem
    
    // Payload
    buffer.writeU32(playerId);
    buffer.writeFloat(pos.x);
    buffer.writeFloat(pos.y);
    buffer.writeFloat(pos.z);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    buffer.writeFloat(velocity.x);
    buffer.writeFloat(velocity.y);
    buffer.writeFloat(velocity.z);
    buffer.writeU8(onGround ? 1 : 0);
    buffer.writeU8(heldItem);
}

void serializeBlockChange(PacketBuffer& buffer, int x, int y, int z, uint8_t blockType) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::BLOCK_CHANGE));
    buffer.writeU16(12 + 1);  // xyz + blockType
    
    // Payload
    buffer.writeI32(x);
    buffer.writeI32(y);
    buffer.writeI32(z);
    buffer.writeU8(blockType);
}

void serializePlayerJoin(PacketBuffer& buffer, uint32_t playerId, const std::string& name,
                         const glm::vec3& pos, float yaw, float pitch, uint8_t modelIndex) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::PLAYER_JOIN));
    buffer.writeU16(4 + 32 + 12 + 8 + 1);  // id + name + pos + angles + modelIndex
    
    // Payload
    buffer.writeU32(playerId);
    buffer.writeString(name, 32);
    buffer.writeFloat(pos.x);
    buffer.writeFloat(pos.y);
    buffer.writeFloat(pos.z);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    buffer.writeU8(modelIndex);
}

void serializePlayerLeave(PacketBuffer& buffer, uint32_t playerId) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::PLAYER_LEAVE));
    buffer.writeU16(4);
    
    // Payload
    buffer.writeU32(playerId);
}

void serializeChatMessage(PacketBuffer& buffer, uint32_t senderId, const std::string& message) {
    // Header
    buffer.writeU8(static_cast<uint8_t>(PacketType::CHAT_MESSAGE));
    buffer.writeU16(4 + 256);
    
    // Payload
    buffer.writeU32(senderId);
    buffer.writeString(message, 256);
}

void serializePing(PacketBuffer& buffer, uint64_t timestamp) {
    buffer.writeU8(static_cast<uint8_t>(PacketType::PING));
    buffer.writeU16(8);
    buffer.writeU64(timestamp);
}

void serializePong(PacketBuffer& buffer, uint64_t timestamp) {
    buffer.writeU8(static_cast<uint8_t>(PacketType::PONG));
    buffer.writeU16(8);
    buffer.writeU64(timestamp);
}

void serializeDisconnect(PacketBuffer& buffer, const std::string& reason) {
    buffer.writeU8(static_cast<uint8_t>(PacketType::DISCONNECT));
    buffer.writeU16(64);
    buffer.writeString(reason, 64);
}

void serializeTimeSync(PacketBuffer& buffer, float timeOfDay, bool isPaused) {
    buffer.writeU8(static_cast<uint8_t>(PacketType::TIME_SYNC));
    buffer.writeU16(4 + 1);  // float + bool
    buffer.writeFloat(timeOfDay);
    buffer.writeU8(isPaused ? 1 : 0);
}

} // namespace Network
