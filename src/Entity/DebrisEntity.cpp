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
    , inWater(false)
    , waterSubmersion(0.0f)
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
    if (isExpired()) return;

    updateWaterState();

    if (inWater && atRest) {
        atRest = false;
        restFrames = 0;
    }
    
    // Check if resting debris lost its support (ground destroyed)
    if (atRest && terrainQuery) {
        float halfSize = debrisScale * 0.5f;
        // Check block below us
        int bx = static_cast<int>(std::floor(position.x));
        int by = static_cast<int>(std::floor(position.y - halfSize - 0.1f));
        int bz = static_cast<int>(std::floor(position.z));
        
        Block below = terrainQuery(bx, by, bz);
        if (below.type == BlockType::AIR || below.type == BlockType::WATER || !below.isSolid()) {
            // No support - wake up!
            atRest = false;
            restFrames = 0;
        }
    }
    
    if (atRest) return;
    
    // Apply gravity
    applyGravity(fixedDeltaTime);

    // Apply water flow drift
    applyWaterFlow(fixedDeltaTime);
    
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
    float buoyancy = config.waterBuoyancy * waterSubmersion;
    float gravityScale = 1.0f - buoyancy;
    gravityScale = std::clamp(gravityScale, -0.6f, 1.0f);
    linearVelocity.y += GRAVITY * gravityScale * dt;

    if (inWater && terrainQuery) {
        int bx = static_cast<int>(std::floor(position.x));
        int by = static_cast<int>(std::floor(position.y));
        int bz = static_cast<int>(std::floor(position.z));
        Block above = terrainQuery(bx, by + 1, bz);

        if (above.type == BlockType::AIR) {
            float targetSubmersion = 0.45f;
            float error = waterSubmersion - targetSubmersion;
            float lift = error * 10.0f;
            linearVelocity.y += lift * dt;

            float damping = 6.0f;
            linearVelocity.y -= linearVelocity.y * damping * dt;
        }

        if (waterSubmersion > 0.2f) {
            linearVelocity.y *= (1.0f - 0.4f * dt);
        }
    }
}

void DebrisEntity::applyDamping(float dt) {
    float linearDamp = config.linearDamping + (config.waterLinearDamping - config.linearDamping) * waterSubmersion;
    float angularDamp = config.angularDamping + (config.waterAngularDamping - config.angularDamping) * waterSubmersion;

    // Linear damping (air/water resistance)
    linearVelocity *= (1.0f - linearDamp * dt);
    
    // Angular damping
    angularVelocity *= (1.0f - angularDamp * dt);
}

void DebrisEntity::applyWaterFlow(float dt) {
    if (!inWater || !terrainQuery || config.waterFlowStrength <= 0.0f) return;

    int bx = static_cast<int>(std::floor(position.x));
    int by = static_cast<int>(std::floor(position.y));
    int bz = static_cast<int>(std::floor(position.z));

    Block center = terrainQuery(bx, by, bz);
    if (center.type != BlockType::WATER) return;

    glm::vec3 flow(0.0f);
    const glm::ivec3 dirs[4] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };

    for (const auto& dir : dirs) {
        Block neighbor = terrainQuery(bx + dir.x, by + dir.y, bz + dir.z);
        if (neighbor.type == BlockType::AIR) {
            flow += glm::vec3(dir);
        } else if (neighbor.type == BlockType::WATER) {
            Block neighborBelow = terrainQuery(bx + dir.x, by - 1, bz + dir.z);
            if (neighborBelow.type == BlockType::AIR) {
                flow += glm::vec3(dir) * 0.5f;
            }
        }
    }

    if (glm::length(flow) > 0.01f) {
        flow = glm::normalize(flow) * (config.waterFlowStrength * waterSubmersion);
        linearVelocity += flow * dt;
    }
}

void DebrisEntity::handleTerrainCollision(float dt) {
    if (!terrainQuery) return;
    
    float halfSize = debrisScale * 0.5f;
    
    // Check each axis separately for cleaner collision response
    // This prevents debris from getting stuck in corners
    
    // Check all 6 directions and find solid blocks
    struct AxisCheck {
        glm::vec3 dir;
        float overlap;
        bool hit;
    };
    
    AxisCheck checks[6] = {
        {{1, 0, 0}, 0, false},   // +X
        {{-1, 0, 0}, 0, false},  // -X
        {{0, 1, 0}, 0, false},   // +Y
        {{0, -1, 0}, 0, false},  // -Y
        {{0, 0, 1}, 0, false},   // +Z
        {{0, 0, -1}, 0, false}   // -Z
    };
    
    // Check each face of the debris box
    for (int i = 0; i < 6; i++) {
        glm::vec3 checkPoint = position + checks[i].dir * halfSize;
        int bx = static_cast<int>(std::floor(checkPoint.x));
        int by = static_cast<int>(std::floor(checkPoint.y));
        int bz = static_cast<int>(std::floor(checkPoint.z));
        
        Block block = terrainQuery(bx, by, bz);
        if (block.type != BlockType::AIR && block.type != BlockType::WATER && block.isSolid()) {
            checks[i].hit = true;
            
            // Calculate penetration depth
            glm::vec3 blockMin(bx, by, bz);
            glm::vec3 blockMax(bx + 1.0f, by + 1.0f, bz + 1.0f);
            
            if (checks[i].dir.x > 0) checks[i].overlap = (position.x + halfSize) - blockMin.x;
            else if (checks[i].dir.x < 0) checks[i].overlap = blockMax.x - (position.x - halfSize);
            else if (checks[i].dir.y > 0) checks[i].overlap = (position.y + halfSize) - blockMin.y;
            else if (checks[i].dir.y < 0) checks[i].overlap = blockMax.y - (position.y - halfSize);
            else if (checks[i].dir.z > 0) checks[i].overlap = (position.z + halfSize) - blockMin.z;
            else if (checks[i].dir.z < 0) checks[i].overlap = blockMax.z - (position.z - halfSize);
        }
    }
    
    // Track if we're touching ground (for angular friction)
    bool onGround = false;
    
    // Resolve collisions - process each hit axis
    for (int i = 0; i < 6; i++) {
        if (!checks[i].hit || checks[i].overlap <= 0) continue;
        
        glm::vec3 normal = -checks[i].dir; // Normal points OUT of the block
        float penetration = checks[i].overlap;
        
        // Check if this is ground contact (normal pointing up)
        if (normal.y > 0.5f) {
            onGround = true;
        }
        
        // Push out of block
        position += normal * (penetration + 0.001f);
        
        // Reflect velocity component along this axis
        float velAlongNormal = glm::dot(linearVelocity, normal);
        
        if (velAlongNormal < 0) {
            // Moving into the block - bounce!
            float impactSpeed = -velAlongNormal;
            float bounceVel = impactSpeed * config.bounceRestitution;
            
            // For walls (X and Z), give extra bounce
            if (std::abs(normal.x) > 0.5f || std::abs(normal.z) > 0.5f) {
                bounceVel *= 1.2f; // 20% extra bounce on walls
            }
            
            // Remove incoming velocity and add bounce
            linearVelocity -= normal * velAlongNormal; // Remove incoming
            linearVelocity += normal * bounceVel;       // Add bounce
            
            // Only add spin on significant impacts (not tiny bounces)
            if (impactSpeed > 2.0f) {
                glm::vec3 tangent = linearVelocity - glm::dot(linearVelocity, normal) * normal;
                float tangentSpeed = glm::length(tangent);
                if (tangentSpeed > 0.5f) {
                    glm::vec3 spinAxis = glm::cross(normal, tangent / tangentSpeed);
                    // Add spin but cap it so it doesn't accumulate infinitely
                    float currentAngSpeed = glm::length(angularVelocity);
                    if (currentAngSpeed < 15.0f) {
                        angularVelocity += spinAxis * std::min(impactSpeed * 0.5f, 5.0f);
                    }
                }
            }
            
            // Dampen angular velocity on impact
            angularVelocity *= 0.8f;
        }
    }
    
    // Apply friction and damping when on ground
    if (onGround) {
        float angSpeed = glm::length(angularVelocity);
        float linSpeed = glm::length(linearVelocity);
        
        // Apply ground friction to horizontal velocity (not vertical)
        float frictionFactor = config.friction * 0.15f; // Strong ground friction
        linearVelocity.x *= (1.0f - frictionFactor);
        linearVelocity.z *= (1.0f - frictionFactor);
        
        // If barely moving horizontally, stop completely
        float horizSpeed = std::sqrt(linearVelocity.x * linearVelocity.x + 
                                      linearVelocity.z * linearVelocity.z);
        if (horizSpeed < 0.1f) {
            linearVelocity.x = 0.0f;
            linearVelocity.z = 0.0f;
        }
        
        // Angular friction - dampen rotation when on ground
        if (linSpeed < 0.5f) {
            angularVelocity *= 0.85f; // Strong damping when nearly stopped
        } else {
            angularVelocity *= 0.92f; // Moderate damping when sliding
        }
        
        // Stop tiny rotations completely
        if (angSpeed < 0.1f) {
            angularVelocity = glm::vec3(0.0f);
        }
    }
    
    // Limit maximum velocities
    float maxSpeed = 30.0f;
    float speed = glm::length(linearVelocity);
    if (speed > maxSpeed) {
        linearVelocity = (linearVelocity / speed) * maxSpeed;
    }
    
    float maxAngSpeed = 20.0f;
    float angSpeed = glm::length(angularVelocity);
    if (angSpeed > maxAngSpeed) {
        angularVelocity = (angularVelocity / angSpeed) * maxAngSpeed;
    }
    
    // Ground plane fallback (y=0)
    if (position.y < halfSize) {
        position.y = halfSize;
        onGround = true;
        if (linearVelocity.y < 0.0f) {
            linearVelocity.y = -linearVelocity.y * config.bounceRestitution;
            linearVelocity.x *= (1.0f - config.friction * 0.5f);
            linearVelocity.z *= (1.0f - config.friction * 0.5f);
            angularVelocity *= 0.85f; // Dampen spin on ground bounce
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
    if (inWater) {
        restFrames = 0;
        atRest = false;
        return;
    }

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

void DebrisEntity::updateWaterState() {
    inWater = false;
    waterSubmersion = 0.0f;

    if (!terrainQuery) return;

    float halfSize = debrisScale * 0.5f;
    const glm::vec3 samples[3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, -halfSize * 0.6f, 0.0f},
        {0.0f, halfSize * 0.6f, 0.0f}
    };

    int waterHits = 0;
    for (const auto& offset : samples) {
        glm::vec3 samplePos = position + offset;
        int bx = static_cast<int>(std::floor(samplePos.x));
        int by = static_cast<int>(std::floor(samplePos.y));
        int bz = static_cast<int>(std::floor(samplePos.z));

        Block block = terrainQuery(bx, by, bz);
        if (block.type == BlockType::WATER) {
            waterHits++;
        }
    }

    waterSubmersion = static_cast<float>(waterHits) / 3.0f;
    inWater = waterSubmersion > 0.0f;
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
        if (!debris->isExpired()) {
            // Always call physicsUpdate - it handles rest state and support checks internally
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
