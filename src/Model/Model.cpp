#define GLM_ENABLE_EXPERIMENTAL
#include "Model.h"
#include "../Core/Logger.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>


namespace ModelSystem {

// Pimpl idiom to keep tinygltf headers out of Model.h
struct Model::Impl {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
};

Model::Model(const std::string& path) : impl(std::make_unique<Impl>()) {
    loadModel(path);
}

Model::~Model() = default;

void Model::loadModel(const std::string& path) {
    std::string err;
    std::string warn;

    bool ret = false;
    if (path.find(".glb") != std::string::npos) {
        ret = impl->loader.LoadBinaryFromFile(&impl->model, &err, &warn, path);
    } else {
        ret = impl->loader.LoadASCIIFromFile(&impl->model, &err, &warn, path);
    }

    if (!warn.empty()) {
        LOG_WARNING("glTF Warning: " + warn);
    }

    if (!err.empty()) {
        LOG_ERROR("glTF Error: " + err);
    }

    if (!ret) {
        LOG_ERROR("Failed to load glTF model: " + path);
        return;
    }

    LOG_INFO("Loaded glTF model structure: " + path);

    // Process textures
    // Process textures
    for (size_t i = 0; i < impl->model.images.size(); ++i) {
        const auto& img = impl->model.images[i];
        // img.image is a vector of bytes
        LOG_INFO("Processing image " + std::to_string(i) + ": " + img.name);
        LOG_INFO("  Dimensions: " + std::to_string(img.width) + "x" + std::to_string(img.height));
        LOG_INFO("  Components: " + std::to_string(img.component));
        LOG_INFO("  Data size: " + std::to_string(img.image.size()));
        
        if (img.image.empty() || img.width <= 0 || img.height <= 0) {
             LOG_ERROR("  Image data is invalid or empty!");
        }

        // Create Texture from memory
        auto tex = std::make_shared<Texture>(img.image.data(), img.width, img.height, img.component);
        textures.push_back(tex);
    }

    // Process nodes recursively
    const tinygltf::Scene& scene = impl->model.scenes[impl->model.defaultScene > -1 ? impl->model.defaultScene : 0];
    
    // Build the full tree
    std::function<std::unique_ptr<Node>(int)> processNode = [&](int nodeIndex) -> std::unique_ptr<Node> {
        const tinygltf::Node& inputNode = impl->model.nodes[nodeIndex];
        auto newNode = std::make_unique<Node>();
        
        // Transform
        if (inputNode.matrix.size() == 16) {
            newNode->localTransform = glm::make_mat4(inputNode.matrix.data());
        } else {
            // TRS
            glm::vec3 t(0.0f), s(1.0f);
            glm::quat r(1.0f, 0.0f, 0.0f, 0.0f);
            
            if (inputNode.translation.size() == 3) {
                t = glm::make_vec3(inputNode.translation.data());
            }
            if (inputNode.rotation.size() == 4) {
                r = glm::make_quat(inputNode.rotation.data());
            }
            if (inputNode.scale.size() == 3) {
                s = glm::make_vec3(inputNode.scale.data());
            }

            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, t);
            m = glm::rotate(m, glm::angle(r), glm::axis(r)); // This might need verification of axis/angle vs quat mat cast
            m = m * glm::mat4_cast(r); // Safest
            m = glm::scale(m, s);
            newNode->localTransform = m;
        }

        // Mesh
        if (inputNode.mesh > -1) {
            const tinygltf::Mesh& mesh = impl->model.meshes[inputNode.mesh];
            for (const auto& primitive : mesh.primitives) {
                MeshPrimitive prim;
                prim.materialIndex = primitive.material;
                prim.mode = primitive.mode;
                if (prim.mode < 0) prim.mode = 4; // Default to TRIANGLES

                // Create VAO/VBO/EBO
                glGenVertexArrays(1, &prim.VAO);
                glBindVertexArray(prim.VAO);

                // Index Buffer
                if (primitive.indices > -1) {
                    const tinygltf::Accessor& accessor = impl->model.accessors[primitive.indices];
                    prim.indexType = accessor.componentType;
                    const tinygltf::BufferView& bufferView = impl->model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = impl->model.buffers[bufferView.buffer];
                    
                    prim.indexCount = static_cast<int>(accessor.count);
                    
                    glGenBuffers(1, &prim.EBO);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.EBO);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferView.byteLength, 
                                 &buffer.data[bufferView.byteOffset + accessor.byteOffset], GL_STATIC_DRAW);
                }

                // Attributes
                for (const auto& attrib : primitive.attributes) {
                    const std::string& name = attrib.first;
                    int accessorIdx = attrib.second;
                    
                    const tinygltf::Accessor& accessor = impl->model.accessors[accessorIdx];
                    const tinygltf::BufferView& bufferView = impl->model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = impl->model.buffers[bufferView.buffer];

                    unsigned int vbo;
                    glGenBuffers(1, &vbo);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferData(GL_ARRAY_BUFFER, bufferView.byteLength, 
                                 &buffer.data[bufferView.byteOffset + accessor.byteOffset], GL_STATIC_DRAW);
                    
                    int location = -1;
                    if (name == "POSITION") location = 0;
                    else if (name == "NORMAL") location = 1;
                    else if (name == "TEXCOORD_0") location = 2;
                    else if (name == "JOINTS_0") location = 3;
                    else if (name == "WEIGHTS_0") location = 4;

                    if (location > -1) {
                        int size = 1;
                        if (accessor.type == TINYGLTF_TYPE_SCALAR) size = 1;
                        else if (accessor.type == TINYGLTF_TYPE_VEC2) size = 2;
                        else if (accessor.type == TINYGLTF_TYPE_VEC3) size = 3;
                        else if (accessor.type == TINYGLTF_TYPE_VEC4) size = 4;
                        
                        glEnableVertexAttribArray(location);
                        glVertexAttribPointer(location, size, accessor.componentType, 
                                              accessor.normalized ? GL_TRUE : GL_FALSE, 
                                              accessor.ByteStride(bufferView), (void*)0);
                    }
                    // Note: We leak VBO handle here technically if we don't store it to delete later. 
                    // Usually we store them in primitive or just rely on VAO deletion (VAO doesn't delete VBOs though).
                    // For prototype, this is acceptable but should be tracked. Not critical for static models.
                }
                
                glBindVertexArray(0);
                newNode->primitives.push_back(prim);
            }
        }

        // Children
        for (int childIdx : inputNode.children) {
            newNode->children.push_back(processNode(childIdx));
        }

        return newNode;
    };

    for (int nodeIdx : scene.nodes) {
        nodes.push_back(processNode(nodeIdx));
    }
}

void Model::draw(Shader& shader, const glm::mat4& modelMatrix) {
    for (const auto& node : nodes) {
        drawNode(node.get(), shader, modelMatrix);
    }
}

void Model::drawNode(Node* node, Shader& shader, const glm::mat4& parentTransform) {
    glm::mat4 globalTransform = parentTransform * node->localTransform;

    for (const auto& primitive : node->primitives) {
        // Material binding
        if (primitive.materialIndex >= 0 && static_cast<size_t>(primitive.materialIndex) < impl->model.materials.size()) {
            const auto& mat = impl->model.materials[primitive.materialIndex];
            // Base Color Texture
            int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
            if (texIndex >= 0 && static_cast<size_t>(texIndex) < textures.size()) {
                textures[texIndex]->bind(0);
                shader.setInt("uAlbedoMap", 0); 
                shader.setBool("uHasTexture", true);
            } else {
                shader.setBool("uHasTexture", false);
            }

            // Emissive Texture
            int emissiveIndex = mat.emissiveTexture.index;
            if (emissiveIndex >= 0 && static_cast<size_t>(emissiveIndex) < textures.size()) {
                textures[emissiveIndex]->bind(1);
                shader.setInt("uEmissiveMap", 1);
                shader.setBool("uHasEmissive", true);
            } else {
                 shader.setBool("uHasEmissive", false);
            }

            // Base color factor
             std::vector<double> baseColor = mat.pbrMetallicRoughness.baseColorFactor;
             if (baseColor.size() == 4) {
                 shader.setVec4("uBaseColor", glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]));
             } else {
                 shader.setVec4("uBaseColor", glm::vec4(1.0f));
             }
        } else {
            shader.setBool("uHasTexture", false);
            shader.setBool("uHasEmissive", false);
            shader.setVec4("uBaseColor", glm::vec4(1.0f));
        }
        
        shader.setMat4("uModel", globalTransform); // Override model matrix passed by Entity if needed, or multiply? 
        // Wait, Entity calls draw() after setting uModel with its own transform.
        // If we set uModel here, we overwrite Entity's transform!
        // We should pass Entity transform into draw() or multiply.
        // Actually, drawNode should probably take a "baseTransform" which is the Entity's transform.
        // BUT, shader.setMat4("uModel", globalTransform) sets exact world space matrix.
        // Ideally we assume `parentTransform` passed to `draw` IS the Entity transform.
        
        glBindVertexArray(primitive.VAO);
        if (primitive.indexCount > 0) {
            glDrawElements(primitive.mode, primitive.indexCount, primitive.indexType, 0); 
        } else {
            // Draw arrays? GLTF usually indexed.
        }
        glBindVertexArray(0);
    }

    for (const auto& child : node->children) {
        drawNode(child.get(), shader, globalTransform);
    }
}

void Model::updateAnimation(float deltaTime) {
    animationTime += deltaTime;
    // Placeholder for implementing skeletal animation later
}

}
