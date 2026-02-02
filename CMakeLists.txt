cmake_minimum_required(VERSION 3.31)
project(minecraft_cpp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# GLFW
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM not found, using FetchContent")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
    )
    FetchContent_MakeAvailable(glm)
endif()

# stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)
include_directories(${stb_SOURCE_DIR})

# tinygltf
set(TINYGLTF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinygltf)
include_directories(${tinygltf_SOURCE_DIR})

# ImGui (for dedicated server GUI)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# ImGui source files
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# GLAD
set(GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/glad)
if(NOT EXISTS ${GLAD_DIR})
    file(MAKE_DIRECTORY ${GLAD_DIR}/include/glad ${GLAD_DIR}/include/KHR ${GLAD_DIR}/src)
    # We'll generate GLAD files inline or provide instructions
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/Core/Window.cpp
    src/Core/Time.cpp
    src/Core/Logger.cpp
    src/Core/ThreadPool.cpp
    src/Math/Ray.cpp
    src/World/Chunk.cpp
    src/World/ChunkManager.cpp
    src/World/WorldGenerator.cpp
    src/World/WorldSerializer.cpp
    src/World/Item.cpp
    src/Mesh/MeshBuilder.cpp
    src/Mesh/Mesh.cpp
    src/Render/Shader.cpp
    src/Render/Camera.cpp
    src/Render/Renderer.cpp
    src/Render/Texture.cpp
    src/Render/GPUBufferAllocator.cpp
    src/Render/Frustum.cpp
    src/Render/FrameBuffer.cpp
    src/Render/ShadowMap.cpp
    src/Render/PostProcess.cpp
    src/Render/VoxelRayTracer.cpp
    src/Render/HeldItemRenderer.cpp
    src/Render/ResourcePackManager.cpp
    src/Render/ExplosionVolumeSystem.cpp
    src/Network/Socket.cpp
    src/Network/Protocol.cpp
    src/Network/GameServer.cpp
    src/Network/GameClient.cpp
    src/Network/NetworkManager.cpp
    src/Server/DedicatedServer.cpp
    src/UI/UIManager.cpp
    src/UI/Console.cpp
    src/Entity/Entity.cpp
    src/Entity/PlayerEntity.cpp
    src/Entity/EntityManager.cpp
    src/Entity/ZombieEntity.cpp
    src/Entity/PassiveMob.cpp
    src/Entity/PigEntity.cpp
    src/Entity/ChickenEntity.cpp
    src/Entity/SheepEntity.cpp
    src/Entity/SkeletonEntity.cpp
    src/Entity/MobSpawnManager.cpp
    src/Entity/EntityManager.cpp
    src/Entity/DebrisEntity.cpp
    src/Model/Model.cpp
    src/Physics/RigidBody.cpp
    src/Physics/Collider.cpp
    src/Physics/CollisionDetection.cpp
    src/Physics/Broadphase.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/DestructionSystem.cpp
    src/Physics/ExplosionSystem.cpp
    src/Audio/AudioManager.cpp
    src/Audio/stb_vorbis_impl.c
    ${GLAD_DIR}/src/glad.c
)

# Shared sources (used by both game and server)
set(SHARED_SOURCES
    src/Core/Logger.cpp
    src/Network/Socket.cpp
    src/Network/Protocol.cpp
    src/Network/GameServer.cpp
    src/Network/GameClient.cpp
    src/Server/DedicatedServer.cpp
    ${GLAD_DIR}/src/glad.c
)

# Server-specific sources
set(SERVER_SOURCES
    src/Server/ServerMain.cpp
    src/Server/ServerGUI.cpp
    ${SHARED_SOURCES}
    ${IMGUI_SOURCES}
)

# Create main game executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Hide console window in Release builds on Windows (use /SUBSYSTEM:WINDOWS but keep main() entry point)
if(WIN32)
    target_link_options(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
        "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>"
    )
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    glm::glm
)

# Create dedicated server executable
add_executable(minecraft_server ${SERVER_SOURCES})

# Hide console window in Release builds on Windows (use /SUBSYSTEM:WINDOWS but keep main() entry point)
if(WIN32)
    target_link_options(minecraft_server PRIVATE
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
        "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>"
    )
endif()

target_include_directories(minecraft_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(minecraft_server PRIVATE
    OpenGL::GL
    glfw
    glm::glm
)

# Platform-specific settings for server
if(WIN32)
    target_compile_definitions(minecraft_server PRIVATE _CRT_SECURE_NO_WARNINGS SERVER_BUILD)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
    target_link_libraries(minecraft_server PRIVATE ws2_32)
else()
    target_compile_definitions(minecraft_server PRIVATE SERVER_BUILD)
endif()

# Copy assets and shaders to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

add_custom_command(TARGET minecraft_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:minecraft_server>/shaders
)

add_custom_command(TARGET minecraft_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:minecraft_server>/assets
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(minecraft_server PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

message(STATUS "Project configured successfully")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
