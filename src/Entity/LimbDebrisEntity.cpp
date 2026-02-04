#include "LimbDebrisEntity.h"
#include "../Core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Static configuration
LimbDebrisEntity::Config LimbDebrisEntity::config;

LimbDebrisEntity::LimbDebrisEntity(const glm::vec3& position,
                                   std::shared_ptr<ModelSystem::Model> sourceModel,
                                   int limbNodeIndex,
                                   const std::string& limbName,
                                   const glm::vec3& initialVelocity,
                                   const glm::vec3& initialAngularVelocity,
                                   float scale)
    : limbNodeIndex(limbNodeIndex)
    , limbName(limbName)
    , linearVelocity(initialVelocity)
    , angularVelocity(initialAngularVelocity)
    , orientationQuat(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , debrisScale(scale)
    , lifetime(config.lifetime)
{
    this->position = position;
    this->scale = glm::vec3(scale);
    
    setupLimbModel(sourceModel, limbNodeIndex);
    
    LOG_INFO("[LimbDebris] Created limb debris: " + limbName + " at " + 
             std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z));
}

void LimbDebrisEntity::setupLimbModel(std::shared_ptr<ModelSystem::Model> sourceModel, int nodeIndex) {
    if (!sourceModel) {
        LOG_ERROR("[LimbDebris] No source model provided");
        return;
    }
    
    LOG_INFO("[LimbDebris] setupLimbModel called, cloning model...");
    
    // Clone the model so we have independent state
    limbModel = sourceModel->clone();
    
    if (!limbModel) {
        LOG_ERROR("[LimbDebris] Failed to clone model");
        return;
    }
    
    LOG_INFO("[LimbDebris] Model cloned successfully");
    
    // Clear any hidden state from the clone
    limbModel->clearAllHiddenNodes();
    
    // Get all node names
    const auto& nodeNames = limbModel->getNodeNames();
    
    LOG_INFO("[LimbDebris] Setting up limb model with " + std::to_string(nodeNames.size()) + " nodes, limb node=" + std::to_string(nodeIndex));
    
    // For skinned meshes, we need to HIDE all joints EXCEPT the limb and its descendants
    // This zeroes those joint matrices, making vertices weighted to them invisible
    
    // First, find all nodes that are part of this limb (the node + all its descendants)
    std::vector<bool> isPartOfLimb(nodeNames.size(), false);
    
    // Mark the limb node itself
    if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < nodeNames.size()) {
        isPartOfLimb[nodeIndex] = true;
        LOG_INFO("[LimbDebris] Marked limb node " + std::to_string(nodeIndex) + " (" + nodeNames[nodeIndex] + ")");
    }
    
    // Find all descendants by checking parent relationships iteratively
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < nodeNames.size(); ++i) {
            if (isPartOfLimb[i]) continue;
            
            int parentIdx = limbModel->getParentIndex(static_cast<int>(i));
            if (parentIdx >= 0 && static_cast<size_t>(parentIdx) < nodeNames.size() && isPartOfLimb[parentIdx]) {
                isPartOfLimb[i] = true;
                changed = true;
                LOG_INFO("[LimbDebris] Marked descendant node " + std::to_string(i) + " (" + nodeNames[i] + ")");
            }
        }
    }
    
    // Count how many nodes are part of limb vs hidden (joints only)
    int limbCount = 0;
    int hiddenJointCount = 0;
    int jointCount = 0;

    // Build a fast lookup of which nodes are joints in the active skin
    const auto& joints = limbModel->getActiveSkinJoints();
    std::vector<bool> isJoint(nodeNames.size(), false);
    for (int j : joints) {
        if (j >= 0 && static_cast<size_t>(j) < isJoint.size()) {
            isJoint[j] = true;
            jointCount++;
        }
    }
    LOG_INFO("[LimbDebris] Active skin joints=" + std::to_string(jointCount));

    // Hide ONLY non-limb JOINT nodes.
    // Keep non-joint mesh nodes visible so the skinned mesh can still render.
    for (size_t i = 0; i < nodeNames.size(); ++i) {
        if (!isPartOfLimb[i]) {
            if (isJoint[i]) {
                limbModel->setNodeHidden(static_cast<int>(i), true);
                hiddenJointCount++;
            }
        } else {
            limbCount++;
        }
    }

    LOG_INFO("[LimbDebris] Limb setup complete: " + std::to_string(limbCount) + " limb nodes, " + std::to_string(hiddenJointCount) + " hidden joints");
}

void LimbDebrisEntity::update(float deltaTime) {
    age += deltaTime;
    
    if (isExpired()) return;
    
    // Update model animation (keeps joint matrices updated)
    if (limbModel) {
        limbModel->updateAnimation(deltaTime);
    }
}

void LimbDebrisEntity::physicsUpdate(float fixedDeltaTime) {
    if (isExpired()) return;

    // Apply gravity
    linearVelocity.y -= config.gravity * fixedDeltaTime;

    // Apply damping
    linearVelocity *= (1.0f - config.linearDamping * fixedDeltaTime);
    angularVelocity *= (1.0f - config.angularDamping * fixedDeltaTime);

    // Update position
    position += linearVelocity * fixedDeltaTime;

    // Update rotation
    if (glm::length(angularVelocity) > 0.001f) {
        float angle = glm::length(angularVelocity) * fixedDeltaTime;
        glm::vec3 axis = glm::normalize(angularVelocity);
        glm::quat deltaRot = glm::angleAxis(angle, axis);
        orientationQuat = deltaRot * orientationQuat;
        orientationQuat = glm::normalize(orientationQuat);
    }

    // Terrain collision
    float radius = debrisScale * 0.5f;
    bool hasGround = false;
    float groundLevel = 0.0f;
    if (terrainQuery) {
        int bx = static_cast<int>(std::floor(position.x));
        int by = static_cast<int>(std::floor(position.y - radius - 0.05f));
        int bz = static_cast<int>(std::floor(position.z));
        Block below = terrainQuery(bx, by, bz);
        if (below.isSolid()) {
            groundLevel = static_cast<float>(by + 1);
            hasGround = true;
        }
    }
    if (hasGround && position.y < groundLevel + radius) {
        position.y = groundLevel + radius;
        // Bounce
        if (linearVelocity.y < 0) {
            linearVelocity.y = -linearVelocity.y * config.bounceRestitution;
            linearVelocity.x *= config.friction;
            linearVelocity.z *= config.friction;
            angularVelocity *= 0.8f;
        }
    }
}

void LimbDebrisEntity::render(Shader& shader) {
    renderWithOrigin(shader, glm::vec3(0.0f));
}

void LimbDebrisEntity::renderWithOrigin(Shader& shader, const glm::vec3& renderOrigin) {
    static int earlyExitLog = 0;
    
    if (!limbModel) {
        if (earlyExitLog < 5) {
            LOG_ERROR("[LimbDebris] render() - limbModel is NULL!");
            earlyExitLog++;
        }
        return;
    }
    
    if (isExpired()) {
        return;
    }
    
    float alpha = getFadeAlpha();
    if (alpha <= 0.0f) return;
    
    static int renderLogCount = 0;
    if (renderLogCount < 20) {
        LOG_INFO("[LimbDebris] Rendering " + limbName + " at " + 
                 std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) +
                 " scale=" + std::to_string(scale.x));
        renderLogCount++;
    }
    
    // Build model matrix with camera-relative position, rotation, and scale
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 relPos = position - renderOrigin;
    modelMatrix = glm::translate(modelMatrix, relPos);
    modelMatrix = modelMatrix * glm::mat4_cast(orientationQuat);
    modelMatrix = glm::scale(modelMatrix, scale);
    
    // For motion vectors, use same matrix (debris doesn't need accurate motion vectors)
    limbModel->draw(shader, modelMatrix, modelMatrix);
}

float LimbDebrisEntity::getFadeAlpha() const {
    float remaining = lifetime - age;
    if (remaining <= 0.0f) return 0.0f;
    if (remaining >= config.fadeTime) return 1.0f;
    return remaining / config.fadeTime;
}
