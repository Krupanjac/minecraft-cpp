#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
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
    glm::mat4 localTransform = glm::mat4(1.0f);
    std::vector<MeshPrimitive> primitives;
    std::vector<std::unique_ptr<Node>> children;
};

class Model {
public:
    Model(const std::string& path);
    ~Model();

    void draw(Shader& shader, const glm::mat4& modelMatrix);
    void updateAnimation(float deltaTime);

private:
    // Root nodes of the scene
    std::vector<std::unique_ptr<Node>> nodes;
    
    // Loaded textures
    std::vector<std::shared_ptr<Texture>> textures;
    
    // Internal loading helpers
    void loadModel(const std::string& path);
    void drawNode(Node* node, Shader& shader, const glm::mat4& parentTransform);
    
    // Animation state
    float animationTime = 0.0f;
    // We will hold the tinygltf::Model data opaque or in cpp
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
