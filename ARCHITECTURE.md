# Architecture Documentation

This document explains the architecture and design decisions of the Minecraft C++ voxel engine.

## System Overview

The engine is designed as a modular, high-performance voxel renderer with the following key systems:

```
┌─────────────┐
│    Main     │
│ Application │
└──────┬──────┘
       │
       ├──────────────┬──────────────┬──────────────┬──────────────┐
       │              │              │              │              │
   ┌───▼────┐    ┌───▼────┐    ┌───▼────┐    ┌───▼────┐    ┌───▼────┐
   │  Core  │    │  World │    │  Mesh  │    │ Render │    │  Math  │
   │ System │    │ System │    │ System │    │ System │    │ System │
   └────────┘    └────────┘    └────────┘    └────────┘    └────────┘
```

## Core System

### Window (Window.h/cpp)
- Wraps GLFW for window management
- Handles OpenGL context creation
- Provides input callbacks
- Manages cursor modes

### Time (Time.h/cpp)
- Tracks delta time between frames
- Calculates FPS
- Provides timing utilities

### Logger (Logger.h)
- Centralized logging with levels (DEBUG, INFO, WARNING, ERROR)
- Timestamp formatting
- Singleton pattern for global access

### ThreadPool (ThreadPool.h/cpp)
- Fixed-size worker thread pool
- Task queue with futures
- Used for async chunk generation and meshing
- Lock-based synchronization

## World System

### Block (Block.h)
- Enum-based block types (AIR, GRASS, DIRT, STONE, etc.)
- Properties: opacity, solidity
- Material ID for rendering

### Chunk (Chunk.h/cpp)
**Structure:**
- 16x16x16 block grid
- Flat array storage for cache efficiency
- Atomic state machine

**State Machine:**
```
UNLOADED → GENERATING → MESH_BUILD → READY → GPU_UPLOADED
```

**Key Functions:**
- `getBlock()` / `setBlock()` - Block access with bounds checking
- `isBlockOpaque()` - Used for mesh culling

### ChunkManager (ChunkManager.h/cpp)
**Responsibilities:**
- Spatial hash map of chunks (`unordered_map<ChunkPos, shared_ptr<Chunk>>`)
- Load/unload chunks based on camera position
- Spiral generation pattern for smooth loading

**Key Algorithms:**
- `worldToChunk()` - Convert world coordinates to chunk coordinates
- `getChunksToGenerate()` - Priority queue of chunks to generate
- `unloadDistantChunks()` - Remove chunks beyond render distance

### WorldGenerator (WorldGenerator.h/cpp)
**Generation Strategy:**
- 3D Perlin-like noise for terrain height
- Layered generation: stone → dirt → grass
- Sea level with water blocks
- Deterministic based on seed

**Noise Implementation:**
- Custom noise3D function
- Fade curves for smooth interpolation
- Octave layering for detail

## Mesh System

### Vertex (Vertex.h)
**Compact Format (12 bytes):**
```cpp
struct Vertex {
    i16 x, y, z;      // Position (6 bytes)
    u8 normal;        // Packed normal (1 byte)
    u8 material;      // Material ID (1 byte)
    u16 uv;           // Packed UV (2 bytes)
    u16 padding;      // Alignment (2 bytes)
};
```

**Packing Functions:**
- `packNormal()` - 6 directions encoded as 0-5
- `packUV()` - UV coordinates in 8 bits each

### MeshBuilder (MeshBuilder.h/cpp)
**Greedy Meshing Algorithm:**

1. For each direction (±X, ±Y, ±Z):
   - Sweep through slices perpendicular to direction
   - Build 2D mask of visible faces
   - Merge adjacent same-material faces into quads
   - Expand quads horizontally and vertically

**Benefits:**
- Reduces vertex count by ~70%
- Fewer draw calls
- Better GPU cache utilization

**Neighbor Handling:**
- Requires neighbor chunks for boundary faces
- Falls back to conservative culling without neighbors

### Mesh (Mesh.h/cpp)
**OpenGL Resources:**
- VAO (Vertex Array Object)
- VBO (Vertex Buffer Object)
- EBO (Element Buffer Object)

**Vertex Attributes:**
```
Location 0: Position (3 x int16)
Location 1: Normal (1 x uint8)
Location 2: Material (1 x uint8)
Location 3: UV (1 x uint16)
```

## Render System

### Shader (Shader.h/cpp)
**Features:**
- Compile vertex and fragment shaders
- Link shader program
- Uniform setters (int, float, vec3, mat4)
- Error checking and logging

**Shaders:**
- `block.vert` - Transform vertices, unpack attributes
- `block.frag` - Per-material colors, lighting, fog

### Camera (Camera.h/cpp)
**FPS Camera:**
- Position, front, right, up vectors
- Yaw/pitch Euler angles
- WASD movement
- Mouse look with sensitivity

**Matrices:**
- View matrix: `lookAt(position, position + front, up)`
- Projection matrix: Perspective with configurable FOV

### Frustum (Frustum.h/cpp)
**Frustum Culling:**
- Extract 6 planes from view-projection matrix
- AABB vs. frustum test
- Skips off-screen chunks before rendering

**Plane Equation:**
```
ax + by + cz + d = 0
```

### Renderer (Renderer.h/cpp)
**Rendering Pipeline:**
```
1. Clear buffers
2. Update frustum from camera
3. For each chunk:
   - Check state (READY or GPU_UPLOADED)
   - Frustum cull
   - Set uniforms
   - Draw mesh
4. Swap buffers
```

**Optimizations:**
- Single shader for all chunks
- Instanced rendering (one draw call per chunk)
- Early frustum culling

### GPUBufferAllocator (GPUBufferAllocator.h/cpp)
**Persistent Mapped Buffers:**
- OpenGL 4.5+ feature
- Zero-copy GPU uploads
- Ring buffer for frame pipelining

**Fallback:**
- `glBufferData` for older drivers
- Dynamic draw hint

**TODO:** Implement proper frame tracking to prevent data corruption

## Threading Model

### Main Thread
- Input processing
- Rendering
- OpenGL calls (OpenGL is not thread-safe)

### Worker Threads
- Chunk generation (`WorldGenerator::generate()`)
- Mesh building (`MeshBuilder::buildChunkMesh()`)

### Synchronization
- Atomic chunk states
- Task futures for completion tracking
- Lock-free return values

**Flow:**
```
Main Thread                Worker Thread
    |                           |
    |--enqueue(generate)------->|
    |                       Generate
    |                       chunk blocks
    |<------future----------|
    |                           |
    |--enqueue(mesh)----------->|
    |                       Build mesh
    |                       greedy merge
    |<------future----------|
    |                           |
Upload mesh to GPU             |
Mark GPU_UPLOADED               |
```

## Data Flow

### Chunk Generation Pipeline
```
1. Camera moves
2. ChunkManager detects missing chunks
3. Create chunk (UNLOADED state)
4. Enqueue generation task
5. Worker generates blocks (GENERATING)
6. Mark for meshing (MESH_BUILD)
7. Enqueue mesh task
8. Worker builds mesh (READY)
9. Main thread uploads to GPU (GPU_UPLOADED)
10. Renderer draws chunk
```

### Frame Update
```
1. Time::update() - Calculate delta time
2. Process input - Update camera
3. ChunkManager::update() - Load/unload chunks
4. Generate new chunks (background)
5. Build meshes (background)
6. Upload ready meshes to GPU
7. Renderer::render() - Draw visible chunks
8. Swap buffers
```

## Performance Considerations

### Memory Layout
- Chunks use flat arrays for cache coherency
- Vertex format minimized to 12 bytes
- Spatial locality in chunk storage

### CPU Optimization
- Greedy meshing reduces vertices
- Multithreading prevents frame stalls
- Frustum culling skips invisible geometry

### GPU Optimization
- Minimal vertex stride
- Indexed drawing
- Single shader program
- Persistent mapped buffers (when available)

### Future Optimizations
- LOD system for distant chunks
- Occlusion culling
- Chunk batching
- GPU-driven rendering

## Design Patterns

### Singleton
- `Time`, `Logger` - Global utilities

### Factory
- Chunk creation through `ChunkManager`

### State Machine
- Chunk lifecycle management

### Observer
- Callback system in Window

### Object Pool
- TODO: Implement for chunks and meshes

## Testing Recommendations

### Unit Tests
- WorldGenerator noise functions
- ChunkManager coordinate conversions
- MeshBuilder quad merging

### Integration Tests
- Chunk generation pipeline
- Mesh upload workflow
- Camera movement

### Performance Tests
- Chunk generation rate
- FPS under various render distances
- Memory usage over time

## Future Architecture Improvements

1. **Entity Component System (ECS)**
   - Separate data from behavior
   - Better cache utilization
   - More flexible entity management

2. **Render Graph**
   - Automatic dependency tracking
   - Better GPU resource management
   - Multiple render passes

3. **Lock-Free Data Structures**
   - Reduce thread contention
   - Improve throughput
   - Better scaling

4. **GPU Culling**
   - Move frustum culling to GPU
   - Indirect drawing
   - Better utilization

5. **Streaming**
   - Save/load chunks from disk
   - Infinite world support
   - Memory management

## References

- [GPU Gems 2 - Terrain Rendering](https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry)
- [Minecraft Rendering](https://tomcc.github.io/2014/08/31/visibility-1.html)
- [Greedy Meshing](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/)
- [OpenGL Best Practices](https://www.khronos.org/opengl/wiki/Common_Mistakes)
