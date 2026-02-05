#define GLM_ENABLE_EXPERIMENTAL

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "Model.h"
#include "../Core/Logger.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

Model::Model() : impl(std::make_unique<Impl>()) {
    // Private constructor for cloning - impl will be shared
}

Model::Model(const std::string& path) : impl(std::make_unique<Impl>()) {
    loadModel(path);
}

Model::~Model() = default;

std::unique_ptr<Node> Model::cloneNode(const Node* src) const {
    if (!src) return nullptr;
    
    auto dst = std::make_unique<Node>();
    dst->index = src->index;
    dst->translation = src->translation;
    dst->rotation = src->rotation;
    dst->scale = src->scale;
    dst->bindTranslation = src->bindTranslation;
    dst->bindRotation = src->bindRotation;
    dst->bindScale = src->bindScale;
    dst->matrix = src->matrix;
    dst->useTRS = src->useTRS;
    dst->localTransform = src->localTransform;
    dst->globalTransform = src->globalTransform;
    // Share mesh primitives (GPU resources)
    dst->primitives = src->primitives;
    
    // Clone children recursively
    for (const auto& child : src->children) {
        auto clonedChild = cloneNode(child.get());
        if (clonedChild) {
            clonedChild->parent = dst.get();
            dst->children.push_back(std::move(clonedChild));
        }
    }
    
    return dst;
}

std::shared_ptr<Model> Model::clone() const {
    auto cloned = std::shared_ptr<Model>(new Model());
    
    // Share the tinygltf impl (contains animation data)
    cloned->impl = std::make_unique<Impl>();
    cloned->impl->model = impl->model;  // Copy the tinygltf model data
    
    // Share textures (GPU resources)
    cloned->textures = textures;
    
    // Clone node tree (for independent transforms/animation state)
    cloned->nodeMap.resize(nodeMap.size(), nullptr);
    for (const auto& rootNode : nodes) {
        auto clonedRoot = cloneNode(rootNode.get());
        if (clonedRoot) {
            cloned->nodes.push_back(std::move(clonedRoot));
        }
    }
    
    // Rebuild nodeMap for cloned nodes
    std::function<void(Node*)> rebuildNodeMap = [&](Node* node) {
        if (!node) return;
        if (node->index >= 0 && static_cast<size_t>(node->index) < cloned->nodeMap.size()) {
            cloned->nodeMap[node->index] = node;
        }
        for (auto& child : node->children) {
            rebuildNodeMap(child.get());
        }
    };
    for (auto& rootNode : cloned->nodes) {
        rebuildNodeMap(rootNode.get());
    }
    
    // Copy skins (reference same joint indices but independent matrices)
    cloned->skins = skins;
    cloned->activeSkin = activeSkin;
    
    // Initialize independent animation state
    cloned->jointMatrices.resize(jointMatrices.size(), glm::mat4(1.0f));
    cloned->prevJointMatrices.resize(prevJointMatrices.size(), glm::mat4(1.0f));
    cloned->prevNodeGlobalTransforms.resize(prevNodeGlobalTransforms.size(), glm::mat4(1.0f));
    
    // Reset animation state (clone starts with no animation)
    cloned->currentAnimation = -1;
    cloned->currentAnimationName = "";
    cloned->animationLoop = true;
    cloned->animationTime = 0.0f;
    cloned->animationDuration = 0.0f;
    cloned->animationSpeed = 1.0f;
    cloned->animationLoopEndFactor = 1.0f;
    cloned->lockRootMotionXZ = false;
    cloned->rootMotionNodeIndex = -1;
    cloned->lockRootXZMask.clear();
    cloned->externalPoseEnabled = false;
    
    // Reload skins for the cloned model
    cloned->loadSkins();
    
    return cloned;
}

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

    // Log animations (helps us pick idle/walk for mobs like Zombie)
    if (!impl->model.animations.empty()) {
        LOG_INFO("Animations found: " + std::to_string(impl->model.animations.size()));
        for (size_t i = 0; i < impl->model.animations.size(); ++i) {
            const auto& a = impl->model.animations[i];
            std::string name = a.name.empty() ? ("anim" + std::to_string(i)) : a.name;
            float duration = 0.0f;
            for (const auto& sampler : a.samplers) {
                const auto& accessor = impl->model.accessors[sampler.input];
                if (!accessor.maxValues.empty()) {
                    duration = std::max(duration, static_cast<float>(accessor.maxValues[0]));
                }
            }
            LOG_INFO("  [" + std::to_string(i) + "] " + name + " (channels=" + std::to_string(a.channels.size()) + ", duration~" + std::to_string(duration) + "s)");
        }
    } else {
        LOG_INFO("Animations found: 0");
    }

    // Process textures from images
    LOG_INFO("Processing " + std::to_string(impl->model.images.size()) + " images for textures");
    for (size_t i = 0; i < impl->model.images.size(); ++i) {
        const auto& img = impl->model.images[i];
        LOG_INFO("Image " + std::to_string(i) + ": " + img.name + 
                 " (" + std::to_string(img.width) + "x" + std::to_string(img.height) + 
                 ", " + std::to_string(img.component) + " channels, " + 
                 std::to_string(img.image.size()) + " bytes)");
        
        if (img.image.empty() || img.width <= 0 || img.height <= 0) {
            LOG_ERROR("  Image data is invalid or empty!");
            textures.push_back(nullptr); // Push placeholder to keep indices aligned
            continue;
        }

        // glTF specifies UV origin at top-left, OpenGL expects bottom-left.
        // Flip the image vertically to match OpenGL conventions.
        std::vector<unsigned char> flippedData(img.image.size());
        int rowSize = img.width * img.component;
        for (int y = 0; y < img.height; ++y) {
            const unsigned char* srcRow = &img.image[(img.height - 1 - y) * rowSize];
            unsigned char* dstRow = &flippedData[y * rowSize];
            std::memcpy(dstRow, srcRow, rowSize);
        }

        // Create Texture from flipped memory
        auto tex = std::make_shared<Texture>(flippedData.data(), img.width, img.height, img.component);
        textures.push_back(tex);
    }
    LOG_INFO("Textures loaded: " + std::to_string(textures.size()));

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
        // Cache bind pose TRS
        newNode->bindTranslation = newNode->translation;
        newNode->bindRotation = newNode->rotation;
        newNode->bindScale = newNode->scale;
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
                    // Upload full bufferView so interleaved data + stride works correctly.
                    glBufferData(GL_ARRAY_BUFFER, bufferView.byteLength,
                                 &buffer.data[bufferView.byteOffset], GL_STATIC_DRAW);
                    
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
                        
                        // Debug: Log UV data for first few vertices
                        if (name == "TEXCOORD_0" && accessor.componentType == 5126) { // 5126 = GL_FLOAT
                            const float* uvData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                            LOG_INFO("TEXCOORD_0 Debug: accessorIdx=" + std::to_string(accessorIdx) + 
                                     ", bvOffset=" + std::to_string(bufferView.byteOffset) + 
                                     ", accOffset=" + std::to_string(accessor.byteOffset) +
                                     ", stride=" + std::to_string(accessor.ByteStride(bufferView)) +
                                     ", count=" + std::to_string(accessor.count));
                            LOG_INFO("  First 3 UVs: (" + std::to_string(uvData[0]) + "," + std::to_string(uvData[1]) + ") " +
                                     "(" + std::to_string(uvData[2]) + "," + std::to_string(uvData[3]) + ") " +
                                     "(" + std::to_string(uvData[4]) + "," + std::to_string(uvData[5]) + ")");
                        }
                        
                        glEnableVertexAttribArray(location);
                        glVertexAttribPointer(location, size, accessor.componentType,
                                              accessor.normalized ? GL_TRUE : GL_FALSE,
                                              accessor.ByteStride(bufferView), (void*)(intptr_t)accessor.byteOffset);
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

std::vector<std::string> Model::getAnimationNames() const {
    std::vector<std::string> out;
    out.reserve(impl->model.animations.size());
    for (size_t i = 0; i < impl->model.animations.size(); ++i) {
        const auto& a = impl->model.animations[i];
        out.push_back(a.name.empty() ? ("anim" + std::to_string(i)) : a.name);
    }
    return out;
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

void Model::draw(Shader& shader, const glm::mat4& modelMatrix, const glm::mat4& prevModelMatrix) {
    for (const auto& node : nodes) {
        drawNode(node.get(), shader, modelMatrix, prevModelMatrix);
    }
}

void Model::updateGlobalTransforms(Node* node, const glm::mat4& parentTransform) {
    glm::mat4 local = node->matrix;
    if (node->overrideLocal) {
        local = node->overrideLocalTransform;
    } else if (node->useTRS) {
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

void Model::drawNode(Node* node, Shader& shader, const glm::mat4& modelMatrix, const glm::mat4& prevModelMatrix) {
    if (!node) return;
    
    // Skip hidden nodes (detached limbs)
    if (node->hidden) {
        static int hiddenLogCount = 0;
        if (hiddenLogCount < 20) {
            LOG_INFO("[Model] Skipping hidden node index=" + std::to_string(node->index));
            hiddenLogCount++;
        }
        // Still recurse to children in case they need to be drawn separately
        for (const auto& child : node->children) {
            drawNode(child.get(), shader, modelMatrix, prevModelMatrix);
        }
        return;
    }

    // Current node's world transform
    glm::mat4 worldTransform = modelMatrix * node->globalTransform;
    shader.setMat4("uModel", worldTransform);

    // Previous node's world transform (same hierarchy, but previous entity transform)
    glm::mat4 prevNodeGlobal = node->globalTransform;
    if (node->index >= 0 && static_cast<size_t>(node->index) < prevNodeGlobalTransforms.size()) {
        prevNodeGlobal = prevNodeGlobalTransforms[node->index];
    }
    glm::mat4 prevWorldTransform = prevModelMatrix * prevNodeGlobal;
    shader.setMat4("uPrevModel", prevWorldTransform);

    // Skinning
    if (!skins.empty() && activeSkin >= 0 && static_cast<size_t>(activeSkin) < skins.size() && !jointMatrices.empty()) {
        shader.setBool("uHasSkin", true);
        // Using getProgram() as ID is private
        glUniformMatrix4fv(glGetUniformLocation(shader.getProgram(), "uJoints"), static_cast<GLsizei>(jointMatrices.size()), GL_FALSE, glm::value_ptr(jointMatrices[0]));

        // Previous joints for motion vectors. If missing, fall back to current.
        const auto& pj = prevJointMatrices.empty() ? jointMatrices : prevJointMatrices;
        glUniformMatrix4fv(glGetUniformLocation(shader.getProgram(), "uPrevJoints"), static_cast<GLsizei>(pj.size()), GL_FALSE, glm::value_ptr(pj[0]));
    } else {
        shader.setBool("uHasSkin", false);
    }

    for (const auto& primitive : node->primitives) {
        // Material binding
        if (primitive.materialIndex >= 0 && static_cast<size_t>(primitive.materialIndex) < impl->model.materials.size()) {
            const auto& mat = impl->model.materials[primitive.materialIndex];
            
            // Base Color Texture
            // glTF: material -> texture index -> texture -> source (image index)
            int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
            bool texturesBound = false;
            if (texIndex >= 0 && static_cast<size_t>(texIndex) < impl->model.textures.size()) {
                int imageIndex = impl->model.textures[texIndex].source;
                if (imageIndex >= 0 && static_cast<size_t>(imageIndex) < textures.size() && textures[imageIndex]) {
                    textures[imageIndex]->bind(0);
                    shader.setInt("uAlbedoMap", 0);
                    shader.setBool("uHasTexture", true);
                    texturesBound = true;
                    // Debug: Log texture ID and size once per model
                    static std::unordered_map<void*, bool> loggedOnce;
                    if (loggedOnce.find((void*)textures[imageIndex].get()) == loggedOnce.end()) {
                        LOG_INFO("Draw Texture: texID=" + std::to_string(textures[imageIndex]->getID()) + 
                                 ", size=" + std::to_string(textures[imageIndex]->getWidth()) + "x" + std::to_string(textures[imageIndex]->getHeight()));
                        loggedOnce[(void*)textures[imageIndex].get()] = true;
                    }
                }
            }
            if (!texturesBound) {
                shader.setBool("uHasTexture", false);
            }

            // Emissive Texture
            int emissiveTexIndex = mat.emissiveTexture.index;
            bool emissiveBound = false;
            if (emissiveTexIndex >= 0 && static_cast<size_t>(emissiveTexIndex) < impl->model.textures.size()) {
                int emissiveImageIndex = impl->model.textures[emissiveTexIndex].source;
                if (emissiveImageIndex >= 0 && static_cast<size_t>(emissiveImageIndex) < textures.size() && textures[emissiveImageIndex]) {
                    textures[emissiveImageIndex]->bind(1);
                    shader.setInt("uEmissiveMap", 1);
                    shader.setBool("uHasEmissive", true);
                    emissiveBound = true;
                }
            }
            if (!emissiveBound) {
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
        drawNode(child.get(), shader, modelMatrix, prevModelMatrix);
    }
}

// Helper for case-insensitive comparison
static std::string toLowerStr(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

void Model::playAnimation(const std::string& name, bool loop) {
    std::string lowerName = toLowerStr(name);
    
    // First try exact match
    for (size_t i = 0; i < impl->model.animations.size(); i++) {
        if (impl->model.animations[i].name == name) {
            currentAnimation = static_cast<int>(i);
            currentAnimationName = impl->model.animations[i].name;
            animationLoop = loop;
            animationTime = 0.0f;
            animationLoopEndFactor = 1.0f;
            
            animationDuration = 0.0f;
            for (const auto& sampler : impl->model.animations[i].samplers) {
                const auto& accessor = impl->model.accessors[sampler.input];
                animationDuration = std::max(animationDuration, static_cast<float>(accessor.maxValues[0]));
            }
            LOG_INFO("Playing animation: " + currentAnimationName + " (Duration: " + std::to_string(animationDuration) + "s)");
            return;
        }
    }
    
    // Try case-insensitive match
    for (size_t i = 0; i < impl->model.animations.size(); i++) {
        if (toLowerStr(impl->model.animations[i].name) == lowerName) {
            currentAnimation = static_cast<int>(i);
            currentAnimationName = impl->model.animations[i].name;
            animationLoop = loop;
            animationTime = 0.0f;
            animationLoopEndFactor = 1.0f;
            
            animationDuration = 0.0f;
            for (const auto& sampler : impl->model.animations[i].samplers) {
                const auto& accessor = impl->model.accessors[sampler.input];
                animationDuration = std::max(animationDuration, static_cast<float>(accessor.maxValues[0]));
            }
            LOG_INFO("Playing animation: " + currentAnimationName + " (Duration: " + std::to_string(animationDuration) + "s)");
            return;
        }
    }
    LOG_WARNING("Animation not found: " + name);
}

void Model::stopAnimation() {
    currentAnimation = -1;
}

void Model::updateAnimation(float deltaTime) {
    // Preserve previous node globals for motion vectors (covers animations that move nodes, not just bones)
    if (prevNodeGlobalTransforms.empty()) {
        prevNodeGlobalTransforms.resize(nodeMap.size(), glm::mat4(1.0f));
    }
    for (size_t i = 0; i < nodeMap.size(); ++i) {
        if (nodeMap[i]) prevNodeGlobalTransforms[i] = nodeMap[i]->globalTransform;
    }

    // Preserve previous joints before updating animation time/pose
    if (!jointMatrices.empty()) {
        prevJointMatrices = jointMatrices;
    }

    if (!externalPoseEnabled && currentAnimation >= 0) {
        // Pick a root-motion node if not set: prefer active skin skeleton root.
        if (rootMotionNodeIndex < 0) {
            if (!skins.empty() && activeSkin >= 0 && static_cast<size_t>(activeSkin) < skins.size() && skins[activeSkin].skeletonRoot >= 0) {
                rootMotionNodeIndex = skins[activeSkin].skeletonRoot;
            } else if (!nodes.empty() && nodes[0]) {
                rootMotionNodeIndex = nodes[0]->index; // fallback: first root node
            }
        }
        
        // Build a mask of skeleton nodes so we can lock XZ translation on ALL of them when needed.
        // Many assets bake forward motion into hips/pelvis translation instead of the skeleton root.
        if (lockRootMotionXZ) {
            if (lockRootXZMask.size() != nodeMap.size()) {
                lockRootXZMask.assign(nodeMap.size(), 0);
                if (!skins.empty() && activeSkin >= 0 && static_cast<size_t>(activeSkin) < skins.size()) {
                    const auto& skin = skins[activeSkin];
                    if (skin.skeletonRoot >= 0 && (size_t)skin.skeletonRoot < lockRootXZMask.size()) {
                        lockRootXZMask[(size_t)skin.skeletonRoot] = 1;
                    }
                    for (int j : skin.joints) {
                        if (j >= 0 && (size_t)j < lockRootXZMask.size()) lockRootXZMask[(size_t)j] = 1;
                    }
                } else if (rootMotionNodeIndex >= 0 && (size_t)rootMotionNodeIndex < lockRootXZMask.size()) {
                    lockRootXZMask[(size_t)rootMotionNodeIndex] = 1;
                }
            }
        } else {
            // Keep mask empty when not in use
            lockRootXZMask.clear();
        }

        float endTime = animationDuration * std::clamp(animationLoopEndFactor, 0.0f, 1.0f);
        // Avoid 0-length loops
        endTime = std::max(endTime, 0.0001f);

        animationTime += deltaTime * std::max(0.0f, animationSpeed);
        if (animationTime > endTime) {
            if (animationLoop) {
                animationTime = fmod(animationTime, endTime);
            } else {
                animationTime = endTime;
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
                glm::vec3 tr = glm::mix(values[prevKey], values[nextKey], t);
                // Lock root-motion XZ to bind pose to prevent mesh drifting away from entity.
                // We lock the skeleton root AND (if available) all skeleton nodes (hips/pelvis commonly have baked motion).
                bool lockThisNode = false;
                if (lockRootMotionXZ) {
                    if (node->index == rootMotionNodeIndex) lockThisNode = true;
                    if (!lockRootXZMask.empty() && node->index >= 0 && (size_t)node->index < lockRootXZMask.size() && lockRootXZMask[(size_t)node->index]) {
                        lockThisNode = true;
                    }
                }
                if (lockThisNode) {
                    tr.x = node->bindTranslation.x;
                    tr.z = node->bindTranslation.z;
                }
                node->translation = tr;
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
                // If node is hidden (detached limb), scale the joint matrix to zero
                // This will collapse all vertices weighted to this joint to origin
                if (jointNode->hidden) {
                    jointMatrices[i] = glm::mat4(0.0f);
                    continue;
                }
                
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

    // First frame / no previous joints yet: initialize prev = current to avoid huge bogus motion.
    if (prevJointMatrices.empty() && !jointMatrices.empty()) {
        prevJointMatrices = jointMatrices;
    }

    // First frame / no previous nodes yet: initialize prev nodes too.
    if (prevNodeGlobalTransforms.empty() && !nodeMap.empty()) {
        prevNodeGlobalTransforms.resize(nodeMap.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < nodeMap.size(); ++i) {
            if (nodeMap[i]) prevNodeGlobalTransforms[i] = nodeMap[i]->globalTransform;
        }
    }
}

glm::mat4 Model::getNodeGlobalTransform(const std::string& nodeName) const {
    // Search through all nodes in nodeMap to find the one with matching name
    for (size_t i = 0; i < impl->model.nodes.size(); ++i) {
        if (impl->model.nodes[i].name == nodeName) {
            if (i < nodeMap.size() && nodeMap[i]) {
                return nodeMap[i]->globalTransform;
            }
        }
    }
    
    // Not found, return identity
    return glm::mat4(1.0f);
}

bool Model::hasNode(const std::string& nodeName) const {
    for (const auto& node : impl->model.nodes) {
        if (node.name == nodeName) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> Model::getNodeNames() const {
    std::vector<std::string> names;
    names.reserve(impl->model.nodes.size());
    for (const auto& node : impl->model.nodes) {
        names.push_back(node.name);
    }
    return names;
}

int Model::getNodeIndexByName(const std::string& nodeName) const {
    for (size_t i = 0; i < impl->model.nodes.size(); ++i) {
        if (impl->model.nodes[i].name == nodeName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Model::getParentIndex(int nodeIndex) const {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= nodeMap.size()) return -1;
    Node* node = nodeMap[nodeIndex];
    if (!node || !node->parent) return -1;
    return node->parent->index;
}

glm::mat4 Model::getNodeGlobalTransformByIndex(int nodeIndex) const {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= nodeMap.size()) {
        return glm::mat4(1.0f);
    }
    Node* node = nodeMap[nodeIndex];
    return node ? node->globalTransform : glm::mat4(1.0f);
}

void Model::setNodeOverrideLocalTransform(int nodeIndex, const glm::mat4& local) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= nodeMap.size()) return;
    Node* node = nodeMap[nodeIndex];
    if (!node) return;
    node->overrideLocal = true;
    node->overrideLocalTransform = local;
}

void Model::clearNodeOverride(int nodeIndex) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= nodeMap.size()) return;
    Node* node = nodeMap[nodeIndex];
    if (!node) return;
    node->overrideLocal = false;
}

void Model::clearAllNodeOverrides() {
    for (auto* node : nodeMap) {
        if (node) node->overrideLocal = false;
    }
}

void Model::setNodeHidden(int nodeIndex, bool hidden) {
    if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < nodeMap.size() && nodeMap[nodeIndex]) {
        nodeMap[nodeIndex]->hidden = hidden;
        LOG_INFO("[Model] setNodeHidden nodeIndex=" + std::to_string(nodeIndex) + " hidden=" + (hidden ? "true" : "false") + " nodeMapSize=" + std::to_string(nodeMap.size()));
    } else {
        LOG_INFO("[Model] setNodeHidden FAILED nodeIndex=" + std::to_string(nodeIndex) + " nodeMapSize=" + std::to_string(nodeMap.size()));
    }
}

void Model::clearAllHiddenNodes() {
    for (auto* node : nodeMap) {
        if (node) node->hidden = false;
    }
}

void Model::setAllNodesHidden(bool hidden) {
    for (auto* node : nodeMap) {
        if (node) node->hidden = hidden;
    }
}

const std::vector<int>& Model::getActiveSkinJoints() const {
    static const std::vector<int> empty;
    if (skins.empty() || activeSkin < 0 || static_cast<size_t>(activeSkin) >= skins.size()) {
        return empty;
    }
    return skins[activeSkin].joints;
}

}
