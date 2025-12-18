#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include "../Render/Shader.h"
#include "../Render/Texture.h"

namespace ModelSystem {

struct MeshPrimitive {
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    int indexCount = 0;
    int indexType = 0x1403; // GL_UNSIGNED_SHORT default
    int mode = 4; // GL_TRIANGLES default
    int materialIndex = -1;
};

struct Node {
    int index = -1;
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;

    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    // Bind pose TRS (for root-motion lock / resets)
    glm::vec3 bindTranslation = glm::vec3(0.0f);
    glm::quat bindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 bindScale = glm::vec3(1.0f);
    glm::mat4 matrix = glm::mat4(1.0f); // If set in GLTF
    bool useTRS = true; 

    // Calculated local transform (updated by animation)
    glm::mat4 localTransform = glm::mat4(1.0f);
    // Calculated global transform
    glm::mat4 globalTransform = glm::mat4(1.0f);

    std::vector<MeshPrimitive> primitives;
};

class Model {
public:
    Model(const std::string& path);
    ~Model();

    void draw(Shader& shader, const glm::mat4& modelMatrix, const glm::mat4& prevModelMatrix);
    void updateAnimation(float deltaTime);
    
    void playAnimation(const std::string& name, bool loop = true);
    void stopAnimation();
    std::string getCurrentAnimation() const { return currentAnimationName; }
    std::vector<std::string> getAnimationNames() const;
    void setAnimationSpeed(float speed) { animationSpeed = speed; }
    float getAnimationSpeed() const { return animationSpeed; }
    // Loop only a fraction of the animation [0..1]. Useful to cut out root-motion drift.
    void setAnimationLoopEndFactor(float factor) { animationLoopEndFactor = factor; }
    float getAnimationLoopEndFactor() const { return animationLoopEndFactor; }
    // Root-motion control: if enabled, lock the skeleton root node's XZ translation to bind pose.
    void setLockRootMotionXZ(bool lock) { lockRootMotionXZ = lock; }
    bool getLockRootMotionXZ() const { return lockRootMotionXZ; }

private:
    // Root nodes of the scene
    std::vector<std::unique_ptr<Node>> nodes;
    
    // Loaded textures
    std::vector<std::shared_ptr<Texture>> textures;
    
    // Internal loading helpers
    void loadModel(const std::string& path);
    void drawNode(Node* node, Shader& shader, const glm::mat4& modelMatrix, const glm::mat4& prevModelMatrix);
    void updateGlobalTransforms(Node* node, const glm::mat4& parentTransform);
    void loadSkins();

    // Mapping from GLTF node index to Node*
    std::vector<Node*> nodeMap; // Resized to total nodes count
    
    // Skinning data
    struct Skin {
        std::string name;
        int skeletonRoot = -1;
        std::vector<int> joints;
        std::vector<glm::mat4> inverseBindMatrices;
    };
    std::vector<Skin> skins;
    int activeSkin = 0;
    std::vector<glm::mat4> jointMatrices; // CPU cache of joints
    std::vector<glm::mat4> prevJointMatrices; // Previous frame joints (for motion vectors)
    std::vector<glm::mat4> prevNodeGlobalTransforms; // Previous frame node globals (for motion vectors)
    
    // Animation state
    int currentAnimation = -1;
    std::string currentAnimationName;
    bool animationLoop = true;
    float animationTime = 0.0f;
    float animationDuration = 0.0f;
    float animationSpeed = 1.0f;
    float animationLoopEndFactor = 1.0f;
    bool lockRootMotionXZ = false;
    int rootMotionNodeIndex = -1;
    
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
