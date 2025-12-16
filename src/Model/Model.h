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

    void draw(Shader& shader, const glm::mat4& modelMatrix);
    void updateAnimation(float deltaTime);
    
    void playAnimation(const std::string& name, bool loop = true);
    void stopAnimation();
    std::string getCurrentAnimation() const { return currentAnimationName; }

private:
    // Root nodes of the scene
    std::vector<std::unique_ptr<Node>> nodes;
    
    // Loaded textures
    std::vector<std::shared_ptr<Texture>> textures;
    
    // Internal loading helpers
    void loadModel(const std::string& path);
    void drawNode(Node* node, Shader& shader, const glm::mat4& modelMatrix);
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
    
    // Animation state
    int currentAnimation = -1;
    std::string currentAnimationName;
    bool animationLoop = true;
    float animationTime = 0.0f;
    float animationDuration = 0.0f;
    
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
