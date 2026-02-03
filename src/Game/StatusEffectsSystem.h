#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <functional>

class ChunkManager;
class FireSystem;
class ExplosionVolumeSystem;
class Camera;
class Entity;
struct Block;

class StatusEffectsSystem {
public:
    StatusEffectsSystem(ChunkManager& chunkManager,
                        FireSystem& fireSystem,
                        ExplosionVolumeSystem& explosionVolumes,
                        Camera& camera);

    bool isExposedToSky(const glm::vec3& pos) const;

    void applyDaylightBurn(Entity* entity, float deltaTime);
    void clearDaylightBurnStates();
    void applyFireBurn(Entity* entity, float deltaTime);

    void applyPlayerFireBurn(float deltaTime,
                             float playerHealth,
                             bool isUnderwater,
                             const std::function<void(float, const glm::vec3&, bool)>& playerDamage);

    void applyPlayerFallDamage(float playerHealth,
                               bool isUnderwater,
                               bool isFlying,
                               bool isCreative,
                               const std::function<void(float, const glm::vec3&, bool)>& playerDamage);

    void applyEntityFallDamage(Entity* entity);

private:
    struct BurnState {
        float damageTimer = 0.0f;
        float vfxTimer = 0.0f;
    };

    struct FallState {
        bool falling = false;
        float startY = 0.0f;
    };

    bool isPositionInFire(const glm::vec3& pos) const;
    bool isGroundedAt(const glm::vec3& pos) const;

    ChunkManager& chunkManager;
    FireSystem& fireSystem;
    ExplosionVolumeSystem& explosionVolumes;
    Camera& camera;

    std::unordered_map<const Entity*, BurnState> mobBurnStates;
    std::unordered_map<const Entity*, BurnState> fireBurnStates;
    std::unordered_map<const Entity*, FallState> entityFallStates;
    BurnState playerFireState;
    FallState playerFallState;
};
