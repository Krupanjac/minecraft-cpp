#pragma once

#include <glad/glad.h>
#include "../Util/Types.h"
#include <vector>

class GPUBufferAllocator {
public:
    GPUBufferAllocator(size_t size);
    ~GPUBufferAllocator();

    struct Allocation {
        GLuint buffer;
        size_t offset;
        size_t size;
        void* mappedPtr;
    };

    Allocation allocate(size_t size);
    void free(const Allocation& allocation);
    
    bool isPersistentMappingSupported() const { return persistentMapping; }

private:
    GLuint buffer;
    size_t bufferSize;
    size_t currentOffset;
    void* mappedPtr;
    bool persistentMapping;
};
