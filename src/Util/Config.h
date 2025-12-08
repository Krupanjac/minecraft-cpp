#pragma once

#include <cstdint>

// Chunk configuration
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 16;
constexpr int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT;

// Rendering configuration
constexpr int RENDER_DISTANCE = 8;  // chunks
constexpr int MAX_CHUNKS_PER_FRAME = 4;  // chunks to generate per frame
constexpr int MAX_MESHES_PER_FRAME = 4;  // meshes to build per frame

// Thread pool configuration
constexpr int THREAD_POOL_SIZE = 4;

// Camera configuration
constexpr float FOV = 70.0f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float CAMERA_SPEED = 10.0f;
constexpr float MOUSE_SENSITIVITY = 0.1f;

// GPU Buffer configuration
constexpr size_t GPU_BUFFER_SIZE = 256 * 1024 * 1024;  // 256 MB
constexpr size_t RING_BUFFER_FRAMES = 3;

// World generation
constexpr float NOISE_SCALE = 0.01f;
constexpr int TERRAIN_HEIGHT = 64;
constexpr int SEA_LEVEL = 32;
