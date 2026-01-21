#include "VoxelRayTracer.h"
#include "../World/ChunkManager.h"
#include "../World/Chunk.h"
#include "../Core/Logger.h"
#include "../Util/Config.h"
#include <algorithm>

VoxelRayTracer::VoxelRayTracer() {
}

VoxelRayTracer::~VoxelRayTracer() {
    if (lightingTexture) glDeleteTextures(1, &lightingTexture);
    if (voxelTexture) glDeleteTextures(1, &voxelTexture);
}

bool VoxelRayTracer::isSupported() {
    // Check for compute shader support (OpenGL 4.3+)
    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    
    if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 3)) {
        LOG_WARNING("Compute shaders require OpenGL 4.3+. Current: " + 
                    std::to_string(majorVersion) + "." + std::to_string(minorVersion));
        return false;
    }
    
    // Check for image load/store support
    GLint maxComputeWorkGroupCount[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxComputeWorkGroupCount[2]);
    
    LOG_INFO("Compute shader support detected. Max work groups: " + 
             std::to_string(maxComputeWorkGroupCount[0]) + "x" +
             std::to_string(maxComputeWorkGroupCount[1]) + "x" +
             std::to_string(maxComputeWorkGroupCount[2]));
    
    return true;
}

bool VoxelRayTracer::initialize(int screenWidth, int screenHeight) {
    if (!isSupported()) {
        LOG_ERROR("Ray tracing not supported on this hardware");
        return false;
    }
    
    width = screenWidth;
    height = screenHeight;
    
    // Load compute shader
    if (!computeShader.loadComputeShader("shaders/raytracing.comp")) {
        LOG_ERROR("Failed to load ray tracing compute shader");
        return false;
    }
    
    // Create output texture
    createLightingTexture();
    
    // Create voxel 3D texture
    voxelGridSize = voxelGridRadius * 2;
    createVoxelTexture();
    
    initialized = true;
    LOG_INFO("Voxel Ray Tracer initialized. Grid size: " + std::to_string(voxelGridSize));
    
    return true;
}

void VoxelRayTracer::resize(int w, int h) {
    // Store full resolution
    int newWidth = useHalfResolution ? w / 2 : w;
    int newHeight = useHalfResolution ? h / 2 : h;
    
    if (newWidth == width && newHeight == height) return;
    
    width = newWidth;
    height = newHeight;
    createLightingTexture();
}

void VoxelRayTracer::createLightingTexture() {
    if (lightingTexture) {
        glDeleteTextures(1, &lightingTexture);
    }
    
    glGenTextures(1, &lightingTexture);
    glBindTexture(GL_TEXTURE_2D, lightingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VoxelRayTracer::createVoxelTexture() {
    if (voxelTexture) {
        glDeleteTextures(1, &voxelTexture);
    }
    
    glGenTextures(1, &voxelTexture);
    glBindTexture(GL_TEXTURE_3D, voxelTexture);
    
    // Create empty 3D texture (R8UI for block type storage)
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, voxelGridSize, voxelGridSize, voxelGridSize, 
                 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
    
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_3D, 0);
}

void VoxelRayTracer::updateVoxelGrid(ChunkManager& chunkManager, const glm::vec3& playerPos) {
    if (!initialized) return;
    
    // Check if player moved to a new chunk
    glm::ivec3 currentChunk = glm::ivec3(
        static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE)),
        0,
        static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE))
    );
    
    // Only update if player moved to a different chunk area
    if (currentChunk == lastUpdateChunk) return;
    lastUpdateChunk = currentChunk;
    
    // Calculate grid origin (corner of the voxel grid in world space)
    voxelGridOrigin = glm::vec3(
        playerPos.x - voxelGridRadius,
        playerPos.y - voxelGridRadius,
        playerPos.z - voxelGridRadius
    );
    
    // Create CPU buffer for voxel data
    std::vector<uint8_t> voxelData(voxelGridSize * voxelGridSize * voxelGridSize, 0);
    
    // Fill voxel data from chunks
    for (int z = 0; z < voxelGridSize; ++z) {
        for (int y = 0; y < voxelGridSize; ++y) {
            for (int x = 0; x < voxelGridSize; ++x) {
                // World position of this voxel
                int worldX = static_cast<int>(voxelGridOrigin.x) + x;
                int worldY = static_cast<int>(voxelGridOrigin.y) + y;
                int worldZ = static_cast<int>(voxelGridOrigin.z) + z;
                
                // Get block at this position
                Block block = chunkManager.getBlockAt(worldX, worldY, worldZ);
                
                // Store block type (0 = air, >0 = solid)
                uint8_t blockType = static_cast<uint8_t>(block.getType());
                
                // Index into 3D array
                int index = x + y * voxelGridSize + z * voxelGridSize * voxelGridSize;
                voxelData[index] = blockType;
            }
        }
    }
    
    // Upload to GPU
    glBindTexture(GL_TEXTURE_3D, voxelTexture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 
                    voxelGridSize, voxelGridSize, voxelGridSize,
                    GL_RED_INTEGER, GL_UNSIGNED_BYTE, voxelData.data());
    glBindTexture(GL_TEXTURE_3D, 0);
}

GLuint VoxelRayTracer::trace(GLuint depthTexture, const glm::mat4& invViewProj,
                              const glm::vec3& cameraPos, const glm::vec3& lightDir,
                              const glm::vec3& lightColor, const glm::vec3& renderOrigin) {
    if (!initialized) return 0;
    
    computeShader.use();
    
    // Bind output image
    glBindImageTexture(0, lightingTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    
    // Bind input textures
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    computeShader.setInt("depthTexture", 1);
    
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, voxelTexture);
    computeShader.setInt("voxelData", 3);
    
    // Set uniforms
    computeShader.setMat4("invViewProj", invViewProj);
    computeShader.setVec3("cameraPos", cameraPos);
    computeShader.setVec3("lightDir", glm::normalize(lightDir));
    computeShader.setVec3("lightColor", lightColor);
    computeShader.setVec3("voxelGridOrigin", voxelGridOrigin);
    computeShader.setVec3("renderOrigin", renderOrigin);  // Pass render origin for coordinate conversion
    computeShader.setFloat("voxelSize", 1.0f);  // 1 block = 1 unit
    computeShader.setInt("maxRaySteps", maxRaySteps);
    computeShader.setFloat("rayMaxDistance", rayMaxDistance);
    
    // Set grid size
    glm::ivec3 gridSize(voxelGridSize, voxelGridSize, voxelGridSize);
    glUniform3i(glGetUniformLocation(computeShader.getProgram(), "voxelGridSize"), 
                gridSize.x, gridSize.y, gridSize.z);
    
    // Dispatch compute shader
    int groupsX = (width + 15) / 16;
    int groupsY = (height + 15) / 16;
    glDispatchCompute(groupsX, groupsY, 1);
    
    // Memory barrier to ensure writes are visible
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    
    computeShader.unuse();
    
    return lightingTexture;
}
