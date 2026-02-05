#include "HeldItemRenderer.h"
#include "Camera.h"
#include "../Core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

HeldItemRenderer::HeldItemRenderer() {
    initializeToolOffsets();
}

HeldItemRenderer::~HeldItemRenderer() = default;

bool HeldItemRenderer::initialize() {
    LOG_INFO("HeldItemRenderer initialized");
    return true;
}

void HeldItemRenderer::initializeToolOffsets() {
    // Sword - held pointing forward and up
    m_toolOffsets[ToolCategory::SWORD] = {
        glm::vec3(0.0f, 0.0f, 0.0f),       // position offset
        glm::vec3(0.0f, 0.0f, 0.0f),       // rotation offset
        1.0f                                // scale
    };
    
    // Pickaxe - angled for mining
    m_toolOffsets[ToolCategory::PICKAXE] = {
        glm::vec3(0.05f, 0.0f, -0.1f),
        glm::vec3(-15.0f, 10.0f, 0.0f),
        1.0f
    };
    
    // Axe - similar to pickaxe
    m_toolOffsets[ToolCategory::AXE] = {
        glm::vec3(0.05f, 0.0f, -0.1f),
        glm::vec3(-15.0f, 10.0f, 0.0f),
        1.0f
    };
    
    // Shovel - held more vertically
    m_toolOffsets[ToolCategory::SHOVEL] = {
        glm::vec3(0.0f, -0.05f, 0.0f),
        glm::vec3(-20.0f, 5.0f, 0.0f),
        1.0f
    };
}

void HeldItemRenderer::preloadAllModels() {
    LOG_INFO("Preloading all tool models...");
    
    // Load all tool types
    for (int i = 1; i < static_cast<int>(ItemType::COUNT); ++i) {
        ItemType type = static_cast<ItemType>(i);
        loadToolModel(type);
    }
    
    m_preloadComplete = true;
    LOG_INFO("Tool model preload complete: " + std::to_string(m_modelCache.size()) + " models loaded");
}

std::shared_ptr<ModelSystem::Model> HeldItemRenderer::loadToolModel(ItemType type) {
    // Check cache first
    auto it = m_modelCache.find(type);
    if (it != m_modelCache.end()) {
        return it->second;
    }
    
    // Load new model
    std::string path = ItemRegistry::getModelPath(type);
    if (path.empty()) {
        return nullptr;
    }
    
    try {
        auto model = std::make_shared<ModelSystem::Model>(path);
        m_modelCache[type] = model;
        LOG_INFO("Loaded tool model: " + path);
        return model;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load tool model " + path + ": " + e.what());
        return nullptr;
    }
}

void HeldItemRenderer::setHeldItem(ItemType item) {
    if (m_currentItem != item) {
        m_currentItem = item;
        // Preload the model if not already loaded
        if (item != ItemType::NONE) {
            loadToolModel(item);
        }
    }
}

void HeldItemRenderer::triggerSwing() {
    if (m_swingProgress <= 0.0f) {
        m_swingProgress = 1.0f;
    }
}

void HeldItemRenderer::update(float deltaTime, const glm::vec3& playerVelocity, bool onGround, bool isMoving) {
    // Update swing animation
    if (m_swingProgress > 0.0f) {
        m_swingProgress -= deltaTime * m_swingSpeed;
        if (m_swingProgress < 0.0f) {
            m_swingProgress = 0.0f;
        }
    }
    
    // Update mining animation (continuous swing)
    if (m_isMining && m_swingProgress <= 0.0f) {
        m_miningTimer += deltaTime;
        if (m_miningTimer >= 0.25f) { // Swing every 250ms while mining
            triggerSwing();
            m_miningTimer = 0.0f;
        }
    } else if (!m_isMining) {
        m_miningTimer = 0.0f;
    }
    
    // Update view bobbing
    float horizontalSpeed = glm::length(glm::vec2(playerVelocity.x, playerVelocity.z));
    
    // Target bob intensity based on movement
    if (isMoving && onGround && horizontalSpeed > 0.1f) {
        m_targetBobIntensity = std::min(1.0f, horizontalSpeed / 5.0f);
    } else {
        m_targetBobIntensity = 0.0f;
    }
    
    // Smooth transition
    float bobLerp = 1.0f - std::pow(0.001f, deltaTime);
    m_bobIntensity += (m_targetBobIntensity - m_bobIntensity) * bobLerp;
    
    // Advance bob timer
    if (m_bobIntensity > 0.01f) {
        m_bobTimer += deltaTime * horizontalSpeed * 1.5f;
    }
    
    // Update model animation if exists
    if (m_currentItem != ItemType::NONE) {
        auto model = getCurrentModel();
        if (model) {
            model->updateAnimation(deltaTime);
        }
    }
}

void HeldItemRenderer::renderFirstPerson(Shader& shader, const Camera& camera, int screenWidth, int screenHeight) {
    if (m_currentItem == ItemType::NONE) {
        return;
    }
    
    auto model = getCurrentModel();
    if (!model) {
        return;
    }
    
    // Get tool category for offset adjustments
    ToolCategory category = ItemRegistry::getCategory(m_currentItem);
    ToolPoseOffset offset;
    auto it = m_toolOffsets.find(category);
    if (it != m_toolOffsets.end()) {
        offset = it->second;
    }
    
    // Calculate bob offset
    float bobX = std::sin(m_bobTimer * 2.0f) * 0.02f * m_bobIntensity;
    float bobY = std::abs(std::sin(m_bobTimer * 4.0f)) * 0.03f * m_bobIntensity;
    
    // Calculate swing animation
    // Swing arcs the item down and to the left
    float swingAngle = 0.0f;
    float swingOffsetY = 0.0f;
    float swingOffsetX = 0.0f;
    
    if (m_swingProgress > 0.0f) {
        // Use sine curve for smooth swing
        float t = 1.0f - m_swingProgress;
        float swingCurve = std::sin(t * 3.14159f);
        
        swingAngle = swingCurve * 60.0f;     // Rotate down
        swingOffsetY = -swingCurve * 0.3f;   // Move down
        swingOffsetX = -swingCurve * 0.2f;   // Move left
    }
    
    // Build the model matrix for first-person view
    // We render this in screen space, so use orthographic-like positioning
    
    // Start with base position
    glm::vec3 position = m_basePosition + offset.position;
    position.x += bobX + swingOffsetX;
    position.y += bobY + swingOffsetY;
    
    // Build rotation
    glm::vec3 rotation = m_baseRotation + offset.rotation;
    rotation.x += swingAngle; // Add swing rotation
    
    // Create view-space model matrix (position/rotation relative to camera)
    glm::mat4 viewSpaceModel = glm::mat4(1.0f);
    
    // Position in view space (relative to camera)
    viewSpaceModel = glm::translate(viewSpaceModel, position);
    
    // Apply rotations
    viewSpaceModel = glm::rotate(viewSpaceModel, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    viewSpaceModel = glm::rotate(viewSpaceModel, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    viewSpaceModel = glm::rotate(viewSpaceModel, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Scale
    float scale = m_baseScale * offset.scale;
    viewSpaceModel = glm::scale(viewSpaceModel, glm::vec3(scale));
    
    // For first-person rendering, we need a special projection that doesn't clip close objects
    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    glm::mat4 fpProjection = glm::perspective(glm::radians(70.0f), aspect, 0.01f, 10.0f);
    
    // Convert view-space model to world-space model for proper shadow lookup
    // The shader computes vFragPosLightSpace = uLightSpaceMatrix * uModel * localPos
    // so uModel must be in world space (camera-relative rendering origin space)
    glm::mat4 cameraView = camera.getViewMatrix();
    glm::mat4 invCameraView = glm::inverse(cameraView);
    glm::mat4 worldModel = invCameraView * viewSpaceModel;
    
    // Set shader uniforms
    shader.use();
    shader.setMat4("uView", cameraView);  // Use regular camera view
    shader.setMat4("uProjection", fpProjection);
    shader.setMat4("uModel", worldModel);  // World-space model for shadow lookup
    shader.setMat4("uPrevModel", worldModel); // No motion blur for held item
    
    // Use world-space light direction (the renderer already set this for entities)
    // The uniform uLightDir should already be set from entity rendering pass
    // Just ensure shadows are enabled
    shader.setInt("uUseShadows", 1);
    
    // Clear depth buffer to ensure held item renders on top of everything
    // but can still receive shadows based on world position
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Draw the model
    model->draw(shader, worldModel, worldModel);
}

void HeldItemRenderer::renderThirdPerson(Shader& shader, const glm::mat4& handMatrix) {
    if (m_currentItem == ItemType::NONE) {
        return;
    }
    
    auto model = getCurrentModel();
    if (!model) {
        return;
    }
    
    // For third person, attach to hand bone matrix
    // Apply a scale and rotation adjustment
    glm::mat4 modelMatrix = handMatrix;
    
    // Scale down for hand
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.3f));
    
    // Rotate to point forward from hand
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    
    shader.setMat4("uModel", modelMatrix);
    shader.setMat4("uPrevModel", modelMatrix);
    
    model->draw(shader, modelMatrix, modelMatrix);
}

void HeldItemRenderer::renderThirdPerson(Shader& shader, const Camera& camera,
                                          const glm::vec3& playerPos, float playerYaw, ItemType item,
                                          int screenWidth, int screenHeight) {
    if (item == ItemType::NONE) {
        return;
    }
    
    // Get the model for this item
    auto it = m_modelCache.find(item);
    if (it == m_modelCache.end()) {
        // Try to load it
        auto model = const_cast<HeldItemRenderer*>(this)->loadToolModel(item);
        if (!model) return;
        it = m_modelCache.find(item);
    }
    
    auto model = it->second;
    if (!model) return;
    
    // Calculate hand position based on player position and rotation
    // Player yaw: 0 = looking at +Z, 90 = looking at -X, 180 = looking at -Z, 270 = looking at +X
    // Convert to radians and calculate direction vectors
    float yawRad = glm::radians(playerYaw);
    
    // Forward direction (where player is facing)
    glm::vec3 forwardDir = glm::vec3(-std::sin(yawRad), 0.0f, std::cos(yawRad));
    // Right direction (perpendicular to forward, to player's right side)
    glm::vec3 rightDir = glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad));
    
    // The right hand is roughly:
    // - 0.35 units to the right of center
    // - 0.7 units above feet (hand height)
    // - 0.2 units in front
    glm::vec3 handPos = playerPos;
    handPos.y += 0.7f;  // Hand height
    handPos += rightDir * 0.35f;  // To the right
    handPos += forwardDir * 0.2f;  // In front
    
    // Build model matrix
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, handPos);
    
    // Rotate to face the same direction as the player
    // Yaw rotation around Y axis
    modelMatrix = glm::rotate(modelMatrix, yawRad + glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Tilt the tool down and slightly rotated like being held
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-60.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(15.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Scale appropriately for third person
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.3f));
    
    // Set up shader
    shader.use();
    shader.setMat4("uModel", modelMatrix);
    shader.setMat4("uPrevModel", modelMatrix);
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera.getFov()), 
                                            (float)screenWidth / (float)screenHeight, 
                                            0.1f, 1000.0f);
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setVec3("uViewPos", camera.getPosition());
    shader.setVec3("uCameraPos", camera.getPosition());
    shader.setVec3("uLightDir", glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)));
    shader.setVec4("uBaseColor", glm::vec4(1.0f));
    
    // Enable depth testing AND depth writing so tool properly occludes/is occluded
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);  // Enable depth writing
    
    // Draw the model
    model->draw(shader, modelMatrix, modelMatrix);
}

void HeldItemRenderer::renderThirdPersonWithBone(Shader& shader, const Camera& camera,
                                                  const glm::mat4& handWorldTransform, ItemType item,
                                                  int screenWidth, int screenHeight) {
    if (item == ItemType::NONE) {
        return;
    }
    
    // Get the model for this item
    auto it = m_modelCache.find(item);
    if (it == m_modelCache.end()) {
        // Try to load it
        auto model = const_cast<HeldItemRenderer*>(this)->loadToolModel(item);
        if (!model) return;
        it = m_modelCache.find(item);
    }
    
    auto toolModel = it->second;
    if (!toolModel) return;
    
    // Start with the hand bone's world transform
    glm::mat4 modelMatrix = handWorldTransform;
    
    // Apply local adjustments to position the tool correctly in the hand
    // These offsets are tuned for the Quaternius character models with Fist.R bone
    // The bone transform includes entity scale (0.5f), so the tool needs to be scaled
    // relative to that
    
    // Rotate the tool to align with hand orientation and point FORWARD
    // The tool should point in the direction the player is facing
    // Rotate 90 degrees around Y to turn from right to forward
    modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Turn to face forward
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Tilt down into hand
    
    // Offset to position in palm
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 0.0f, 0.05f));
    
    // Scale appropriately for third person
    // The tool models are quite large, need significant scaling down
    // Note: The scale is relative to the player model scale (0.5 for Quaternius)
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.6f));  // Adjusted for better visibility
    
    // Set up shader
    shader.use();
    shader.setMat4("uModel", modelMatrix);
    shader.setMat4("uPrevModel", modelMatrix);
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera.getFov()), 
                                            static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 
                                            0.1f, 1000.0f);
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setVec3("uViewPos", camera.getPosition());
    shader.setVec3("uCameraPos", camera.getPosition());
    shader.setVec3("uLightDir", glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)));
    shader.setVec4("uBaseColor", glm::vec4(1.0f));
    
    // Enable depth testing AND depth writing so tool properly occludes/is occluded
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);  // Enable depth writing
    
    // Draw the model
    toolModel->draw(shader, modelMatrix, modelMatrix);
}

std::shared_ptr<ModelSystem::Model> HeldItemRenderer::getCurrentModel() const {
    if (m_currentItem == ItemType::NONE) {
        return nullptr;
    }
    
    auto it = m_modelCache.find(m_currentItem);
    if (it != m_modelCache.end()) {
        return it->second;
    }
    
    return nullptr;
}

