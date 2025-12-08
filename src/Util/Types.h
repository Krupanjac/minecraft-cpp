#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// Common type aliases
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

// 3D position types
using ChunkPos = glm::ivec3;
using BlockPos = glm::ivec3;
using WorldPos = glm::vec3;

// Hash function for ChunkPos (for unordered_map)
namespace std {
    template<>
    struct hash<ChunkPos> {
        size_t operator()(const ChunkPos& pos) const noexcept {
            // Simple hash combining
            size_t h1 = std::hash<int>{}(pos.x);
            size_t h2 = std::hash<int>{}(pos.y);
            size_t h3 = std::hash<int>{}(pos.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
