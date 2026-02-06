#include "UIManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

// Prevent Windows min/max macros from interfering with std::min/max
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#endif

#define STB_IMAGE_IMPLEMENTATION_DONE
#include <stb_image.h>

// ============================================================================
// UIManager Core - Constructor, Initialization, Input Handling, State Management
// ============================================================================

UIManager::UIManager() : vao(0), vbo(0), texturedVao(0), texturedVbo(0), blockIconVao(0), blockIconVbo(0), width(1280), height(720), showDebug(false), currentFPS(0.0f), currentMenuState(MenuState::MAIN_MENU) {}

void UIManager::initialize(int windowWidth, int windowHeight) {
    width = windowWidth;
    height = windowHeight;

    // Simple UI shader
    const char* vertSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 uProjection;
        uniform mat4 uModel;
        void main() {
            gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* fragSrc = R"(
        #version 450 core
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    uiShader.loadFromSource(vertSrc, fragSrc);
    
    // Textured UI shader for world preview thumbnails
    const char* texVertSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        uniform mat4 uProjection;
        uniform mat4 uModel;
        void main() {
            gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* texFragSrc = R"(
        #version 450 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        void main() {
            FragColor = texture(uTexture, TexCoord);
        }
    )";
    
    texturedShader.loadFromSource(texVertSrc, texFragSrc);
    
    // 3D Isometric block icon shader
    const char* blockIconVertSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        layout (location = 2) in vec3 aNormal;
        layout (location = 3) in float aFaceId;
        
        out vec2 TexCoord;
        out vec3 Normal;
        out float FaceId;
        
        uniform mat4 uProjection;
        uniform mat4 uModel;
        
        void main() {
            gl_Position = uProjection * uModel * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
            Normal = aNormal;
            FaceId = aFaceId;
        }
    )";
    
    const char* blockIconFragSrc = R"(
        #version 450 core
        in vec2 TexCoord;
        in vec3 Normal;
        in float FaceId;
        
        out vec4 FragColor;
        
        uniform sampler2D uTexture;
        uniform vec2 uAtlasOffset[3]; // top, side, bottom atlas offsets
        uniform float uCellSize;
        
        void main() {
            // Select atlas offset based on face
            int faceIdx = int(FaceId + 0.5);
            vec2 atlasOffset = uAtlasOffset[faceIdx];
            
            // Sample from atlas
            vec2 atlasUV = atlasOffset + TexCoord * uCellSize;
            vec4 texColor = texture(uTexture, atlasUV);
            
            // Simple lighting based on normal
            float light = 1.0;
            if (Normal.y > 0.5) light = 1.0;      // Top - brightest
            else if (Normal.x > 0.5) light = 0.8;  // Right side
            else if (Normal.z > 0.5) light = 0.6;  // Front side
            
            FragColor = vec4(texColor.rgb * light, texColor.a);
        }
    )";
    
    blockIconShader.loadFromSource(blockIconVertSrc, blockIconFragSrc);
    
    // Setup isometric cube VAO for block icons
    // Cube vertices: position (3), texcoord (2), normal (3), faceId (1)
    // All 6 faces for full rotation support
    float cubeVertices[] = {
        // Top face (Y+) - faceId = 0
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,  0.0f,
        
        // Bottom face (Y-) - faceId = 2
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  2.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,  2.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,  2.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  2.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,  2.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,  2.0f,
        
        // Front face (Z+) - faceId = 1 (side texture)
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f,
        
        // Back face (Z-) - faceId = 1 (side texture)
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f,
        
        // Right face (X+) - faceId = 1 (side texture)
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  1.0f,
        
        // Left face (X-) - faceId = 1 (side texture)
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,  1.0f,
    };
    
    glGenVertexArrays(1, &blockIconVao);
    glGenBuffers(1, &blockIconVbo);
    
    glBindVertexArray(blockIconVao);
    glBindBuffer(GL_ARRAY_BUFFER, blockIconVbo);;
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    // TexCoord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    // Normal
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
    // FaceId
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
    
    glBindVertexArray(0);
    
    // Load block atlas texture
    // Force flip to true so we get Bottom-Up data (OpenGL standard)
    // This matches what Texture.cpp does, ensuring consistency
    stbi_set_flip_vertically_on_load(true);
    int texW, texH, texChannels;
    unsigned char* data = stbi_load("assets/block_atlas.png", &texW, &texH, &texChannels, 4);
    if (data) {
        // No manual flip needed - stbi does it for us
        glGenTextures(1, &blockAtlasTexture);
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Do not unbind here if not necessary, but good practice
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
    }
    
    // Load model preview shader (reuse existing model shaders)
    if (!modelPreviewShader.loadFromFiles("shaders/model.vert", "shaders/model.frag")) {
        std::cerr << "Failed to load model preview shader" << std::endl;
    }
    
    // Create preview FBO for model rendering
    glGenFramebuffers(1, &previewFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, previewFBO);
    
    // Create color texture (color output)
    glGenTextures(1, &previewTexture);
    glBindTexture(GL_TEXTURE_2D, previewTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, previewWidth, previewHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, previewTexture, 0);
    
    // Create velocity texture (shader outputs to location 1)
    GLuint velocityTexture;
    glGenTextures(1, &velocityTexture);
    glBindTexture(GL_TEXTURE_2D, velocityTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, previewWidth, previewHeight, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocityTexture, 0);
    
    // Set draw buffers for both attachments
    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);
    
    // Create depth renderbuffer
    glGenRenderbuffers(1, &previewDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, previewDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, previewWidth, previewHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, previewDepth);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Preview FBO not complete! Status: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Setup quad VAO (no texture coords)
    float vertices[] = { 
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f, 
    
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
    // Setup textured quad VAO (with texture coords)
    float texturedVertices[] = {
        // pos        // tex
        0.0f, 1.0f,   0.0f, 0.0f,
        1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 0.0f,   0.0f, 1.0f,
        
        0.0f, 1.0f,   0.0f, 0.0f,
        1.0f, 1.0f,   1.0f, 0.0f,
        1.0f, 0.0f,   1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &texturedVao);
    glGenBuffers(1, &texturedVbo);
    
    glBindVertexArray(texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, texturedVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texturedVertices), texturedVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // Pre-load the preview model for player settings
    loadPreviewModel(Settings::instance().playerModelIndex);
    
    // Initialize hotbar with default items
    initializeHotbar();

    setupMainMenu();
}

void UIManager::initializeHotbar() {
    // Default hotbar layout: some blocks and some tools
    // First 5 slots: blocks
    hotbarSlots[0] = HotbarSlot(BlockType::STONE);
    hotbarSlots[1] = HotbarSlot(BlockType::DIRT);
    hotbarSlots[2] = HotbarSlot(BlockType::WOOD);
    hotbarSlots[3] = HotbarSlot(BlockType::LEAVES);
    hotbarSlots[4] = HotbarSlot(BlockType::SAND);
    
    // Slots 5-8: tools
    hotbarSlots[5] = HotbarSlot(ItemType::SWORD_DIAMOND);
    hotbarSlots[6] = HotbarSlot(ItemType::PICKAXE_DIAMOND);
    hotbarSlots[7] = HotbarSlot(ItemType::AXE_DIAMOND);
    hotbarSlots[8] = HotbarSlot(ItemType::SHOVEL_DIAMOND);
    
    // Keep legacy hotbar array in sync
    for (int i = 0; i < 9; ++i) {
        if (!hotbarSlots[i].isItem) {
            hotbar[i] = hotbarSlots[i].blockType;
        } else {
            hotbar[i] = BlockType::AIR; // Items show AIR in legacy array
        }
    }
}

void UIManager::handleResize(int w, int h) {
    width = w;
    height = h;
    // Re-setup menu to center elements
    if (isMenuOpen()) setMenuState(currentMenuState); 
}

void UIManager::setMenuState(MenuState state) {
    currentMenuState = state;
    elements.clear();
    
    switch (state) {
        case MenuState::MAIN_MENU: setupMainMenu(); break;
        case MenuState::IN_GAME_MENU: setupInGameMenu(); break;
        case MenuState::SETTINGS: setupSettingsMenu(); break;
        case MenuState::VIDEO_SETTINGS: setupVideoSettingsMenu(); break;
        case MenuState::AUDIO_SETTINGS: setupAudioSettingsMenu(); break;
        case MenuState::PLAYER_SETTINGS: setupPlayerSettingsMenu(); break;
        case MenuState::CONTROLS: setupControlsMenu(); break;
        case MenuState::LOAD_GAME: setupLoadGameMenu(); break;
        case MenuState::NEW_GAME: setupNewGameMenu(); break;
        case MenuState::INVENTORY: setupInventoryMenu(); break;
        case MenuState::MAP: setupMapMenu(); break;
        case MenuState::MULTIPLAYER: setupMultiplayerMenu(); break;
        case MenuState::HOST_GAME: setupHostGameMenu(); break;
        case MenuState::JOIN_GAME: setupJoinGameMenu(); break;
        case MenuState::ABOUT: setupAboutMenu(); break;
        case MenuState::DEATH_SCREEN: setupDeathScreen(); break;
        case MenuState::CHAT: break; // Chat doesn't use elements system
        case MenuState::NONE: break;
    }
}

void UIManager::handleCharInput(unsigned int codepoint) {
    // Handle chat input
    if (currentMenuState == MenuState::CHAT) {
        if (codepoint >= 32 && codepoint <= 126) {
            chatInput += (char)codepoint;
        }
        return;
    }
    
    if (!isMenuOpen()) return;
    
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (codepoint >= 32 && codepoint <= 126) {
                *el.textRef += (char)codepoint;
            }
            // Update display text - sync with textRef for all input fields
            el.text = *el.textRef;
        }
    }
}

void UIManager::handleKeyInput(int key) {
    // Handle chat keys
    if (currentMenuState == MenuState::CHAT) {
        if (key == 259) { // GLFW_KEY_BACKSPACE
            if (!chatInput.empty()) {
                chatInput.pop_back();
            }
        } else if (key == 257) { // GLFW_KEY_ENTER
            if (!chatInput.empty() && onSendChat) {
                onSendChat(chatInput);
                chatInput.clear();
            }
            closeChat();
        } else if (key == 256) { // GLFW_KEY_ESCAPE
            closeChat();
        }
        return;
    }
    
    if (!isMenuOpen()) return;
    
    // Handle backspace for text input in any menu
    for (auto& el : elements) {
        if (el.isInput && el.isHovered && el.textRef) {
            if (key == 259) { // GLFW_KEY_BACKSPACE
                if (!el.textRef->empty()) {
                    el.textRef->pop_back();
                    el.text = *el.textRef;
                }
            }
        }
    }
    
    // For Keybinding
    if (waitingForKeyBind && keyBindPtr) {
        *keyBindPtr = key;
        waitingForKeyBind = false;
        keyBindPtr = nullptr;
        setupControlsMenu(); // Refresh text
        if (onSettingsChanged) onSettingsChanged();
    }
}

void UIManager::updateDebugInfo(float fps, const std::string& blockName, const glm::vec3& playerPos, const glm::vec3& playerVel, float taaMotion, float taaHistoryWeight) {
    currentFPS = fps;
    currentBlockName = blockName;
    currentPlayerPos = playerPos;
    currentPlayerVel = playerVel;
    // TAA debug metrics
    lastTaaMotion = taaMotion;
    lastTaaHistoryWeight = taaHistoryWeight;
}
