#pragma once

#include "../World/Item.h"
#include "../Model/Model.h"
#include "../Render/Shader.h"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

class Camera;

/**
 * HeldItemRenderer - Renders the item/tool held by the player
 * 
 * Features:
 * - First-person view: Tool appears in bottom-right like Minecraft
 * - Third-person view: Tool attached to player model's hand
 * - View bobbing synchronized with player movement
 * - Attack swing animation
 * - Mining animation (tool swings repeatedly)
 * - Caches loaded models to prevent stutters
 */
class HeldItemRenderer {
public:
    HeldItemRenderer();
    ~HeldItemRenderer();
    
    // Initialize shaders and load default items
    bool initialize();
    
    // Preload all tool models (call during loading screen)
    void preloadAllModels();
    bool isPreloadComplete() const { return m_preloadComplete; }
    
    // Set the currently held item
    void setHeldItem(ItemType item);
    ItemType getHeldItem() const { return m_currentItem; }
    
    // Trigger attack/swing animation
    void triggerSwing();
    bool isSwinging() const { return m_swingProgress > 0.0f; }
    
    // Set mining state (continuous swing while mining)
    void setMining(bool mining) { m_isMining = mining; }
    bool isMining() const { return m_isMining; }
    
    // Update animations
    void update(float deltaTime, const glm::vec3& playerVelocity, bool onGround, bool isMoving);
    
    // Render the held item in first-person view
    // Call this after main world rendering, before UI
    void renderFirstPerson(Shader& shader, const Camera& camera, int screenWidth, int screenHeight);
    
    // Render the held item for third-person/other players using hand matrix
    // Returns the model matrix for attaching to hand bone
    void renderThirdPerson(Shader& shader, const glm::mat4& handMatrix);
    
    // Render held item for a remote player (simplified - positions near their right hand)
    void renderThirdPerson(Shader& shader, const Camera& camera, 
                           const glm::vec3& playerPos, float playerYaw, ItemType item,
                           int screenWidth, int screenHeight);
    
    // NEW: Render held item attached to player's hand bone transform
    // handWorldTransform is the world-space transform of the right hand bone
    // This is the proper way to render weapons that follow player animations
    void renderThirdPersonWithBone(Shader& shader, const Camera& camera,
                                   const glm::mat4& handWorldTransform, ItemType item,
                                   int screenWidth, int screenHeight);
    
    // Get the model for the current item (for third-person rendering)
    std::shared_ptr<ModelSystem::Model> getCurrentModel() const;
    
private:
    // Load a tool model (cached)
    std::shared_ptr<ModelSystem::Model> loadToolModel(ItemType type);
    
    // Animation state
    ItemType m_currentItem = ItemType::NONE;
    float m_swingProgress = 0.0f;     // 0 = no swing, 1 = full swing
    float m_swingSpeed = 6.0f;        // Swing animation speed
    bool m_isMining = false;
    float m_miningTimer = 0.0f;
    
    // View bobbing
    float m_bobTimer = 0.0f;
    float m_bobIntensity = 0.0f;      // 0-1, based on movement
    float m_targetBobIntensity = 0.0f;
    
    // Item sway (subtle movement when looking around)
    glm::vec2 m_itemSway = glm::vec2(0.0f);
    glm::vec2 m_lastCameraRotation = glm::vec2(0.0f);
    
    // Model cache
    std::unordered_map<ItemType, std::shared_ptr<ModelSystem::Model>> m_modelCache;
    bool m_preloadComplete = false;
    
    // Position/rotation offsets for first-person view
    // These are tuned to look like Minecraft's held item positioning
    glm::vec3 m_basePosition = glm::vec3(0.6f, -0.5f, -0.8f);  // Right, down, forward
    glm::vec3 m_baseRotation = glm::vec3(-10.0f, 45.0f, 0.0f); // Pitch, yaw, roll
    float m_baseScale = 0.4f;
    
    // Per-tool position adjustments
    struct ToolPoseOffset {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        float scale = 1.0f;
    };
    std::unordered_map<ToolCategory, ToolPoseOffset> m_toolOffsets;
    
    void initializeToolOffsets();
};

