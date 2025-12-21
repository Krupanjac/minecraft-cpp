# Minecraft C++ - High-Performance Voxel Engine

A Minecraft-like voxel engine built from scratch in C++20 with OpenGL 4.5+. Features include infinite world streaming, chunk-based world management, greedy meshing, multithreaded generation, and frustum culling for optimal performance.
## DAY 3
![img33](https://i.imgur.com/WkcfMg1.png)
![img36](https://i.imgur.com/2QRVBqh.png)
![img34](https://i.imgur.com/RSh5edS.png)
![img35](https://i.imgur.com/w56NlVc.png)
![img32](https://i.imgur.com/voVfhKU.png)
![img31](https://i.imgur.com/jylFVWX.png)
## DAY 2
![imgnew](https://i.imgur.com/p9foGhs.png)
![Imgur](https://i.imgur.com/ZeQHhLO.png)
## DAY 1
![Img1](https://i.imgur.com/ZqBQS3k.png)
![Img2](https://i.imgur.com/TW7kn2q.png)





## Features

- **High Performance**: Designed for maximizing performance
- **Infinite World**: Chunk-based streaming with configurable render distance
- **Greedy Meshing**: Optimized mesh generation reducing vertex count
- **Multithreading**: Asynchronous chunk generation and mesh building
- **Modern OpenGL**: Uses OpenGL 4.5+ with persistent mapped buffers
- **Frustum Culling**: Only renders visible chunks
- **FPS Camera**: Smooth WASD + mouse control with flight mode
- **Block Interaction**: Place and break blocks with raycasting
- **Water Rendering**: Transparent animated water with wave effects
- **Day/Night Cycle**: Dynamic lighting with sunrise/sunset transitions
- **Player Physics**: Gravity, collision detection, and water physics

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.15+
- OpenGL 4.5+ capable GPU

## Dependencies

The project uses:
- **GLFW** - Window and input management
- **GLM** - Mathematics library
- **GLAD** - OpenGL loader

Dependencies are automatically fetched via CMake FetchContent if not found on the system.

## Building

### Step 1: Generate GLAD Files

First, you need to generate GLAD files. Visit https://glad.dav1d.de/ and generate with:
- Language: C/C++
- Specification: OpenGL
- API gl: Version 4.5 (or higher)
- Profile: Core
- Generate a loader: YES

Extract the generated files to:
```
external/glad/
├── include/
│   ├── glad/
│   │   └── glad.h
│   └── KHR/
│       └── khrplatform.h
└── src/
    └── glad.c
```

**Or** use this quick command (requires Python and pip):
```bash
pip install glad
python -m glad --generator=c --out-path=external/glad --profile=core --api="gl=4.5"
```

### Step 2: Build the Project

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Step 3: Run

```bash
./bin/minecraft_cpp
```

## Controls

- **W/A/S/D** - Move forward/left/backward/right
- **Space** - Move up (double-tap to toggle flight mode)
- **Left Shift** - Move down
- **Mouse** - Look around
- **Left Click** - Break block
- **Right Click** - Place block
- **ESC** - Open/close menu
- **Q** - Exit (from menu)

## Project Structure

```
minecraft-cpp/
├── src/
│   ├── Core/           # Core utilities (Window, Time, Logger, ThreadPool)
│   ├── Math/           # Math utilities (Ray)
│   ├── World/          # World system (Block, Chunk, ChunkManager, WorldGenerator)
│   ├── Mesh/           # Mesh system (Vertex, MeshBuilder, Mesh)
│   ├── Render/         # Rendering (Shader, Camera, Renderer, Frustum, GPUBufferAllocator)
│   ├── Util/           # Configuration and types
│   └── main.cpp        # Application entry point
├── shaders/            # GLSL shaders
├── assets/             # Textures and resources
├── external/           # External dependencies (GLAD)
└── CMakeLists.txt
```

## Architecture

### Chunk State Machine
```
UNLOADED → GENERATING → MESH_BUILD → READY → GPU_UPLOADED
```

### Threading Model
- Main thread: Rendering and input
- Worker threads: Chunk generation and mesh building
- Lock-free queues for communication

### Rendering Pipeline
1. Frustum culling removes invisible chunks
2. Visible chunks rendered with instanced drawing
3. Greedy meshing minimizes draw calls
4. Persistent mapped buffers for zero-copy GPU uploads

## Configuration

Edit `src/Util/Config.h` to customize:
- Chunk size and render distance
- Thread pool size
- Camera settings
- World generation parameters

## Performance Notes

- Greedy meshing reduces vertices by ~70%
- Frustum culling skips off-screen chunks
- Multithreading prevents main-thread stalls
- Persistent buffers minimize GPU synchronization

## Future Enhancements

- [x] Block placement/destruction
- [x] Advanced LOD system
- [x] Biomes and caves
- [x] Water rendering with transparency
- [x] Lighting system (Ambient Occlusion)
- [x] Save/load world data
- [x] Player physics and collision
- [ ] Multiplayer support

## License

See LICENSE file for details.

## Acknowledgments

Built as a learning project to explore:
- Modern C++ (C++20)
- OpenGL 4.5+ features
- Game engine architecture
- Voxel rendering techniques
