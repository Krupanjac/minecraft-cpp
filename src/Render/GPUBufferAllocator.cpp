#include "GPUBufferAllocator.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"

GPUBufferAllocator::GPUBufferAllocator(size_t size)
    : bufferSize(size), currentOffset(0), mappedPtr(nullptr), persistentMapping(false) {
    
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    
    // Try to use persistent mapping (GL 4.4+)
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glBufferStorage(GL_ARRAY_BUFFER, bufferSize, nullptr, flags);
    
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        mappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, flags);
        if (mappedPtr) {
            persistentMapping = true;
            LOG_INFO("GPU Buffer with persistent mapping created: " + std::to_string(bufferSize / 1024 / 1024) + " MB");
        }
    }
    
    if (!persistentMapping) {
        // Fallback to traditional buffer
        glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
        LOG_INFO("GPU Buffer created (fallback): " + std::to_string(bufferSize / 1024 / 1024) + " MB");
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GPUBufferAllocator::~GPUBufferAllocator() {
    if (persistentMapping && mappedPtr) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glDeleteBuffers(1, &buffer);
}

GPUBufferAllocator::Allocation GPUBufferAllocator::allocate(size_t size) {
    Allocation alloc;
    alloc.buffer = buffer;
    alloc.offset = currentOffset;
    alloc.size = size;
    alloc.mappedPtr = nullptr;
    
    if (persistentMapping && mappedPtr) {
        alloc.mappedPtr = static_cast<char*>(mappedPtr) + currentOffset;
    }
    
    currentOffset += size;
    
    // Simple ring buffer wraparound
    // TODO: Implement proper frame tracking to prevent GPU data corruption
    // Should track which frames are in-flight and wait for GPU completion
    if (currentOffset >= bufferSize) {
        currentOffset = 0;
        LOG_WARNING("GPU Buffer ring wraparound");
    }
    
    return alloc;
}

void GPUBufferAllocator::free(const Allocation& allocation) {
    // Simple allocator doesn't support individual frees
    // In production, use a proper memory allocator
}
