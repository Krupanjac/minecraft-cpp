#pragma once

#include "../Util/Types.h"
#include <glm/glm.hpp>

// Compact vertex format: 12 bytes total
struct Vertex {
    i16 x, y, z;        // Position (6 bytes)
    u8 normal;          // Packed normal direction (1 byte)
    u8 material;        // Material/Block ID (1 byte)
    u16 uv;             // Packed UV coordinates (2 bytes)
    u16 padding;        // Padding to 12 bytes
    
    Vertex() = default;
    Vertex(i16 x, i16 y, i16 z, u8 normal, u8 material, u16 uv)
        : x(x), y(y), z(z), normal(normal), material(material), uv(uv), padding(0) {}
    
    static u8 packNormal(int nx, int ny, int nz) {
        // Pack normal into 3 bits each (sign + axis)
        if (nx != 0) return nx > 0 ? 0 : 1;
        if (ny != 0) return ny > 0 ? 2 : 3;
        if (nz != 0) return nz > 0 ? 4 : 5;
        return 0;
    }
    
    static u16 packUV(float u, float v) {
        // Pack UV into 16 bits (8 bits each)
        u8 uu = static_cast<u8>(u * 255.0f);
        u8 vv = static_cast<u8>(v * 255.0f);
        return (static_cast<u16>(uu) << 8) | vv;
    }
};

static_assert(sizeof(Vertex) == 12, "Vertex size must be 12 bytes");
