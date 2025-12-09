# Building and Running the Minecraft C++ Engine

This guide provides step-by-step instructions for building and running the voxel engine.

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git
sudo apt install libgl1-mesa-dev libglu1-mesa-dev xorg-dev
sudo apt install python3 python3-pip
```

### macOS
```bash
brew install cmake
brew install python3
```

### Windows
- Install Visual Studio 2019 or later with C++ development tools
- Install CMake from https://cmake.org/download/
- Install Python 3 from https://www.python.org/downloads/

## Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/Krupanjac/minecraft-cpp.git
cd minecraft-cpp
```

### 2. Generate GLAD Files
GLAD provides the OpenGL function loader. Run the setup script:

**Linux/macOS:**
```bash
./setup_glad.sh
```

**Windows:**
```cmd
setup_glad.bat
```

**Or manually:**
```bash
pip install glad
python -m glad --generator=c --out-path=external/glad --profile=core --api="gl=4.5"
```

### 3. Build the Project
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**On Windows with Visual Studio:**
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 4. Run the Engine
**Linux/macOS:**
```bash
cd build/bin
./minecraft_cpp
```

**Windows:**
```cmd
cd build\bin\Release
minecraft_cpp.exe
```

## Controls

- **W** - Move forward
- **A** - Move left
- **S** - Move backward
- **D** - Move right
- **Space** - Move up
- **Left Shift** - Move down
- **Mouse** - Look around
- **ESC** - Exit

## Configuration

Edit `src/Util/Config.h` to customize:

```cpp
// Chunk configuration
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 16;

// Rendering configuration
constexpr int RENDER_DISTANCE = 8;  // chunks
constexpr int MAX_CHUNKS_PER_FRAME = 4;
constexpr int MAX_MESHES_PER_FRAME = 4;

// Thread pool configuration
constexpr int THREAD_POOL_SIZE = 4;

// Camera configuration
constexpr float FOV = 70.0f;
constexpr float CAMERA_SPEED = 10.0f;
constexpr float MOUSE_SENSITIVITY = 0.1f;
```

## Troubleshooting

### OpenGL Version Error
If you get "OpenGL 4.5 not supported", your GPU may not support it:
- Check your GPU specifications
- Update your graphics drivers
- Try reducing the OpenGL version requirement in the shaders

### GLAD Generation Failed
If the setup script fails:
1. Visit https://glad.dav1d.de/
2. Configure:
   - Language: C/C++
   - Specification: OpenGL
   - API gl: Version 4.5 (Core)
   - Profile: Core
   - Generate a loader: YES
3. Download and extract to `external/glad/`

### Build Errors
If you encounter build errors:
- Ensure you have C++20 compiler support (GCC 10+, Clang 12+, MSVC 2019+)
- Update CMake to version 3.15 or later
- Clean the build directory: `rm -rf build/*` and rebuild

### Runtime Issues
If the program crashes or doesn't render:
- Check that shaders are in the correct location (`build/bin/shaders/`)
- Verify OpenGL support: `glxinfo | grep "OpenGL version"` (Linux)
- Run with a display: The engine requires a graphical display

## Performance Tips

1. **Render Distance**: Lower `RENDER_DISTANCE` in `Config.h` for better FPS
2. **Thread Count**: Adjust `THREAD_POOL_SIZE` based on your CPU cores
3. **VSync**: The engine uses VSync by default. Disable in `Window.cpp` by changing `glfwSwapInterval(0)`
4. **Chunk Generation**: Adjust `MAX_CHUNKS_PER_FRAME` and `MAX_MESHES_PER_FRAME` for smoother generation

## Known Limitations

Current implementation notes:
- Window callbacks have potential memory leaks (marked with TODOs)
- Ring buffer in GPUBufferAllocator needs frame tracking
- No block interaction (placement/destruction) yet
- No lighting system implemented
- Water rendering is opaque (no transparency)
- No save/load functionality

## Next Steps for Development

After getting the basic engine running, consider:

1. **Fix Memory Issues**: Address callback memory leaks in Window.cpp (Completed)
2. **Implement Block Interaction**: Add ray casting for block selection (Completed)
3. **Add Lighting**: Implement ambient occlusion and sunlight (Completed)
4. **Improve World Gen**: Add biomes, caves, trees
5. **Add Physics**: Player collision detection and gravity (Completed)
6. **Optimize Rendering**: Implement proper LOD system
7. **Save/Load**: Add world serialization

## Contributing

When making changes:
1. Follow the existing code style (C++20, 4-space indentation)
2. Test your changes thoroughly
3. Update documentation as needed
4. Add comments for complex algorithms

## Additional Resources

- [LearnOpenGL](https://learnopengl.com/) - OpenGL tutorials
- [GLM Documentation](https://github.com/g-truc/glm/blob/master/manual.md) - Math library
- [GLFW Documentation](https://www.glfw.org/documentation.html) - Window/input
- [Greedy Meshing](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/) - Voxel meshing algorithm

## License

See LICENSE file for details.
