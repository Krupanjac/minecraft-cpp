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
    // Build the full tree
    nodeMap.resize(impl->model.nodes.size());
    
    std::function<std::unique_ptr<Node>(int)> processNode = [&](int nodeIndex) -> std::unique_ptr<Node> {
        const tinygltf::Node& inputNode = impl->model.nodes[nodeIndex];
        auto newNode = std::make_unique<Node>();
        newNode->index = nodeIndex;
        // Check array bounds before assignment
        if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < nodeMap.size()) {
            nodeMap[nodeIndex] = newNode.get();
        }
        
        // Transform
        if (inputNode.matrix.size() == 16) {
            newNode->matrix = glm::make_mat4(inputNode.matrix.data());
            newNode->useTRS = false;
        } else {
            newNode->useTRS = true;
            if (inputNode.translation.size() == 3) {
                newNode->translation = glm::make_vec3(inputNode.translation.data());
            }
            if (inputNode.rotation.size() == 4) {
                newNode->rotation = glm::make_quat(inputNode.rotation.data());
            }
            if (inputNode.scale.size() == 3) {
                newNode->scale = glm::make_vec3(inputNode.scale.data());
            }
        }
        // Calculate initial local transform
        // (Moved to getLocalMatrix or updateGlobalTransforms)
        // Check getLocalMatrix() in Node struct definition (Model.h)

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
            auto childNode = processNode(childIdx);
            childNode->parent = newNode.get();
            newNode->children.push_back(std::move(childNode));
        }

        return newNode;
    };

    for (int nodeIdx : scene.nodes) {
        nodes.push_back(processNode(nodeIdx));
    }
    
    loadSkins();
    // Initial update to set global transforms
    updateAnimation(0.0f);
}

void Model::loadSkins() {
    for (const auto& sourceSkin : impl->model.skins) {
        Skin skin;
        skin.name = sourceSkin.name;
        skin.skeletonRoot = sourceSkin.skeleton;
        
        for (int jointIndex : sourceSkin.joints) {
            skin.joints.push_back(jointIndex);
        }
        
        if (sourceSkin.inverseBindMatrices > -1) {
            const auto& accessor = impl->model.accessors[sourceSkin.inverseBindMatrices];
            const auto& bufferView = impl->model.bufferViews[accessor.bufferView];
            const auto& buffer = impl->model.buffers[bufferView.buffer];
            
            skin.inverseBindMatrices.resize(accessor.count);
            memcpy(skin.inverseBindMatrices.data(), 
                   &buffer.data[accessor.byteOffset + bufferView.byteOffset], 
                   accessor.count * sizeof(glm::mat4));
        }
        
        skins.push_back(skin);
    }
}

void Model::draw(Shader& shader, const glm::mat4& modelMatrix) {
    for (const auto& node : nodes) {
        drawNode(node.get(), shader, modelMatrix);
    }
}

void Model::updateGlobalTransforms(Node* node, const glm::mat4& parentTransform) {
    glm::mat4 local = node->matrix;
    if (node->useTRS) {
        local = glm::mat4(1.0f);
        local = glm::translate(local, node->translation);
        local = local * glm::mat4_cast(node->rotation);
        local = glm::scale(local, node->scale);
    }
    node->localTransform = local;
    node->globalTransform = parentTransform * local;
    
    for (auto& child : node->children) {
        updateGlobalTransforms(child.get(), node->globalTransform);
    }
}

void Model::drawNode(Node* node, Shader& shader, const glm::mat4& modelMatrix) {
    if (!node) return;

    // Current node's world transform
    glm::mat4 worldTransform = modelMatrix * node->globalTransform;
    shader.setMat4("uModel", worldTransform);

    // Skinning
    if (!skins.empty() && activeSkin >= 0 && static_cast<size_t>(activeSkin) < skins.size() && !jointMatrices.empty()) {
        shader.setBool("uHasSkin", true);
        // Using getProgram() as ID is private
        glUniformMatrix4fv(glGetUniformLocation(shader.getProgram(), "uJoints"), static_cast<GLsizei>(jointMatrices.size()), GL_FALSE, glm::value_ptr(jointMatrices[0]));
    } else {
        shader.setBool("uHasSkin", false);
    }

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
        
        glBindVertexArray(primitive.VAO);
        if (primitive.indexCount > 0) {
            glDrawElements(primitive.mode, primitive.indexCount, primitive.indexType, 0); 
        } else {
            // Arrays not supported yet
        }
        glBindVertexArray(0);
    }

    for (const auto& child : node->children) {
        drawNode(child.get(), shader, modelMatrix);
    }
}

void Model::playAnimation(const std::string& name, bool loop) {
    for (size_t i = 0; i < impl->model.animations.size(); i++) {
        if (impl->model.animations[i].name == name) {
            currentAnimation = i;
            currentAnimationName = name;
            animationLoop = loop;
            animationTime = 0.0f;
            
            // Calculate duration
            animationDuration = 0.0f;
            for (const auto& sampler : impl->model.animations[i].samplers) {
                const auto& accessor = impl->model.accessors[sampler.input];
                animationDuration = std::max(animationDuration, static_cast<float>(accessor.maxValues[0]));
            }
            LOG_INFO("Playing animation: " + name + " (Duration: " + std::to_string(animationDuration) + "s)");
            return;
        }
    }
    LOG_WARNING("Animation not found: " + name);
}

void Model::stopAnimation() {
    currentAnimation = -1;
}

void Model::updateAnimation(float deltaTime) {
    if (currentAnimation >= 0) {
        animationTime += deltaTime;
        if (animationTime > animationDuration) {
            if (animationLoop) {
                animationTime = fmod(animationTime, animationDuration);
            } else {
                animationTime = animationDuration;
            }
        }
        
        const auto& anim = impl->model.animations[currentAnimation];
        for (const auto& channel : anim.channels) {
            Node* node = nodeMap[channel.target_node];
            if (!node) continue;
            
            const auto& sampler = anim.samplers[channel.sampler];
            const auto& inputAccessor = impl->model.accessors[sampler.input];
            const auto& outputAccessor = impl->model.accessors[sampler.output];
            const auto& bufferView = impl->model.bufferViews[inputAccessor.bufferView];
            const auto& buffer = impl->model.buffers[bufferView.buffer];
            
            const float* times = reinterpret_cast<const float*>(&buffer.data[inputAccessor.byteOffset + bufferView.byteOffset]);
            
            // Find keyframe
            // Simple linear search or binary search
            size_t count = inputAccessor.count;
            size_t prevKey = 0;
            size_t nextKey = 0;
            for (size_t i = 0; i < count - 1; i++) {
                if (animationTime >= times[i] && animationTime <= times[i+1]) {
                    prevKey = i;
                    nextKey = i + 1;
                    break;
                }
            }
            float t = (animationTime - times[prevKey]) / (times[nextKey] - times[prevKey]);
            if (nextKey == 0) t = 0.0f; // Start or End
            
            const auto& outBufferView = impl->model.bufferViews[outputAccessor.bufferView];
            const auto& outBuffer = impl->model.buffers[outBufferView.buffer];
            const unsigned char* outData = &outBuffer.data[outputAccessor.byteOffset + outBufferView.byteOffset];
            
            if (channel.target_path == "translation") {
                const glm::vec3* values = reinterpret_cast<const glm::vec3*>(outData);
                node->translation = glm::mix(values[prevKey], values[nextKey], t);
                node->useTRS = true;
            } else if (channel.target_path == "rotation") {
                const glm::vec4* values = reinterpret_cast<const glm::vec4*>(outData); // vec4 xyzw
                glm::quat q1 = glm::make_quat(&values[prevKey].x); // GLM make_quat might expect wxyz? standard glTF is xyzw. glm::quat constructor is w,x,y,z usually!
                // glm::make_quat from float ptr assumes memory layout. GLM quat memory is x,y,z,w by default?
                // GLTF is x,y,z,w.
                // glm::quat is x,y,z,w in memory if using GLM_FORCE_QUAT_DATA_XYZW (which might be default).
                // But let's be safe.
                glm::quat qA = glm::quat(values[prevKey].w, values[prevKey].x, values[prevKey].y, values[prevKey].z);
                glm::quat qB = glm::quat(values[nextKey].w, values[nextKey].x, values[nextKey].y, values[nextKey].z);
                node->rotation = glm::slerp(qA, qB, t);
                node->useTRS = true;
            } else if (channel.target_path == "scale") {
                const glm::vec3* values = reinterpret_cast<const glm::vec3*>(outData);
                node->scale = glm::mix(values[prevKey], values[nextKey], t);
                node->useTRS = true;
            }
        }
    }
    
    // Update transforms hierarchy
    for (auto& node : nodes) {
        updateGlobalTransforms(node.get(), glm::mat4(1.0f));
    }
    
    // Compute joints
    if (!skins.empty()) {
        const auto& skin = skins[activeSkin];
        jointMatrices.resize(skin.joints.size());
        for (size_t i = 0; i < skin.joints.size(); i++) {
            int jointNodeIdx = skin.joints[i];
            Node* jointNode = nodeMap[jointNodeIdx];
            if (jointNode) {
                glm::mat4 inverseBind = (i < skin.inverseBindMatrices.size()) ? skin.inverseBindMatrices[i] : glm::mat4(1.0f);
                glm::mat4 global = jointNode->globalTransform;
                
                // If skeleton root is specified, we might need to multiply by inverse of skeleton root?
                // But generally glTF global transforms are enough if we use inverseBindMatrix properly.
                // The formula is: JointMatrix = GlobalTransform * InverseBindMatrix
                // But GlobalTransform is model space.
                jointMatrices[i] = global * inverseBind;
            }
        }
    }
}

}
