#include "BloodSplatterSystem.h"
#include "../Core/Logger.h"
#include "../World/ChunkManager.h"
#include "../World/Block.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <algorithm>

BloodSplatterSystem::BloodSplatterSystem() 
    : m_rng(std::random_device{}()) {
    m_particles.resize(m_maxParticles);
    for (auto& p : m_particles) {
        p.active = false;
    }
}

BloodSplatterSystem::~BloodSplatterSystem() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    if (m_atlasTexture) glDeleteTextures(1, &m_atlasTexture);
}

bool BloodSplatterSystem::initialize() {
    if (m_initialized) return true;
    
    // Load shader
    if (!m_shader.loadFromFiles("shaders/sprite_particle.vert", "shaders/sprite_particle.frag")) {
        LOG_ERROR("Failed to load sprite particle shader");
        return false;
    }
    
    // Load blood atlas
    if (!loadAtlas()) {
        LOG_ERROR("Failed to load blood splatter atlas");
        return false;
    }
    
    // Initialize quad mesh
    initQuadMesh();
    
    m_instanceData.reserve(m_maxParticles);
    m_initialized = true;
    
    LOG_INFO("BloodSplatterSystem initialized");
    return true;
}

bool BloodSplatterSystem::loadAtlas() {
    const char* atlasPath = "assets/particles/predrawn_atlas/blood_impact_6x5.png";
    
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char* data = stbi_load(atlasPath, &width, &height, &channels, 4);
    
    if (!data) {
        LOG_ERROR("Failed to load blood atlas: " + std::string(atlasPath));
        return false;
    }
    
    glGenTextures(1, &m_atlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    stbi_set_flip_vertically_on_load(false);
    
    LOG_INFO("Loaded blood splatter atlas: " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

void BloodSplatterSystem::initQuadMesh() {
    // Simple quad vertices: position (x,y,z) + texcoord (u,v)
    float quadVertices[] = {
        // Position           // TexCoord
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_instanceVBO);
    
    glBindVertexArray(m_quadVAO);
    
    // Quad vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // TexCoord attribute (location 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Instance data buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, m_maxParticles * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);
    
    // Instance attributes
    // aWorldPos (location 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, worldPos));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    // aSize (location 3)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, size));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    
    // aAtlasInfo (location 4)
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, atlasInfo));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);
    
    // aColor (location 5)
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, color));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);
    
    // aRotation (location 6)
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, rotation));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);
    
    glBindVertexArray(0);
}

void BloodSplatterSystem::update(float deltaTime) {
    std::uniform_real_distribution<float> decalChance(0.0f, 1.0f);
    
    for (auto& p : m_particles) {
        if (!p.active) continue;
        
        p.age += deltaTime;
        if (p.age >= p.lifetime) {
            p.active = false;
            continue;
        }
        
        // Store old position for collision check
        glm::vec3 oldPos = p.position;
        
        // Update position with gravity
        p.velocity.y -= 9.8f * deltaTime * 0.5f;  // Half gravity for floaty blood
        p.position += p.velocity * deltaTime;
        
        // Check for collision with world (occasionally spawn decal)
        if (m_chunkManager && decalChance(m_rng) < 0.3f) { // 30% of particles can spawn decals
            glm::vec3 moveDir = p.position - oldPos;
            float moveDist = glm::length(moveDir);
            if (moveDist > 0.001f) {
                auto result = m_chunkManager->rayCast(oldPos, glm::normalize(moveDir), moveDist + 0.1f);
                if (result.hit) {
                    // Spawn decal at hit point
                    glm::vec3 hitPos = glm::vec3(ChunkManager::chunkToWorld(result.chunkPos)) + 
                                       glm::vec3(result.blockPos) + glm::vec3(0.5f) + 
                                       glm::vec3(result.normal) * 0.51f;
                    spawnDecalAtPosition(hitPos, glm::vec3(result.normal));
                    p.active = false; // Particle dies on impact
                    continue;
                }
            }
        }
        
        // Update rotation
        p.rotation += p.rotationSpeed * deltaTime;
        
        // Update animation frame
        p.frame += p.frameRate * deltaTime;
        if (p.frame >= static_cast<float>(m_totalFrames)) {
            p.frame = static_cast<float>(m_totalFrames - 1);  // Stay on last frame
        }
        
        // Fade out near end of life
        float lifeRatio = p.age / p.lifetime;
        if (lifeRatio > 0.7f) {
            float fadeRatio = (lifeRatio - 0.7f) / 0.3f;
            p.color.a = 1.0f - fadeRatio;
        }
    }
    
    // Update decals
    updateDecals(deltaTime);
}

void BloodSplatterSystem::render(const glm::mat4& view, const glm::mat4& projection,
                                  const glm::vec3& cameraRight, const glm::vec3& cameraUp) {
    if (!m_initialized) return;
    
    // Build instance data
    m_instanceData.clear();
    for (const auto& p : m_particles) {
        if (!p.active) continue;
        
        InstanceData inst;
        inst.worldPos = p.position;
        inst.size = p.size;
        inst.atlasInfo = glm::vec4(p.frame, static_cast<float>(m_totalFrames), 
                                   static_cast<float>(m_atlasColumns), static_cast<float>(m_atlasRows));
        inst.color = p.color;
        inst.rotation = p.rotation;
        m_instanceData.push_back(inst);
    }
    
    if (m_instanceData.empty()) return;
    
    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_instanceData.size() * sizeof(InstanceData), m_instanceData.data());
    
    // Render
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    
    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", projection);
    m_shader.setVec3("uCameraRight", cameraRight);
    m_shader.setVec3("uCameraUp", cameraUp);
    m_shader.setInt("uAtlasTexture", 0);
    m_shader.setInt("uAdditiveBlend", 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    
    glBindVertexArray(m_quadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_instanceData.size()));
    glBindVertexArray(0);
    
    // Restore state
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    
    m_shader.unuse();
}

void BloodSplatterSystem::spawnSplatter(const glm::vec3& position, const glm::vec3& direction, 
                                         const glm::vec3& backSplashDir, float intensity) {
    std::uniform_real_distribution<float> angleDist(-0.5f, 0.5f);
    std::uniform_real_distribution<float> speedDist(2.0f, 6.0f);
    std::uniform_real_distribution<float> sizeDist(0.15f, 0.4f);
    std::uniform_real_distribution<float> lifeDist(0.4f, 0.8f);
    std::uniform_real_distribution<float> rotDist(0.0f, 6.28f);
    std::uniform_real_distribution<float> rotSpeedDist(-3.0f, 3.0f);
    std::uniform_real_distribution<float> frameRateDist(25.0f, 40.0f);
    std::uniform_real_distribution<float> backSplashChance(0.0f, 1.0f);
    
    // Spawn multiple particles based on intensity
    int count = static_cast<int>(3 + intensity * 5);
    
    for (int i = 0; i < count; ++i) {
        // Find inactive particle
        Particle* p = nullptr;
        for (auto& particle : m_particles) {
            if (!particle.active) {
                p = &particle;
                break;
            }
        }
        
        if (!p) {
            // Pool full, replace oldest
            float maxAge = 0.0f;
            for (auto& particle : m_particles) {
                if (particle.age > maxAge) {
                    maxAge = particle.age;
                    p = &particle;
                }
            }
        }
        
        if (!p) continue;
        
        // Initialize particle
        p->position = position;
        
        // ~15% chance for backsplash toward player (blood flying back)
        bool isBackSplash = backSplashChance(m_rng) < 0.15f;
        glm::vec3 baseDir;
        if (isBackSplash) {
            // Backsplash - blood flies back toward player, slightly upward
            baseDir = glm::normalize(backSplashDir + glm::vec3(0.0f, 0.5f, 0.0f));
        } else {
            // Main spray - blood goes through entity in hit direction
            baseDir = glm::normalize(direction + glm::vec3(0.0f, 0.3f, 0.0f));
        }
        glm::vec3 spread(angleDist(m_rng), angleDist(m_rng), angleDist(m_rng));
        p->velocity = glm::normalize(baseDir + spread) * speedDist(m_rng) * intensity;
        
        float size = sizeDist(m_rng) * intensity;
        p->size = glm::vec2(size);
        
        // Blood red color with slight variation
        float redVar = 0.8f + 0.2f * (static_cast<float>(m_rng() % 100) / 100.0f);
        p->color = glm::vec4(redVar, 0.0f, 0.0f, 1.0f);
        
        p->rotation = rotDist(m_rng);
        p->rotationSpeed = rotSpeedDist(m_rng);
        p->frame = 0.0f;
        p->frameRate = frameRateDist(m_rng);
        p->lifetime = lifeDist(m_rng);
        p->age = 0.0f;
        p->active = true;
    }
}

void BloodSplatterSystem::clear() {
    for (auto& p : m_particles) {
        p.active = false;
    }
    m_decals.clear();
}

size_t BloodSplatterSystem::getActiveCount() const {
    size_t count = 0;
    for (const auto& p : m_particles) {
        if (p.active) ++count;
    }
    return count;
}

void BloodSplatterSystem::updateDecals(float deltaTime) {
    // Age and remove old decals
    for (auto it = m_decals.begin(); it != m_decals.end(); ) {
        it->age += deltaTime;
        if (it->age >= it->lifetime) {
            it = m_decals.erase(it);
        } else {
            // Fade out near end of life
            float lifeRatio = it->age / it->lifetime;
            if (lifeRatio > 0.8f) {
                float fadeRatio = (lifeRatio - 0.8f) / 0.2f;
                it->alpha = 1.0f - fadeRatio;
            }
            ++it;
        }
    }
}

void BloodSplatterSystem::spawnDecalAtPosition(const glm::vec3& pos, const glm::vec3& normal) {
    if (!m_chunkManager) return;
    
    // Calculate block position
    glm::vec3 inside = pos - normal * 0.02f;
    glm::ivec3 blockPos(
        static_cast<int>(std::floor(inside.x)),
        static_cast<int>(std::floor(inside.y)),
        static_cast<int>(std::floor(inside.z))
    );
    
    // Check block type - don't place blood on vegetation
    Block block = m_chunkManager->getBlockAt(blockPos.x, blockPos.y, blockPos.z);
    if (block.isCrossModel() || block.isLeaves() || block.getType() == BlockType::AIR) {
        return; // Don't place blood on flowers, grass, leaves, or air
    }
    
    if (m_decals.size() >= m_maxDecals) {
        // Remove oldest decal
        float maxAge = 0.0f;
        size_t oldestIdx = 0;
        for (size_t i = 0; i < m_decals.size(); ++i) {
            if (m_decals[i].age > maxAge) {
                maxAge = m_decals[i].age;
                oldestIdx = i;
            }
        }
        m_decals.erase(m_decals.begin() + oldestIdx);
    }
    
    std::uniform_real_distribution<float> sizeDist(0.15f, 0.35f);
    std::uniform_real_distribution<float> rotDist(0.0f, 6.28f);
    std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
    std::uniform_int_distribution<int> patternDist(0, 3);
    
    BloodDecal decal;
    decal.position = pos;
    decal.normal = normal;
    float s = sizeDist(m_rng);
    decal.size = glm::vec2(s, s);
    decal.rotation = rotDist(m_rng);
    decal.color = glm::vec3(0.5f, 0.0f, 0.0f); // Dark blood red
    decal.alpha = 1.0f;
    decal.seed = seedDist(m_rng);
    decal.pattern = patternDist(m_rng);
    decal.age = 0.0f;
    decal.lifetime = 15.0f; // Decals last longer than particles
    decal.attachedToBlock = true;
    decal.blockPos = blockPos;
    
    m_decals.push_back(decal);
}

std::vector<Renderer::BloodDecalRenderData> BloodSplatterSystem::getBloodDecalRenderData() const {
    std::vector<Renderer::BloodDecalRenderData> result;
    result.reserve(m_decals.size());
    
    for (const auto& d : m_decals) {
        Renderer::BloodDecalRenderData out;
        out.position = d.position;
        out.normal = d.normal;
        out.size = d.size;
        out.rotation = d.rotation;
        out.color = d.color;
        out.alpha = d.alpha;
        out.seed = d.seed;
        out.pattern = d.pattern;
        out.attachedToBlock = d.attachedToBlock;
        out.blockPos = d.blockPos;
        result.push_back(out);
    }
    
    return result;
}
