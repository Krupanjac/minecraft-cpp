#include "DebrisEntity.h"
#include "../Physics/BlockPhysics.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// ============================================================================
// DebrisEntity Implementation
// ============================================================================

DebrisEntity::DebrisEntity(const glm::vec3& position,
                           BlockType blockType,
                           const glm::vec3& initialVelocity,
                           const glm::vec3& initialAngularVelocity,
                           float scale)
    : Entity(position)
    , blockType(blockType)
    , debrisScale(scale)
    , linearVelocity(initialVelocity)
    , angularVelocity(initialAngularVelocity)
    , orientationQuat(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , prevLinearVelocity(initialVelocity)
    , lifetime(5.0f)
    , age(0.0f)
    , atRest(false)
    , restFrames(0)
    , modelMatrixDirty(true)
{
    // Set entity scale
    this->scale = glm::vec3(debrisScale);
    
    // Calculate mass from block physics
    const auto& physics = Physics::getBlockPhysics(blockType);
    mass = physics.mass * (debrisScale * debrisScale * debrisScale); // Scale by volume
    
    // Calculate inertia tensor for a uniform cube
    float size = debrisScale;
    float I = (1.0f / 6.0f) * mass * size * size;
    inertiaTensor = glm::mat3(I);
    
    if (mass > 0.0f && I > 0.0f) {
        inverseInertiaTensor = glm::mat3(1.0f / I);
    } else {
        inverseInertiaTensor = glm::mat3(0.0f);
    }
    
    // No model by default - debris uses block rendering
    model = nullptr;
}

void DebrisEntity::update(float deltaTime) {
    // Update age
    age += deltaTime;
    
    // Skip if expired
    if (isExpired()) return;
    
    // Store previous state
    prevPosition = position;
    prevLinearVelocity = linearVelocity;
    
    // Update model matrix if needed
    if (modelMatrixDirty) {
        updateModelMatrix();
    }
}

void DebrisEntity::physicsUpdate(float fixedDeltaTime) {
    if (isExpired() || atRest) return;
    
    // Apply gravity
    applyGravity(fixedDeltaTime);
    
    // Integrate physics
    integratePhysics(fixedDeltaTime);
    
    // Handle terrain collision
    if (config.collideWithTerrain && terrainQuery) {
        handleTerrainCollision(fixedDeltaTime);
    }
    
    // Apply damping
    applyDamping(fixedDeltaTime);
    
    // Check if at rest
    checkRestState();
    
    modelMatrixDirty = true;
}

void DebrisEntity::render(Shader& shader) {
    // Debris rendering is typically handled separately with instancing
    // This is a fallback for individual rendering
    if (isExpired()) return;
    
    // Update model matrix
    updateModelMatrix();
    
    // Set uniforms
    shader.setMat4("model", cachedModelMatrix);
    
    // Set alpha for fade out
    float alpha = getFadeAlpha();
    shader.setFloat("alpha", alpha);
    
    // Block type would be used by the renderer to select texture
    // shader.setInt("blockType", static_cast<int>(blockType));
}

void DebrisEntity::setLinearVelocity(const glm::vec3& vel) {
    linearVelocity = vel;
    atRest = false;
    restFrames = 0;
}

void DebrisEntity::setAngularVelocity(const glm::vec3& angVel) {
    angularVelocity = angVel;
    atRest = false;
    restFrames = 0;
}

void DebrisEntity::applyImpulse(const glm::vec3& impulse) {
    if (mass > 0.0f) {
        linearVelocity += impulse / mass;
        atRest = false;
        restFrames = 0;
    }
}

void DebrisEntity::applyTorqueImpulse(const glm::vec3& torque) {
    angularVelocity += inverseInertiaTensor * torque;
    atRest = false;
    restFrames = 0;
}

float DebrisEntity::getFadeAlpha() const {
    if (age < lifetime - config.fadeTime) {
        return 1.0f;
    }
    float fadeProgress = (age - (lifetime - config.fadeTime)) / config.fadeTime;
    return 1.0f - std::clamp(fadeProgress, 0.0f, 1.0f);
}

Physics::AABB DebrisEntity::getAABB() const {
    float halfSize = debrisScale * 0.5f;
    return Physics::AABB(
        position - glm::vec3(halfSize),
        position + glm::vec3(halfSize)
    );
}

void DebrisEntity::integratePhysics(float dt) {
    // Use substeps for high velocity to prevent tunneling
    float speed = glm::length(linearVelocity);
    float halfSize = debrisScale * 0.5f;
    
    // If moving fast, use multiple substeps
    int substeps = 1;
    if (speed * dt > halfSize * 0.5f) {
        substeps = static_cast<int>(std::ceil(speed * dt / (halfSize * 0.5f)));
        substeps = std::min(substeps, 8); // Cap substeps
    }
    
    float subDt = dt / static_cast<float>(substeps);
    
    for (int i = 0; i < substeps; i++) {
        // Integrate linear velocity
        position += linearVelocity * subDt;
    }
    
    // Integrate angular velocity using quaternion
    if (glm::length(angularVelocity) > 0.001f) {
        float angle = glm::length(angularVelocity) * dt;
        glm::vec3 axis = glm::normalize(angularVelocity);
        glm::quat deltaRot = glm::angleAxis(angle, axis);
        orientationQuat = deltaRot * orientationQuat;
        orientationQuat = glm::normalize(orientationQuat);
    }
}

void DebrisEntity::applyGravity(float dt) {
    const float GRAVITY = -20.0f; // Slightly faster than real for game feel
    linearVelocity.y += GRAVITY * dt;
}

void DebrisEntity::applyDamping(float dt) {
    // Linear damping (air resistance)
    linearVelocity *= (1.0f - config.linearDamping * dt);
    
    // Angular damping
    angularVelocity *= (1.0f - config.angularDamping * dt);
}

void DebrisEntity::handleTerrainCollision(float dt) {
    if (!terrainQuery) return;
    
    float halfSize = debrisScale * 0.5f;
    
    // Multiple collision iterations for better stability
    const int maxIterations = 4;
    for (int iter = 0; iter < maxIterations; iter++) {
        glm::vec3 normal;
        float penetration;
        
        if (checkBoxTerrainCollision(position, halfSize, normal, penetration)) {
            // Resolve penetration immediately
            position += normal * (penetration * 1.05f); // Slight extra push to prevent sticking
            
            // Reflect velocity
            float vDotN = glm::dot(linearVelocity, normal);
            
            if (vDotN < 0.0f) {
                // Bounce with restitution
                float impactSpeed = -vDotN;
                linearVelocity -= (1.0f + config.bounceRestitution) * vDotN * normal;
                
                // Apply friction to tangential velocity
                glm::vec3 tangent = linearVelocity - glm::dot(linearVelocity, normal) * normal;
                float tangentSpeed = glm::length(tangent);
                if (tangentSpeed > 0.01f) {
                    float frictionForce = config.friction * impactSpeed;
                    float frictionReduction = std::min(frictionForce, tangentSpeed);
                    linearVelocity -= (tangent / tangentSpeed) * frictionReduction;
                }
                
                // Reduce angular velocity on collision proportional to impact
                float angularDampFactor = std::max(0.5f, 1.0f - impactSpeed * 0.1f);
                angularVelocity *= angularDampFactor;
                
                // Add some spin from collision based on tangent
                if (tangentSpeed > 0.5f) {
                    glm::vec3 spinAxis = glm::cross(normal, tangent / tangentSpeed);
                    angularVelocity += spinAxis * tangentSpeed * 1.5f;
                }
            }
        } else {
            break; // No collision, done
        }
    }
    
    // Limit maximum velocity to prevent tunneling
    float maxSpeed = 30.0f;
    float speed = glm::length(linearVelocity);
    if (speed > maxSpeed) {
        linearVelocity = (linearVelocity / speed) * maxSpeed;
    }
    
    // Simple ground plane check as fallback (y=0)
    if (position.y < halfSize) {
        position.y = halfSize;
        
        if (linearVelocity.y < 0.0f) {
            linearVelocity.y = -linearVelocity.y * config.bounceRestitution;
            
            // Apply ground friction
            linearVelocity.x *= (1.0f - config.friction * 0.5f);
            linearVelocity.z *= (1.0f - config.friction * 0.5f);
        }
    }
}

bool DebrisEntity::checkBoxTerrainCollision(const glm::vec3& pos, float halfSize, 
                                             glm::vec3& normal, float& penetration) {
    if (!terrainQuery) return false;
    
    // More thorough collision checking - check all 8 corners plus face centers
    const glm::vec3 offsets[] = {
        // Corners
        {-halfSize, -halfSize, -halfSize},
        { halfSize, -halfSize, -halfSize},
        {-halfSize,  halfSize, -halfSize},
        { halfSize,  halfSize, -halfSize},
        {-halfSize, -halfSize,  halfSize},
        { halfSize, -halfSize,  halfSize},
        {-halfSize,  halfSize,  halfSize},
        { halfSize,  halfSize,  halfSize},
        // Face centers for better detection
        { halfSize, 0, 0}, {-halfSize, 0, 0},
        {0,  halfSize, 0}, {0, -halfSize, 0},
        {0, 0,  halfSize}, {0, 0, -halfSize},
        // Center
        {0, 0, 0}
    };
    
    bool collided = false;
    normal = glm::vec3(0.0f);
    penetration = 0.0f;
    int collisionCount = 0;
    
    for (const auto& offset : offsets) {
        glm::vec3 checkPos = pos + offset;
        
        int bx = static_cast<int>(std::floor(checkPos.x));
        int by = static_cast<int>(std::floor(checkPos.y));
        int bz = static_cast<int>(std::floor(checkPos.z));
        
        Block block = terrainQuery(bx, by, bz);
        
        if (block.type != BlockType::AIR && block.type != BlockType::WATER && block.isSolid()) {
            collided = true;
            collisionCount++;
            
            // Calculate AABB overlap for accurate collision response
            // Block AABB: [bx, by, bz] to [bx+1, by+1, bz+1]
            // Debris AABB: [pos - halfSize] to [pos + halfSize]
            
            glm::vec3 blockMin(bx, by, bz);
            glm::vec3 blockMax(bx + 1.0f, by + 1.0f, bz + 1.0f);
            glm::vec3 debrisMin = pos - glm::vec3(halfSize);
            glm::vec3 debrisMax = pos + glm::vec3(halfSize);
            
            // Calculate overlap on each axis
            float overlapX = std::min(debrisMax.x, blockMax.x) - std::max(debrisMin.x, blockMin.x);
            float overlapY = std::min(debrisMax.y, blockMax.y) - std::max(debrisMin.y, blockMin.y);
            float overlapZ = std::min(debrisMax.z, blockMax.z) - std::max(debrisMin.z, blockMin.z);
            
            // Only valid overlap if all axes overlap
            if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
                // Find minimum overlap axis - that's the collision normal direction
                glm::vec3 blockCenter(bx + 0.5f, by + 0.5f, bz + 0.5f);
                glm::vec3 diff = pos - blockCenter;
                
                if (overlapX < overlapY && overlapX < overlapZ) {
                    normal.x += (diff.x > 0) ? 1.0f : -1.0f;
                    penetration = std::max(penetration, overlapX);
                } else if (overlapY < overlapZ) {
                    normal.y += (diff.y > 0) ? 1.0f : -1.0f;
                    penetration = std::max(penetration, overlapY);
                } else {
                    normal.z += (diff.z > 0) ? 1.0f : -1.0f;
                    penetration = std::max(penetration, overlapZ);
                }
            }
        }
    }
    
    if (collided && glm::length(normal) > 0.01f) {
        normal = glm::normalize(normal);
        // Scale penetration correction to prevent debris from sinking into blocks
        penetration += 0.01f; // Small bias to push out of surface
        return true;
    }
    
    return false;
}

void DebrisEntity::checkRestState() {
    float linearSpeed = glm::length(linearVelocity);
    float angularSpeed = glm::length(angularVelocity);
    
    if (linearSpeed < config.minVelocityToRest && angularSpeed < config.minVelocityToRest) {
        restFrames++;
        if (restFrames > 10) { // Require multiple frames at rest
            atRest = true;
            linearVelocity = glm::vec3(0.0f);
            angularVelocity = glm::vec3(0.0f);
        }
    } else {
        restFrames = 0;
    }
}

void DebrisEntity::updateModelMatrix() {
    cachedModelMatrix = glm::translate(glm::mat4(1.0f), position);
    cachedModelMatrix *= glm::mat4_cast(orientationQuat);
    cachedModelMatrix = glm::scale(cachedModelMatrix, glm::vec3(debrisScale));
    modelMatrixDirty = false;
}

// ============================================================================
// DebrisManager Implementation
// ============================================================================

DebrisManager::DebrisManager(size_t maxDebris)
    : maxDebris(maxDebris)
{
}

DebrisEntity* DebrisManager::createDebris(const glm::vec3& position,
                                          BlockType blockType,
                                          const glm::vec3& velocity,
                                          const glm::vec3& angularVelocity,
                                          float scale) {
    // Check if at capacity
    if (activeDebris.size() >= maxDebris) {
        // Remove oldest debris
        if (!activeDebris.empty()) {
            returnToPool(std::move(activeDebris.front()));
            activeDebris.erase(activeDebris.begin());
        }
    }
    
    // Get from pool or create new
    DebrisEntity* debris = getFromPool();
    
    if (debris) {
        // Reinitialize pooled debris
        debris->setPosition(position);
        debris->setLinearVelocity(velocity);
        debris->setAngularVelocity(angularVelocity);
        // Note: For proper pooling, we'd need a reinit method
    } else {
        // Create new
        auto newDebris = std::make_unique<DebrisEntity>(
            position, blockType, velocity, angularVelocity, scale);
        newDebris->setConfig(defaultConfig);
        
        if (terrainQuery) {
            newDebris->setTerrainQuery(terrainQuery);
        }
        
        debris = newDebris.get();
        activeDebris.push_back(std::move(newDebris));
        stats.totalCreated++;
    }
    
    stats.activeDebris = activeDebris.size();
    return debris;
}

void DebrisManager::update(float deltaTime) {
    // Update all active debris
    for (auto& debris : activeDebris) {
        debris->update(deltaTime);
    }
    
    // Remove expired debris
    auto it = std::remove_if(activeDebris.begin(), activeDebris.end(),
        [this](const std::unique_ptr<DebrisEntity>& d) {
            if (d->isExpired()) {
                // Could return to pool here
                return true;
            }
            return false;
        });
    
    activeDebris.erase(it, activeDebris.end());
    stats.activeDebris = activeDebris.size();
}

void DebrisManager::physicsUpdate(float fixedDeltaTime) {
    for (auto& debris : activeDebris) {
        if (!debris->isAtRest() && !debris->isExpired()) {
            debris->physicsUpdate(fixedDeltaTime);
        }
    }
}

void DebrisManager::render(Shader& shader) {
    for (auto& debris : activeDebris) {
        if (!debris->isExpired()) {
            debris->render(shader);
        }
    }
}

void DebrisManager::clear() {
    activeDebris.clear();
    debrisPool.clear();
    stats.activeDebris = 0;
    stats.pooledDebris = 0;
}

DebrisEntity* DebrisManager::getFromPool() {
    if (debrisPool.empty()) {
        return nullptr;
    }
    
    auto debris = std::move(debrisPool.back());
    debrisPool.pop_back();
    
    DebrisEntity* ptr = debris.get();
    activeDebris.push_back(std::move(debris));
    
    stats.pooledDebris = debrisPool.size();
    stats.totalRecycled++;
    
    return ptr;
}

void DebrisManager::returnToPool(std::unique_ptr<DebrisEntity> debris) {
    // Only pool if under limit
    if (debrisPool.size() < maxDebris / 2) {
        debrisPool.push_back(std::move(debris));
        stats.pooledDebris = debrisPool.size();
    }
}
