#pragma once

#include "Shader.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class ExplosionVolumeSystem {
public:
    ExplosionVolumeSystem() = default;
    ~ExplosionVolumeSystem();

    void spawn(const glm::vec3& center, float power);
    void spawnFire(const glm::vec3& center, float radius, float duration);
    void update(float deltaTime);
    void render(Shader& shader, const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPos, const glm::vec3& lightDir,
                const glm::vec3& renderOrigin);

    bool hasActiveVolumes() const { return !volumes.empty(); }
    size_t getActiveFireCount() const;

private:
    enum class VolumeType {
        ExplosionFire = 0,
        Smoke = 1,
        BlockFire = 2
    };

    struct ExplosionVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 color;
    };

    struct ExplosionMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        size_t indexCount = 0;

        void upload(const std::vector<ExplosionVertex>& vertices, const std::vector<uint32_t>& indices);
        void draw() const;
        void destroy();
    };

    struct ExplosionVolume {
        glm::vec3 center;
        float power = 4.0f;
        float age = 0.0f;
        float duration = 2.5f;
        float startDelay = 0.0f;
        float radius = 6.0f;
        float isoLevel = 0.0f;
        float rebuildTimer = 0.0f;
        int gridSize = 32;
        float seed = 0.0f;
        ExplosionMesh mesh;
        ExplosionMesh* sharedMesh = nullptr;
        bool meshReady = false;
        bool useSharedMesh = false;
        VolumeType type = VolumeType::ExplosionFire;
    };

    std::vector<ExplosionVolume> volumes;

    ExplosionMesh sharedFireSmall;
    ExplosionMesh sharedFireMedium;
    bool sharedFireSmallReady = false;
    bool sharedFireMediumReady = false;

    void buildMesh(ExplosionVolume& volume);
    void ensureSharedFireMesh(float radius, ExplosionMesh& mesh, bool& readyFlag);
    float sampleDensity(const ExplosionVolume& volume, const glm::vec3& p) const;
    glm::vec3 sampleGradient(const ExplosionVolume& volume, const glm::vec3& p) const;
};
