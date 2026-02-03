#include "UIManager.h"
#include "Console.h"
#include "../World/WorldSerializer.h"
#include "../World/WorldGenerator.h"
#include "../Core/HardwareInfo.h"
#include "../Audio/AudioManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <random>
#include <climits>
#include <cctype>

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

void UIManager::setupMainMenu() {
    elements.clear();
    
    // Play menu music
    Audio::AudioManager::instance().playMusic(Audio::SoundType::MUSIC_MENU, true, 2.0f);
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f + 30.0f; // Shifted down to make room for title
    float btnW = 280.0f;  // Wider buttons
    float btnH = 50.0f;   // Taller buttons
    float gap = 12.0f;    // More spacing

    elements.push_back({centerX - btnW/2, centerY - 95, btnW, btnH, "Singleplayer", false, [this]() { 
        setMenuState(MenuState::NEW_GAME); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap), btnW, btnH, "Multiplayer", false, [this]() { 
        setMenuState(MenuState::MULTIPLAYER); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*2, btnW, btnH, "Options", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*3, btnW, btnH, "About", false, [this]() { 
        setMenuState(MenuState::ABOUT); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 95 + (btnH + gap)*4, btnW, btnH, "Quit Game", false, [this]() { 
        if (onExit) onExit(); 
    }});
    
    // Player card in top-right corner
    UIElement playerCard;
    playerCard.x = width - 220.0f;
    playerCard.y = 20.0f;
    playerCard.w = 200.0f;
    playerCard.h = 60.0f;
    playerCard.text = Settings::instance().playerNickname;
    playerCard.isCard = true;
    playerCard.tooltip = "Click to edit player settings";
    playerCard.onClick = [this]() { setMenuState(MenuState::PLAYER_SETTINGS); };
    elements.push_back(playerCard);
}

void UIManager::setupInGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    elements.push_back({centerX - btnW/2, centerY - 100, btnW, btnH, "RESUME", false, [this]() { 
        setMenuState(MenuState::NONE); 
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + btnH + gap, btnW, btnH, "SAVE GAME", false, [this]() { 
        if (onSave) onSave();
    }});

    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*2, btnW, btnH, "SETTINGS", false, [this]() { 
        setMenuState(MenuState::SETTINGS); 
    }});
    
    // Respawn button - kill self to test death screen
    elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*3, btnW, btnH, "RESPAWN", false, [this]() { 
        // This triggers death and respawn
        if (onRespawn) onRespawn();
    }});
    
    if (isOnline) {
        elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*4, btnW, btnH, "DISCONNECT", false, [this]() { 
            if (onDisconnectGame) onDisconnectGame();
            worldLoaded = false;
            if (onReturnToMainMenu) onReturnToMainMenu();
            setMenuState(MenuState::MAIN_MENU); 
        }});
    } else {
        elements.push_back({centerX - btnW/2, centerY - 100 + (btnH + gap)*4, btnW, btnH, "MAIN MENU", false, [this]() { 
            if (onSave) onSave(); // Auto save on exit to menu
            worldLoaded = false;
            if (onReturnToMainMenu) onReturnToMainMenu();
            setMenuState(MenuState::MAIN_MENU); 
        }});
    }
}

void UIManager::setupSettingsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 40.0f;
    float gap = 10.0f;
    float startY = cy - 150.0f;

    elements.push_back({cx - btnW/2, startY, btnW, btnH, "VIDEO SETTINGS", false, [this]() { 
        setMenuState(MenuState::VIDEO_SETTINGS); 
    }});
    startY += btnH + gap;
    
    // Audio Settings
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "AUDIO SETTINGS", false, [this]() {
        setMenuState(MenuState::AUDIO_SETTINGS);
    }});
    startY += btnH + gap;

    // Player Settings (nickname + model)
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "PLAYER SETTINGS", false, [this]() {
         setMenuState(MenuState::PLAYER_SETTINGS);
    }});
    startY += btnH + gap;

    // Controls
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "CONTROLS", false, [this]() {
         setMenuState(MenuState::CONTROLS);
    }});
    startY += btnH + gap;

    // Back
    elements.push_back({cx - btnW/2, startY + 20, btnW, btnH, "BACK", false, [this]() { 
        setMenuState(MenuState::MAIN_MENU); 
    }});
}

void UIManager::setupVideoSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    // Grid layout - 2 columns
    float colWidth = 280.0f;
    float colGap = 40.0f;
    float rowHeight = 35.0f;
    float rowGap = 8.0f;
    float startY = 120.0f; // Moved down to accommodate preset selector
    float leftCol = width / 2.0f - colWidth - colGap / 2;
    float rightCol = width / 2.0f + colGap / 2;
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 100;
    title.y = 30;
    title.w = 200;
    title.h = 30;
    title.text = "Video Settings";
    title.isLabel = true;
    elements.push_back(title);
    
    // === Graphics Preset Selector (prominent at top) ===
    UIElement presetLabel;
    presetLabel.x = width / 2.0f - 200;
    presetLabel.y = 70;
    presetLabel.w = 100;
    presetLabel.h = 30;
    presetLabel.text = "Preset:";
    presetLabel.isLabel = true;
    elements.push_back(presetLabel);
    
    UIElement preset;
    preset.x = width / 2.0f - 90;
    preset.y = 70;
    preset.w = 280;
    preset.h = 30;
    preset.text = Settings::PRESET_NAMES[s.graphicsPreset];
    preset.intValueRef = &s.graphicsPreset;
    preset.onClick = [](){}; // intValueRef handles cycling, applyPreset called in handleClick
    preset.minVal = 0.0f; 
    preset.maxVal = float(Settings::NUM_PRESETS - 1);
    preset.tooltip = "Quick graphics preset - Low/Medium/High or RT variants (click to cycle)";
    elements.push_back(preset);
    
    // === LEFT COLUMN - Performance ===
    UIElement perfHeader;
    perfHeader.x = leftCol;
    perfHeader.y = startY;
    perfHeader.w = colWidth;
    perfHeader.h = 25;
    perfHeader.text = "-- Performance --";
    perfHeader.isLabel = true;
    perfHeader.isHeader = true;
    elements.push_back(perfHeader);
    
    float y = startY + 35;
    
    // Render Distance
    UIElement rd;
    rd.x = leftCol; rd.y = y; rd.w = colWidth; rd.h = rowHeight;
    rd.text = "Render Distance: " + std::to_string(s.renderDistance);
    rd.isSlider = true;
    rd.intValueRef = &s.renderDistance;
    rd.minVal = 2.0f; rd.maxVal = 32.0f;
    rd.tooltip = "How far chunks are rendered (2-32)";
    elements.push_back(rd);
    y += rowHeight + rowGap;
    
    // Shadow Distance
    UIElement sd;
    sd.x = leftCol; sd.y = y; sd.w = colWidth; sd.h = rowHeight;
    sd.text = "Shadow Distance: " + std::to_string((int)s.shadowDistance);
    sd.isSlider = true;
    sd.valueRef = &s.shadowDistance;
    sd.minVal = 50.0f; sd.maxVal = 300.0f;
    sd.tooltip = "Max distance for shadow rendering";
    elements.push_back(sd);
    y += rowHeight + rowGap;
    
    // VSync
    UIElement vs;
    vs.x = leftCol; vs.y = y; vs.w = colWidth; vs.h = rowHeight;
    vs.text = "VSync: " + std::string(s.vsync ? "ON" : "OFF");
    vs.boolValueRef = &s.vsync;
    vs.onClick = [](){};
    vs.tooltip = "Sync framerate to monitor refresh rate";
    elements.push_back(vs);
    y += rowHeight + rowGap;
    
    // Window Mode
    UIElement wm;
    wm.x = leftCol; wm.y = y; wm.w = colWidth; wm.h = rowHeight;
    std::string wmText = "Window: ";
    if (s.fullscreen == 0) wmText += "Windowed";
    else if (s.fullscreen == 1) wmText += "Fullscreen";
    else wmText += "Borderless";
    wm.text = wmText;
    wm.intValueRef = &s.fullscreen;
    wm.onClick = [](){};
    wm.minVal = 0.0f; wm.maxVal = 2.0f;
    wm.tooltip = "Click to cycle: Windowed/Fullscreen/Borderless";
    elements.push_back(wm);
    y += rowHeight + rowGap;
    
    // === LEFT COLUMN - Effects ===
    y += 15;
    UIElement fxHeader;
    fxHeader.x = leftCol;
    fxHeader.y = y;
    fxHeader.w = colWidth;
    fxHeader.h = 25;
    fxHeader.text = "-- Effects --";
    fxHeader.isLabel = true;
    fxHeader.isHeader = true;
    elements.push_back(fxHeader);
    y += 35;
    
    // Shadows
    UIElement sh;
    sh.x = leftCol; sh.y = y; sh.w = colWidth; sh.h = rowHeight;
    sh.text = "Shadows: " + std::string(s.enableShadows ? "ON" : "OFF");
    sh.boolValueRef = &s.enableShadows;
    sh.onClick = [](){};
    sh.tooltip = "Enable dynamic shadows from sun/moon";
    elements.push_back(sh);
    y += rowHeight + rowGap;
    
    // SSAO
    UIElement ssao;
    ssao.x = leftCol; ssao.y = y; ssao.w = colWidth; ssao.h = rowHeight;
    ssao.text = "SSAO: " + std::string(s.enableSSAO ? "ON" : "OFF");
    ssao.boolValueRef = &s.enableSSAO;
    ssao.onClick = [](){};
    ssao.tooltip = "Screen-space ambient occlusion";
    elements.push_back(ssao);
    y += rowHeight + rowGap;
    
    // Volumetrics
    UIElement vol;
    vol.x = leftCol; vol.y = y; vol.w = colWidth; vol.h = rowHeight;
    vol.text = "Volumetrics: " + std::string(s.enableVolumetrics ? "ON" : "OFF");
    vol.boolValueRef = &s.enableVolumetrics;
    vol.onClick = [](){};
    vol.tooltip = "Volumetric lighting (god rays)";
    elements.push_back(vol);
    y += rowHeight + rowGap;
    
    // Anti-aliasing method selector
    UIElement aa;
    aa.x = leftCol; aa.y = y; aa.w = colWidth; aa.h = rowHeight;
    aa.text = "AA METHOD: " + std::string(Settings::AA_METHOD_NAMES[s.aaMethod]);
    aa.intValueRef = &s.aaMethod;
    aa.onClick = [](){};  // intValueRef handles cycling
    aa.minVal = 0.0f; aa.maxVal = 2.0f;  // None=0, FXAA=1, TAA=2
    aa.tooltip = "Anti-aliasing method (None/FXAA/TAA)";
    elements.push_back(aa);
    y += rowHeight + rowGap;
    
    // Ray Tracing toggle
    UIElement rt;
    rt.x = leftCol; rt.y = y; rt.w = colWidth; rt.h = rowHeight;
    rt.text = "Ray Tracing: " + std::string(s.enableRayTracing ? "ON" : "OFF");
    rt.boolValueRef = &s.enableRayTracing;
    rt.onClick = [this]() { setupVideoSettingsMenu(); }; // Refresh menu to show/hide RT options
    rt.tooltip = "Experimental voxel ray tracing (OpenGL 4.3+)";
    elements.push_back(rt);
    y += rowHeight + rowGap;
    
    // Only show RT sub-options when Ray Tracing is enabled
    if (s.enableRayTracing) {
        // Ray Tracing quality selector
        UIElement rtq;
        rtq.x = leftCol; rtq.y = y; rtq.w = colWidth; rtq.h = rowHeight;
        rtq.text = "RT Quality: " + std::string(Settings::RT_QUALITY_NAMES[s.rayTracingQuality]);
        rtq.intValueRef = &s.rayTracingQuality;
        rtq.onClick = [](){};  // intValueRef handles cycling
        rtq.minVal = 0.0f; rtq.maxVal = 2.0f;  // Low=0, Medium=1, High=2
        rtq.tooltip = "Ray tracing quality (Low/Medium/High)";
        elements.push_back(rtq);
        y += rowHeight + rowGap;
        
        // Shadow Method selector
        UIElement sm;
        sm.x = leftCol; sm.y = y; sm.w = colWidth; sm.h = rowHeight;
        sm.text = "Shadows: " + std::string(Settings::SHADOW_METHOD_NAMES[s.shadowMethod]);
        sm.intValueRef = &s.shadowMethod;
        sm.onClick = [](){};
        sm.minVal = 0.0f; sm.maxVal = 1.0f;  // ShadowMap=0, RayTraced=1
        sm.tooltip = "Shadow method (Shadow Map = fast, Ray Traced = accurate)";
        elements.push_back(sm);
        y += rowHeight + rowGap;
        
        // RT Shadows toggle
        UIElement rts;
        rts.x = leftCol; rts.y = y; rts.w = colWidth; rts.h = rowHeight;
        rts.text = "RT Shadows: " + std::string(s.rtShadows ? "ON" : "OFF");
        rts.boolValueRef = &s.rtShadows;
        rts.onClick = [](){};
        rts.tooltip = "Ray traced shadows (requires RT enabled)";
        elements.push_back(rts);
        y += rowHeight + rowGap;
        
        // RT Reflections toggle
        UIElement rtrefl;
        rtrefl.x = leftCol; rtrefl.y = y; rtrefl.w = colWidth; rtrefl.h = rowHeight;
        rtrefl.text = "RT Reflections: " + std::string(s.rtReflections ? "ON" : "OFF");
        rtrefl.boolValueRef = &s.rtReflections;
        rtrefl.onClick = [](){};
        rtrefl.tooltip = "Screen-space water reflections (requires RT enabled)";
        elements.push_back(rtrefl);
    }
    
    // === RIGHT COLUMN - Visual ===
    y = startY;
    UIElement visHeader;
    visHeader.x = rightCol;
    visHeader.y = y;
    visHeader.w = colWidth;
    visHeader.h = 25;
    visHeader.text = "-- Visual --";
    visHeader.isLabel = true;
    visHeader.isHeader = true;
    elements.push_back(visHeader);
    y += 35;
    
    // FOV
    UIElement fov;
    fov.x = rightCol; fov.y = y; fov.w = colWidth; fov.h = rowHeight;
    fov.text = "FOV: " + std::to_string((int)s.fov);
    fov.isSlider = true;
    fov.valueRef = &s.fov;
    fov.minVal = 30.0f; fov.maxVal = 110.0f;
    fov.tooltip = "Field of view (30-110 degrees)";
    elements.push_back(fov);
    y += rowHeight + rowGap;
    
    // AO Strength
    UIElement ao;
    ao.x = rightCol; ao.y = y; ao.w = colWidth; ao.h = rowHeight;
    ao.text = "AO Strength: " + std::to_string(s.aoStrength).substr(0, 3);
    ao.isSlider = true;
    ao.valueRef = &s.aoStrength;
    ao.minVal = 0.0f; ao.maxVal = 2.0f;
    ao.tooltip = "Ambient occlusion intensity";
    elements.push_back(ao);
    y += rowHeight + rowGap;
    
    // Gamma
    UIElement gm;
    gm.x = rightCol; gm.y = y; gm.w = colWidth; gm.h = rowHeight;
    gm.text = "Gamma: " + std::to_string(s.gamma).substr(0, 3);
    gm.isSlider = true;
    gm.valueRef = &s.gamma;
    gm.minVal = 1.0f; gm.maxVal = 3.0f;
    gm.tooltip = "Display gamma correction";
    elements.push_back(gm);
    y += rowHeight + rowGap;
    
    // Brightness
    UIElement br;
    br.x = rightCol; br.y = y; br.w = colWidth; br.h = rowHeight;
    br.text = "Brightness: " + std::to_string(s.exposure).substr(0, 3);
    br.isSlider = true;
    br.valueRef = &s.exposure;
    br.minVal = 0.1f; br.maxVal = 5.0f;
    br.tooltip = "Overall scene brightness";
    elements.push_back(br);
    y += rowHeight + rowGap;

    // Color Template
    UIElement ct;
    ct.x = rightCol; ct.y = y; ct.w = colWidth; ct.h = rowHeight;
    ct.text = "Color Template: " + std::string(Settings::COLOR_TEMPLATE_NAMES[s.colorTemplate]);
    ct.intValueRef = &s.colorTemplate;
    ct.onClick = [](){};
    ct.minVal = 0.0f; ct.maxVal = float(Settings::NUM_COLOR_TEMPLATES - 1);
    ct.tooltip = "Cycle color grading templates";
    elements.push_back(ct);
    y += rowHeight + rowGap;

    // Sharpness
    UIElement shp;
    shp.x = rightCol; shp.y = y; shp.w = colWidth; shp.h = rowHeight;
    shp.text = "Sharpness: " + std::to_string(s.sharpness).substr(0, 3);
    shp.isSlider = true;
    shp.valueRef = &s.sharpness;
    shp.minVal = 0.0f; shp.maxVal = 1.0f;
    shp.tooltip = "Sharpen postprocess (0-1)";
    elements.push_back(shp);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - Resource Packs ===
    y += 15;
    UIElement rpHeader;
    rpHeader.x = rightCol;
    rpHeader.y = y;
    rpHeader.w = colWidth;
    rpHeader.h = 25;
    rpHeader.text = "-- Resource Packs --";
    rpHeader.isLabel = true;
    rpHeader.isHeader = true;
    elements.push_back(rpHeader);
    y += 35;
    
    // PBRANDPOM Resource Pack toggle
    UIElement pbr;
    pbr.x = rightCol; pbr.y = y; pbr.w = colWidth; pbr.h = rowHeight;
    pbr.text = "PBRANDPOM: " + std::string(s.usePBRResourcePack ? "ON" : "OFF");
    pbr.boolValueRef = &s.usePBRResourcePack;
    pbr.onClick = [this]() { 
        Settings::instance().pbrSettingsChanged = true; // Trigger shadow recalculation
        setupVideoSettingsMenu(); 
    }; // Refresh to update text
    pbr.tooltip = "Use PBRANDPOM resource pack with PBR textures (triggers shadow update)";
    elements.push_back(pbr);
    y += rowHeight + rowGap;
    
    // Parallax Mapping (3D depth effect)
    UIElement parallax;
    parallax.x = rightCol; parallax.y = y; parallax.w = colWidth; parallax.h = rowHeight;
    parallax.text = "3D Parallax: " + std::string(s.enableParallaxMapping ? "ON" : "OFF");
    parallax.boolValueRef = &s.enableParallaxMapping;
    parallax.onClick = [this]() { setupVideoSettingsMenu(); };
    parallax.tooltip = "Enable parallax occlusion mapping for 3D depth in textures";
    elements.push_back(parallax);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - Celestial ===
    y += 15;
    UIElement celHeader;
    celHeader.x = rightCol;
    celHeader.y = y;
    celHeader.w = colWidth;
    celHeader.h = 25;
    celHeader.text = "-- Celestial --";
    celHeader.isLabel = true;
    celHeader.isHeader = true;
    elements.push_back(celHeader);
    y += 35;
    
    // Sun Size
    UIElement ss;
    ss.x = rightCol; ss.y = y; ss.w = colWidth; ss.h = rowHeight;
    ss.text = "Sun Size: " + std::to_string(s.sunSize).substr(0, 3);
    ss.isSlider = true;
    ss.valueRef = &s.sunSize;
    ss.minVal = 0.5f; ss.maxVal = 10.0f;
    ss.tooltip = "Size of the sun in the sky";
    elements.push_back(ss);
    y += rowHeight + rowGap;
    
    // Moon Size
    UIElement ms;
    ms.x = rightCol; ms.y = y; ms.w = colWidth; ms.h = rowHeight;
    ms.text = "Moon Size: " + std::to_string(s.moonSize).substr(0, 3);
    ms.isSlider = true;
    ms.valueRef = &s.moonSize;
    ms.minVal = 0.5f; ms.maxVal = 10.0f;
    ms.tooltip = "Size of the moon in the sky";
    elements.push_back(ms);
    y += rowHeight + rowGap;
    
    // === RIGHT COLUMN - UI ===
    y += 15;
    UIElement uiHeader;
    uiHeader.x = rightCol;
    uiHeader.y = y;
    uiHeader.w = colWidth;
    uiHeader.h = 25;
    uiHeader.text = "-- Interface --";
    uiHeader.isLabel = true;
    uiHeader.isHeader = true;
    elements.push_back(uiHeader);
    y += 35;
    
    // Tooltips
    UIElement tt;
    tt.x = rightCol; tt.y = y; tt.w = colWidth; tt.h = rowHeight;
    tt.text = "Tooltips: " + std::string(s.enableTooltips ? "ON" : "OFF");
    tt.boolValueRef = &s.enableTooltips;
    tt.onClick = [](){};
    tt.tooltip = "Show helpful tooltips on hover";
    elements.push_back(tt);
    
    // Back button centered at bottom
    UIElement back;
    back.x = width / 2.0f - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::SETTINGS); };
    elements.push_back(back);
    
    // Hardware info labels in bottom-left corner
    auto cpuInfo = HardwareInfo::getCPUInfo();
    auto memInfo = HardwareInfo::getMemoryInfo();
    auto& gpuInfo = HardwareInfo::getGPUInfo();
    
    float infoY = height - 100.0f;
    float infoX = 20.0f;
    float infoH = 18.0f;
    
    UIElement cpuLabel;
    cpuLabel.x = infoX;
    cpuLabel.y = infoY;
    cpuLabel.w = 500.0f;
    cpuLabel.h = infoH;
    cpuLabel.text = "CPU: " + cpuInfo.name;
    cpuLabel.isLabel = true;
    cpuLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(cpuLabel);
    
    UIElement coreLabel;
    coreLabel.x = infoX;
    coreLabel.y = infoY + infoH;
    coreLabel.w = 500.0f;
    coreLabel.h = infoH;
    coreLabel.text = std::to_string(cpuInfo.physicalCores) + " cores / " + 
                     std::to_string(cpuInfo.logicalCores) + " threads | " +
                     std::to_string(memInfo.totalPhysicalMB / 1024) + " GB RAM";
    coreLabel.isLabel = true;
    coreLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(coreLabel);
    
    UIElement gpuLabel;
    gpuLabel.x = infoX;
    gpuLabel.y = infoY + infoH * 2;
    gpuLabel.w = 500.0f;
    gpuLabel.h = infoH;
    gpuLabel.text = "GPU: " + gpuInfo.renderer;
    gpuLabel.isLabel = true;
    gpuLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(gpuLabel);
    
    UIElement glLabel;
    glLabel.x = infoX;
    glLabel.y = infoY + infoH * 3;
    glLabel.w = 500.0f;
    glLabel.h = infoH;
    glLabel.text = "OpenGL: " + gpuInfo.version;
    glLabel.isLabel = true;
    glLabel.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
    elements.push_back(glLabel);
}

std::string getKeyName(int key) {
    if (key > 32 && key <= 126) {
        return std::string(1, (char)key);
    }
    switch (key) {
        case 32: return "SPACE";
        case 256: return "ESC";
        case 257: return "ENTER";
        case 258: return "TAB";
        case 259: return "BACKSPACE";
        case 260: return "INSERT";
        case 261: return "DELETE";
        case 262: return "RIGHT";
        case 263: return "LEFT";
        case 264: return "DOWN";
        case 265: return "UP";
        case 266: return "PAGE UP";
        case 267: return "PAGE DOWN";
        case 268: return "HOME";
        case 269: return "END";
        case 280: return "CAPS LOCK";
        case 281: return "SCROLL LOCK";
        case 282: return "NUM LOCK";
        case 283: return "PRINT SCREEN";
        case 284: return "PAUSE";
        case 290: return "F1";
        case 291: return "F2";
        case 292: return "F3";
        case 293: return "F4";
        case 294: return "F5";
        case 295: return "F6";
        case 296: return "F7";
        case 297: return "F8";
        case 298: return "F9";
        case 299: return "F10";
        case 300: return "F11";
        case 301: return "F12";
        case 340: return "L-SHIFT";
        case 341: return "L-CTRL";
        case 342: return "L-ALT";
        case 343: return "L-SUPER";
        case 344: return "R-SHIFT";
        case 345: return "R-CTRL";
        case 346: return "R-ALT";
        case 347: return "R-SUPER";
        case 348: return "MENU";
        default: return "KEY " + std::to_string(key);
    }
}

void UIManager::setupControlsMenu() {
    elements.clear();
    
    auto& k = Settings::instance().keys;
    
    // Grid layout - 2 columns
    float colWidth = 240.0f;
    float colGap = 60.0f;
    float rowHeight = 38.0f;
    float rowGap = 10.0f;
    float startY = 80.0f;
    float leftCol = width / 2.0f - colWidth - colGap / 2;
    float rightCol = width / 2.0f + colGap / 2;
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 80;
    title.y = 30;
    title.w = 160;
    title.h = 30;
    title.text = "Controls";
    title.isLabel = true;
    elements.push_back(title);
    
    // === LEFT COLUMN - Movement ===
    UIElement moveHeader;
    moveHeader.x = leftCol;
    moveHeader.y = startY;
    moveHeader.w = colWidth;
    moveHeader.h = 25;
    moveHeader.text = "-- Movement --";
    moveHeader.isLabel = true;
    moveHeader.isHeader = true;
    elements.push_back(moveHeader);
    
    float y = startY + 35;
    
    auto addKeyBtn = [&](float x, float& yPos, const std::string& label, int* keyRef, const std::string& tip) {
        UIElement el;
        el.x = x;
        el.y = yPos;
        el.w = colWidth;
        el.h = rowHeight;
        el.text = label + ": " + getKeyName(*keyRef);
        el.isKeybind = true;
        el.keyBindRef = keyRef;
        el.tooltip = tip;
        elements.push_back(el);
        yPos += rowHeight + rowGap;
    };
    
    addKeyBtn(leftCol, y, "Forward", &k.forward, "Move forward");
    addKeyBtn(leftCol, y, "Backward", &k.backward, "Move backward");
    addKeyBtn(leftCol, y, "Strafe Left", &k.left, "Move left");
    addKeyBtn(leftCol, y, "Strafe Right", &k.right, "Move right");
    addKeyBtn(leftCol, y, "Jump", &k.jump, "Jump / Fly up");
    addKeyBtn(leftCol, y, "Sprint", &k.sprint, "Hold to run faster");
    addKeyBtn(leftCol, y, "Sneak", &k.sneak, "Sneak / Fly down");
    
    // === RIGHT COLUMN - Actions ===
    y = startY;
    UIElement actHeader;
    actHeader.x = rightCol;
    actHeader.y = y;
    actHeader.w = colWidth;
    actHeader.h = 25;
    actHeader.text = "-- Actions --";
    actHeader.isLabel = true;
    actHeader.isHeader = true;
    elements.push_back(actHeader);
    y += 35;
    
    addKeyBtn(rightCol, y, "Inventory", &k.inventory, "Open inventory (E)");
    
    // Info labels
    y += 20;
    UIElement info1;
    info1.x = rightCol; info1.y = y; info1.w = colWidth; info1.h = 25;
    info1.text = "Mouse: Look around";
    info1.isLabel = true;
    elements.push_back(info1);
    y += 30;
    
    UIElement info2;
    info2.x = rightCol; info2.y = y; info2.w = colWidth; info2.h = 25;
    info2.text = "LMB: Break block";
    info2.isLabel = true;
    elements.push_back(info2);
    y += 30;
    
    UIElement info3;
    info3.x = rightCol; info3.y = y; info3.w = colWidth; info3.h = 25;
    info3.text = "RMB: Place block";
    info3.isLabel = true;
    elements.push_back(info3);
    y += 30;
    
    UIElement info4;
    info4.x = rightCol; info4.y = y; info4.w = colWidth; info4.h = 25;
    info4.text = "1-9: Select hotbar";
    info4.isLabel = true;
    elements.push_back(info4);
    y += 30;
    
    UIElement info5;
    info5.x = rightCol; info5.y = y; info5.w = colWidth; info5.h = 25;
    info5.text = "Scroll: Cycle hotbar";
    info5.isLabel = true;
    elements.push_back(info5);
    y += 30;
    
    UIElement info6;
    info6.x = rightCol; info6.y = y; info6.w = colWidth; info6.h = 25;
    info6.text = "F1: Toggle debug";
    info6.isLabel = true;
    elements.push_back(info6);
    y += 30;
    
    UIElement info7;
    info7.x = rightCol; info7.y = y; info7.w = colWidth; info7.h = 25;
    info7.text = "M: Open map";
    info7.isLabel = true;
    elements.push_back(info7);
    y += 30;
    
    UIElement info8;
    info8.x = rightCol; info8.y = y; info8.w = colWidth; info8.h = 25;
    info8.text = "~: Toggle console";
    info8.isLabel = true;
    elements.push_back(info8);
    
    // Instruction
    UIElement inst;
    inst.x = width / 2.0f - 150;
    inst.y = height - 130;
    inst.w = 300;
    inst.h = 25;
    inst.text = "Click a key to rebind";
    inst.isLabel = true;
    inst.customColor = glm::vec4(0.7f, 0.7f, 0.5f, 1.0f);
    elements.push_back(inst);
    
    // Back button
    UIElement back;
    back.x = width / 2.0f - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::SETTINGS); };
    elements.push_back(back);
}

void UIManager::setupLoadGameMenu() {
    elements.clear();
    
    // Clear old preview textures and reload them
    clearWorldPreviewTextures();
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 100;
    title.y = 40;
    title.w = 200;
    title.h = 35;
    title.text = "Load World";
    title.isLabel = true;
    elements.push_back(title);
    
    std::vector<std::string> worlds = WorldSerializer::getAvailableWorlds();
    
    // World cards layout - larger to accommodate thumbnail
    float cardW = 400.0f;
    float cardH = 80.0f;
    float cardGap = 15.0f;
    float startY = 100.0f;
    float centerX = width / 2.0f;
    float thumbW = 100.0f;  // Thumbnail width
    float thumbH = 56.0f;   // Thumbnail height (16:9 aspect)
    float thumbPadding = 12.0f;
    
    // Scrollable area hint if many worlds
    if (worlds.empty()) {
        UIElement noWorlds;
        noWorlds.x = centerX - 150;
        noWorlds.y = height / 2.0f - 30;
        noWorlds.w = 300;
        noWorlds.h = 30;
        noWorlds.text = "No saved worlds found";
        noWorlds.isLabel = true;
        noWorlds.customColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        elements.push_back(noWorlds);
    } else {
        for (size_t i = 0; i < worlds.size() && i < 6; i++) { // Max 6 visible
            std::string wName = worlds[i];
            
            // Try to load preview texture
            GLuint previewTex = loadWorldPreviewTexture(wName);
            
            // World card (acts like a big button)
            UIElement card;
            card.x = centerX - cardW / 2;
            card.y = startY + i * (cardH + cardGap);
            card.w = cardW;
            card.h = cardH;
            card.text = wName;
            card.isCard = true;
            card.tooltip = "Click to load \"" + wName + "\"";
            
            // Set thumbnail info if preview exists
            if (previewTex != 0) {
                card.textureId = previewTex;
                card.thumbnailX = card.x + thumbPadding;
                card.thumbnailY = card.y + (cardH - thumbH) / 2.0f;
                card.thumbnailW = thumbW;
                card.thumbnailH = thumbH;
            }
            
            card.onClick = [this, wName]() { 
                if (onLoadGame) onLoadGame(wName);
                setMenuState(MenuState::NONE);
            };
            elements.push_back(card);
        }
        
        if (worlds.size() > 6) {
            UIElement more;
            more.x = centerX - 100;
            more.y = startY + 6 * (cardH + cardGap);
            more.w = 200;
            more.h = 25;
            more.text = "+" + std::to_string(worlds.size() - 6) + " more worlds...";
            more.isLabel = true;
            more.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            elements.push_back(more);
        }
    }
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupDeathScreen() {
    elements.clear();
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 50.0f;
    float gap = 15.0f;
    
    // "You Died!" title - will be rendered specially in render()
    UIElement title;
    title.x = centerX - 100;
    title.y = centerY - 120;
    title.w = 200;
    title.h = 50;
    title.text = "YOU DIED";
    title.isLabel = true;
    title.customColor = glm::vec4(0.8f, 0.1f, 0.1f, 1.0f); // Dark red
    elements.push_back(title);
    
    // Respawn button
    elements.push_back({centerX - btnW/2, centerY, btnW, btnH, "RESPAWN", false, [this]() { 
        if (onRespawn) onRespawn();
        setMenuState(MenuState::NONE);
    }});
    
    // Main Menu button
    elements.push_back({centerX - btnW/2, centerY + btnH + gap, btnW, btnH, "MAIN MENU", false, [this]() { 
        worldLoaded = false;
        if (onReturnToMainMenu) onReturnToMainMenu();
        setMenuState(MenuState::MAIN_MENU);
    }});
}

void UIManager::setupNewGameMenu() {
    elements.clear();
    
    // Title
    UIElement title;
    title.x = width / 2.0f - 120;
    title.y = 50;
    title.w = 240;
    title.h = 35;
    title.text = "Singleplayer";
    title.isLabel = true;
    elements.push_back(title);
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f - 20;
    float inputW = 320.0f;
    float inputH = 45.0f;
    float gap = 20.0f;
    
    // Load World bar at top (above input fields)
    UIElement loadWorldBtn;
    loadWorldBtn.x = centerX - inputW / 2;
    loadWorldBtn.y = 100;
    loadWorldBtn.w = inputW;
    loadWorldBtn.h = 50;
    loadWorldBtn.text = "Load Existing World";
    loadWorldBtn.tooltip = "Load a previously saved world";
    loadWorldBtn.onClick = [this]() { 
        setMenuState(MenuState::LOAD_GAME);
    };
    elements.push_back(loadWorldBtn);
    
    // Divider label
    UIElement dividerLabel;
    dividerLabel.x = centerX - 100;
    dividerLabel.y = 165;
    dividerLabel.w = 200;
    dividerLabel.h = 20;
    dividerLabel.text = "-- Or Create New --";
    dividerLabel.isLabel = true;
    dividerLabel.customColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
    elements.push_back(dividerLabel);
    
    // Seed constraints
    constexpr long MAX_SEED = 999999999L;
    constexpr long MIN_SEED = 1L;

    // Static inputs to preserve values
    static std::string nameInput = "New World";
    static std::string seedInput = "";
    if (seedInput.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<long> dis(MIN_SEED, MAX_SEED);
        seedInput = std::to_string(dis(gen));
    }
    
    // World Name Label
    UIElement nameLabel;
    nameLabel.x = centerX - inputW / 2;
    nameLabel.y = centerY - 60;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "World Name:";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);

    // World Name Input
    UIElement nameField;
    nameField.x = centerX - inputW / 2;
    nameField.y = centerY - 35;
    nameField.w = inputW;
    nameField.h = inputH;
    nameField.text = nameInput;
    nameField.isInput = true;
    nameField.textRef = &nameInput;
    nameField.tooltip = "Enter a name for your world";
    elements.push_back(nameField);

    // Seed Label
    UIElement seedLabel;
    seedLabel.x = centerX - inputW / 2;
    seedLabel.y = centerY + 30;
    seedLabel.w = inputW;
    seedLabel.h = 20;
    seedLabel.text = "World Seed:";
    seedLabel.isLabel = true;
    elements.push_back(seedLabel);
    
    // Seed Input
    UIElement seedField;
    seedField.x = centerX - inputW / 2;
    seedField.y = centerY + 55;
    seedField.w = inputW;
    seedField.h = inputH;
    seedField.text = seedInput;
    seedField.isInput = true;
    seedField.textRef = &seedInput;
    seedField.tooltip = "Numeric seed (1 - 999,999,999)";
    elements.push_back(seedField);
    
    // Seed hint
    UIElement seedHint;
    seedHint.x = centerX - inputW / 2;
    seedHint.y = centerY + 105;
    seedHint.w = inputW;
    seedHint.h = 18;
    seedHint.text = "Leave empty or use numbers only";
    seedHint.isLabel = true;
    seedHint.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    elements.push_back(seedHint);

    // Create World Button
    UIElement createBtn;
    createBtn.x = centerX - inputW / 2;
    createBtn.y = centerY + 140;
    createBtn.w = inputW;
    createBtn.h = 50;
    createBtn.text = "Create World";
    createBtn.tooltip = "Generate and enter a new world";
    createBtn.onClick = [this]() { 
        long seed = 0;
        // Element indices: title=0, loadWorldBtn=1, dividerLabel=2, nameLabel=3, nameField=4, seedLabel=5, seedField=6
        std::string seedStr = *elements[6].textRef; // seedField is element 6
        
        // Filter non-numeric characters
        std::string cleanSeed;
        for (char c : seedStr) {
            if (c >= '0' && c <= '9') cleanSeed += c;
        }
        
        try { 
            if (!cleanSeed.empty()) {
                seed = std::stol(cleanSeed);
            }
        } catch(...) { 
            seed = 0;
        }
        
        constexpr long MAX_SEED = 999999999L;
        constexpr long MIN_SEED = 1L;
        
        if (seed < MIN_SEED || seed > MAX_SEED) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<long> dis(MIN_SEED, MAX_SEED);
            seed = dis(gen);
        }
        
        if (onNewGame) onNewGame(*elements[4].textRef, seed); // nameField is element 4
        setMenuState(MenuState::NONE);
    };
    elements.push_back(createBtn);

    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupInventoryMenu() {
    elements.clear();
    
    // Inventory Grid similar to Minecraft - with tabs for Blocks and Tools
    float slotSize = 52.0f;
    float gap = 6.0f;
    int cols = 9;
    
    // All available blocks (excluding AIR) - organized by category
    std::vector<BlockType> allBlocks = {
        // Natural terrain
        BlockType::GRASS, BlockType::DIRT,
        BlockType::STONE, BlockType::COBBLESTONE, BlockType::MOSSY_COBBLESTONE,
        BlockType::SAND, BlockType::GRAVEL, BlockType::CLAY,
        
        // Ores
        BlockType::COAL_ORE, BlockType::IRON_ORE, BlockType::GOLD_ORE, BlockType::DIAMOND_ORE,
        BlockType::EMERALD_ORE, BlockType::REDSTONE_ORE, BlockType::LAPIS_ORE,
        
        // Mineral blocks
        BlockType::IRON_BLOCK, BlockType::GOLD_BLOCK, BlockType::DIAMOND_BLOCK,
        BlockType::EMERALD_BLOCK, BlockType::REDSTONE_BLOCK,
        
        // Stone variants
        BlockType::STONE_BRICKS, BlockType::MOSSY_STONE_BRICKS, BlockType::CRACKED_STONE_BRICKS,
        BlockType::CHISELED_STONE_BRICKS,
        
        // Sandstone
        BlockType::SANDSTONE, BlockType::CHISELED_SANDSTONE,
        
        // Building blocks
        BlockType::BRICKS, BlockType::OBSIDIAN,
        BlockType::BOOKSHELF, BlockType::TNT, BlockType::CRAFTING_TABLE,
        
        // Light sources
        BlockType::GLOWSTONE, BlockType::REDSTONE_LAMP,
        
        // Glass
        BlockType::GLASS,
        
        // Wool
        BlockType::WHITE_WOOL, BlockType::ORANGE_WOOL, BlockType::MAGENTA_WOOL,
        BlockType::LIGHT_BLUE_WOOL, BlockType::YELLOW_WOOL, BlockType::LIME_WOOL,
        BlockType::PINK_WOOL, BlockType::GRAY_WOOL, BlockType::LIGHT_GRAY_WOOL,
        BlockType::CYAN_WOOL, BlockType::PURPLE_WOOL, BlockType::BLUE_WOOL,
        BlockType::BROWN_WOOL, BlockType::GREEN_WOOL, BlockType::RED_WOOL, BlockType::BLACK_WOOL,
        
        // Wood - Planks
        BlockType::WOOD, BlockType::OAK_PLANKS, BlockType::SPRUCE_PLANKS, BlockType::BIRCH_PLANKS,
        BlockType::JUNGLE_PLANKS,
        
        // Wood - Logs
        BlockType::LOG, BlockType::OAK_LOG, BlockType::SPRUCE_LOG, BlockType::BIRCH_LOG,
        BlockType::JUNGLE_LOG,
        
        // Leaves
        BlockType::LEAVES, BlockType::OAK_LEAVES, BlockType::SPRUCE_LEAVES, BlockType::BIRCH_LEAVES,
        BlockType::JUNGLE_LEAVES,
        
        // Nature
        BlockType::TALL_GRASS, BlockType::ROSE, BlockType::COBWEB, BlockType::SUGAR_CANE,
        
        // Ice variants
        BlockType::SNOW, BlockType::ICE,
        
        // Water
        BlockType::WATER,
        
        // Misc
        BlockType::SPONGE, BlockType::NOTE_BLOCK, BlockType::JUKEBOX,
        BlockType::FARMLAND,
        
        // Bedrock (last - creative only)
        BlockType::BEDROCK
    };
    
    // All available tools
    std::vector<ItemType> allTools = {
        ItemType::SWORD_WOOD, ItemType::PICKAXE_WOOD, ItemType::AXE_WOOD, ItemType::SHOVEL_WOOD,
        ItemType::SWORD_STONE, ItemType::PICKAXE_STONE, ItemType::AXE_STONE, ItemType::SHOVEL_STONE,
        ItemType::SWORD_GOLD, ItemType::PICKAXE_GOLD, ItemType::AXE_GOLD, ItemType::SHOVEL_GOLD,
        ItemType::SWORD_DIAMOND, ItemType::PICKAXE_DIAMOND, ItemType::AXE_DIAMOND, ItemType::SHOVEL_DIAMOND
    };
    
    float totalW = cols * slotSize + (cols - 1) * gap;
    float startX = (width - totalW) / 2.0f;
    float baseY = 100.0f;
    
    // ========================================
    // INVENTORY TITLE
    // ========================================
    UIElement titleEl;
    titleEl.x = width / 2.0f - 100;
    titleEl.y = baseY;
    titleEl.w = 200;
    titleEl.h = 30;
    titleEl.text = "INVENTORY";
    titleEl.isLabel = true;
    elements.push_back(titleEl);
    
    // ========================================
    // SEARCH BAR
    // ========================================
    float searchY = baseY + 45;
    float searchW = 280.0f;
    float searchX = width / 2.0f - searchW / 2.0f;
    
    UIElement searchEl;
    searchEl.x = searchX;
    searchEl.y = searchY;
    searchEl.w = searchW;
    searchEl.h = 30;
    searchEl.text = inventorySearch.empty() ? "Search..." : inventorySearch;
    searchEl.isInput = true;
    searchEl.textRef = &inventorySearch;
    elements.push_back(searchEl);
    
    // ========================================
    // CATEGORY TABS
    // ========================================
    float tabY = searchY + 45;
    float tabW = 120.0f;
    float tabH = 28.0f;
    float tabGap = 10.0f;
    float tabsStartX = width / 2.0f - (tabW * 2 + tabGap) / 2.0f;
    
    // Blocks Tab
    UIElement blocksTab;
    blocksTab.x = tabsStartX;
    blocksTab.y = tabY;
    blocksTab.w = tabW;
    blocksTab.h = tabH;
    blocksTab.text = "BLOCKS";
    blocksTab.customColor = (inventoryTab == 0) ? glm::vec4(0.3f, 0.6f, 0.3f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    blocksTab.onClick = [this]() { inventoryTab = 0; setupInventoryMenu(); };
    elements.push_back(blocksTab);
    
    // Tools Tab
    UIElement toolsTab;
    toolsTab.x = tabsStartX + tabW + tabGap;
    toolsTab.y = tabY;
    toolsTab.w = tabW;
    toolsTab.h = tabH;
    toolsTab.text = "TOOLS";
    toolsTab.customColor = (inventoryTab == 1) ? glm::vec4(0.3f, 0.6f, 0.3f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    toolsTab.onClick = [this]() { inventoryTab = 1; setupInventoryMenu(); };
    elements.push_back(toolsTab);
    
    // ========================================
    // ITEMS GRID
    // ========================================
    float gridY = tabY + tabH + 20;
    int itemIndex = 0;
    
    // Convert search to lowercase for comparison
    std::string searchLower = inventorySearch;
    for (auto& c : searchLower) c = std::tolower(c);
    
    if (inventoryTab == 0) {
        // BLOCKS TAB
        for (size_t i = 0; i < allBlocks.size(); i++) {
            BlockType type = allBlocks[i];
            
            // Filter by search
            if (!inventorySearch.empty()) {
                std::string blockName = getBlockName(type);
                std::string nameLower = blockName;
                for (auto& c : nameLower) c = std::tolower(c);
                if (nameLower.find(searchLower) == std::string::npos) {
                    continue;
                }
            }
            
            int col = itemIndex % cols;
            int row = itemIndex / cols;
            
            float x = startX + col * (slotSize + gap);
            float y = gridY + row * (slotSize + gap);
            
            UIElement el;
            el.x = x;
            el.y = y;
            el.w = slotSize;
            el.h = slotSize;
            el.text = "";
            el.isInventoryItem = true;
            el.isInventoryTool = false;
            el.blockType = type;
            el.inventoryIndex = itemIndex;
            el.tooltip = getBlockName(type);
            el.onClick = [this, type]() {
                // Left click - assign block to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = type;  // Keep legacy array in sync
            };
            el.onRightClick = [this, type]() {
                // Right click - also assign block to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = type;  // Keep legacy array in sync
            };
            elements.push_back(el);
            itemIndex++;
        }
    } else {
        // TOOLS TAB
        for (size_t i = 0; i < allTools.size(); i++) {
            ItemType type = allTools[i];
            
            // Filter by search
            if (!inventorySearch.empty()) {
                std::string toolName = getItemName(type);
                std::string nameLower = toolName;
                for (auto& c : nameLower) c = std::tolower(c);
                if (nameLower.find(searchLower) == std::string::npos) {
                    continue;
                }
            }
            
            int col = itemIndex % cols;
            int row = itemIndex / cols;
            
            float x = startX + col * (slotSize + gap);
            float y = gridY + row * (slotSize + gap);
            
            UIElement el;
            el.x = x;
            el.y = y;
            el.w = slotSize;
            el.h = slotSize;
            el.text = "";
            el.isInventoryItem = true;
            el.isInventoryTool = true;
            el.itemType = type;
            el.inventoryIndex = itemIndex;
            el.tooltip = getItemName(type);
            el.onClick = [this, type]() {
                // Left click - assign tool to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = BlockType::AIR;  // Clear legacy array since it's now a tool
            };
            el.onRightClick = [this, type]() {
                // Right click - also assign tool to current hotbar slot
                hotbarSlots[selectedSlot] = HotbarSlot(type);
                hotbar[selectedSlot] = BlockType::AIR;  // Clear legacy array since it's now a tool
            };
            elements.push_back(el);
            itemIndex++;
        }
    }
    
    // ========================================
    // CURRENT HOTBAR DISPLAY
    // ========================================
    float hotbarDisplayY = height - 160.0f;
    float hotbarLabelY = hotbarDisplayY - 30.0f;
    
    UIElement hotbarLabel;
    hotbarLabel.x = width / 2.0f - 80;
    hotbarLabel.y = hotbarLabelY;
    hotbarLabel.w = 160;
    hotbarLabel.h = 20;
    hotbarLabel.text = "HOTBAR";
    hotbarLabel.isLabel = true;
    elements.push_back(hotbarLabel);
    
    float hotbarSlotSize = 48.0f;
    float hotbarGap = 4.0f;
    float hotbarTotalW = 9 * hotbarSlotSize + 8 * hotbarGap;
    float hotbarStartX = (width - hotbarTotalW) / 2.0f;
    
    for (int i = 0; i < 9; i++) {
        const HotbarSlot& hSlot = hotbarSlots[i];
        
        UIElement slot;
        slot.x = hotbarStartX + i * (hotbarSlotSize + hotbarGap);
        slot.y = hotbarDisplayY;
        slot.w = hotbarSlotSize;
        slot.h = hotbarSlotSize;
        slot.text = std::to_string(i + 1);
        slot.isInventoryItem = true;
        slot.inventoryIndex = 100 + i;  // Use 100+ for hotbar slots to differentiate
        
        // Set the slot content based on hotbarSlots
        if (hSlot.isItem) {
            slot.isInventoryTool = true;
            slot.itemType = hSlot.itemStack.type;
            slot.blockType = BlockType::AIR;
        } else {
            slot.isInventoryTool = false;
            slot.blockType = hSlot.blockType;
            slot.itemType = ItemType::NONE;
        }
        
        slot.onClick = [this, i]() {
            selectedSlot = i;
        };
        slot.onRightClick = [this, i]() {
            // Clear slot
            hotbarSlots[i] = HotbarSlot(BlockType::AIR);
            hotbar[i] = BlockType::AIR;
        };
        elements.push_back(slot);
    }
    
    // ========================================
    // CLOSE INSTRUCTION
    // ========================================
    UIElement closeEl;
    closeEl.x = width / 2.0f - 100;
    closeEl.y = height - 60.0f;
    closeEl.w = 200;
    closeEl.h = 30;
    closeEl.text = "PRESS [E] TO CLOSE";
    closeEl.isLabel = true;
    elements.push_back(closeEl);
}

glm::vec4 UIManager::getBlockColor(BlockType type) {
    switch (type) {
        case BlockType::GRASS: return glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);
        case BlockType::DIRT: return glm::vec4(0.5f, 0.3f, 0.1f, 1.0f);
        case BlockType::STONE: return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        case BlockType::SAND: return glm::vec4(0.9f, 0.8f, 0.5f, 1.0f);
        case BlockType::WOOD: return glm::vec4(0.6f, 0.4f, 0.2f, 1.0f);
        case BlockType::LEAVES: return glm::vec4(0.1f, 0.5f, 0.1f, 1.0f);
        case BlockType::SNOW: return glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        case BlockType::ICE: return glm::vec4(0.6f, 0.8f, 1.0f, 0.8f);
        case BlockType::WATER: return glm::vec4(0.2f, 0.4f, 0.8f, 0.6f);
        case BlockType::GRAVEL: return glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        case BlockType::SANDSTONE: return glm::vec4(0.8f, 0.7f, 0.5f, 1.0f);
        case BlockType::LOG: return glm::vec4(0.4f, 0.3f, 0.1f, 1.0f);
        case BlockType::TALL_GRASS: return glm::vec4(0.2f, 0.6f, 0.2f, 1.0f);
        case BlockType::ROSE: return glm::vec4(0.9f, 0.1f, 0.1f, 1.0f);
        case BlockType::BEDROCK: return glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        default: return glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); 
    }
}

void UIManager::update(float deltaTime, double mouseX, double mouseY, bool mousePressed, bool rightMousePressed) {
    // Update preview model animation and rotation when in player settings
    if (currentMenuState == MenuState::PLAYER_SETTINGS && previewModel) {
        previewModel->updateAnimation(deltaTime);
        
        // Handle drag rotation with momentum - use LEFT mouse but only if not over UI
        // Check if mouse is over any UI element
        bool overUI = false;
        for (const auto& el : elements) {
            if (mouseX >= el.x && mouseX <= el.x + el.w &&
                mouseY >= el.y && mouseY <= el.y + el.h) {
                overUI = true;
                break;
            }
        }
        
        if (mousePressed && !overUI) {
            if (!isDraggingModel) {
                // Start dragging
                isDraggingModel = true;
                lastDragX = (float)mouseX;
                previewRotationVelocity = 0.0f;  // Stop any existing momentum
            } else {
                // Continue dragging - calculate velocity from mouse movement
                float dragDelta = (float)mouseX - lastDragX;
                previewRotationVelocity = dragDelta * 2.0f;  // Sensitivity multiplier
                previewRotation += previewRotationVelocity * deltaTime * 60.0f;
                lastDragX = (float)mouseX;
            }
        } else {
            if (isDraggingModel && mousePressed) {
                // Still holding mouse but moved over UI - keep momentum
                isDraggingModel = false;
            } else if (!mousePressed) {
                isDraggingModel = false;
            }
            
            // Apply deceleration (momentum decay)
            previewRotation += previewRotationVelocity * deltaTime * 60.0f;
            previewRotationVelocity *= 0.95f;  // Decay factor - adjust for faster/slower stop
            
            // Stop when velocity is very small
            if (std::abs(previewRotationVelocity) < 0.01f) {
                previewRotationVelocity = 0.0f;
            }
        }
        
        // Keep rotation in reasonable range
        while (previewRotation > 360.0f) previewRotation -= 360.0f;
        while (previewRotation < 0.0f) previewRotation += 360.0f;
    }
    
    // Always update HUD animations (hover, selection bounce, etc.)
    updateHUDAnimations(deltaTime, mouseX, mouseY);
    
    if (!isMenuOpen()) {
        lastMousePressed = mousePressed;
        currentTooltip.clear();
        tooltipTimer = 0.0f;
        return;
    }

    std::function<void()> pendingClick = nullptr;

    // Block clicks if waiting for keybind
    if (waitingForKeyBind) return;
    
    // Track tooltip
    std::string hoveredTooltip;
    bool anyHovered = false;

    for (auto& el : elements) {
        // Hit test
        if (mouseX >= el.x && mouseX <= el.x + el.w &&
            mouseY >= el.y && mouseY <= el.y + el.h) {
            
            el.isHovered = true;
            anyHovered = true;
            
            // Track tooltip for hovered element
            if (!el.tooltip.empty() && Settings::instance().enableTooltips) {
                if (hoveredTooltip.empty()) {
                    hoveredTooltip = el.tooltip;
                    tooltipX = static_cast<float>(mouseX) + 15.0f;
                    tooltipY = static_cast<float>(mouseY) + 15.0f;
                }
            }
            
            if (mousePressed) {
                if (el.isSlider) {
                    // Calculate slider value
                    float pct = (float)(mouseX - el.x) / el.w;
                    float val = el.minVal + pct * (el.maxVal - el.minVal);
                    val = std::max(el.minVal, std::min(el.maxVal, val));
                    
                    if (el.intValueRef) {
                        *el.intValueRef = (int)val;
                        // Update text based on which slider it is
                        if (el.text.find("Render Distance") != std::string::npos)
                            el.text = "Render Distance: " + std::to_string(*el.intValueRef);
                        else if (el.text.find("Master") != std::string::npos)
                            el.text = "Master: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("Music") != std::string::npos)
                            el.text = "Music: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("SFX") != std::string::npos)
                            el.text = "SFX: " + std::to_string(*el.intValueRef) + "%";
                        else if (el.text.find("Ambient") != std::string::npos)
                            el.text = "Ambient: " + std::to_string(*el.intValueRef) + "%";
                    } else if (el.valueRef) {
                        *el.valueRef = val;
                        // Update text
                        if (el.text.find("FOV") != std::string::npos)
                            el.text = "FOV: " + std::to_string((int)*el.valueRef);
                        else if (el.text.find("SENSITIVITY") != std::string::npos)
                            el.text = "SENSITIVITY: " + std::to_string(*el.valueRef).substr(0, 4);
                        else if (el.text.find("AO") != std::string::npos)
                            el.text = "AO STRENGTH: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("GAMMA") != std::string::npos)
                            el.text = "GAMMA: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("BRIGHTNESS") != std::string::npos)
                            el.text = "BRIGHTNESS: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("Sharpness") != std::string::npos)
                            el.text = "Sharpness: " + std::to_string(*el.valueRef).substr(0, 3);
                        else if (el.text.find("Shadow Distance") != std::string::npos)
                            el.text = "Shadow Distance: " + std::to_string((int)*el.valueRef);
                        else if (el.text.find("Master Volume") != std::string::npos) {
                            el.text = "Master Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setMasterVolume(*el.valueRef);
                        }
                        else if (el.text.find("Music Volume") != std::string::npos) {
                            el.text = "Music Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, *el.valueRef);
                        }
                        else if (el.text.find("Sound Effects") != std::string::npos) {
                            el.text = "Sound Effects: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, *el.valueRef);
                        }
                        else if (el.text.find("Ambient Volume") != std::string::npos) {
                            el.text = "Ambient Volume: " + std::to_string(static_cast<int>(*el.valueRef * 100)) + "%";
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, *el.valueRef);
                            Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::WEATHER, *el.valueRef);
                        }
                    }
                    
                    if (onSettingsChanged) onSettingsChanged();
                } else if (!lastMousePressed) {
                    // Button clicks (Rising Edge)
                    // Play UI click sound for all button interactions
                    Audio::AudioManager::instance().playSound(Audio::SoundType::UI_CLICK, 0.5f);
                    
                    if (el.isKeybind) {
                        waitingForKeyBind = true;
                        keyBindPtr = el.keyBindRef;
                        el.text = "PRESS ANY KEY...";
                    } else if (el.onClick) {
                        if (el.boolValueRef) {
                            *el.boolValueRef = !(*el.boolValueRef);
                            // Update text for toggle
                            size_t colonPos = el.text.find(":");
                            if (colonPos != std::string::npos) {
                                std::string prefix = el.text.substr(0, colonPos + 1);
                                el.text = prefix + (*el.boolValueRef ? " ON" : " OFF");
                            }
                            if (onSettingsChanged) onSettingsChanged();
                        } else if (el.intValueRef) {
                            // Cycle integer value
                            *el.intValueRef = (*el.intValueRef + 1);
                            if (*el.intValueRef > (int)el.maxVal) *el.intValueRef = (int)el.minVal;
                            
                            if (el.text.find("WINDOW MODE") != std::string::npos) {
                                std::string modeStr;
                                if (*el.intValueRef == 0) modeStr = "WINDOWED";
                                else if (*el.intValueRef == 1) modeStr = "FULLSCREEN";
                                else if (*el.intValueRef == 2) modeStr = "BORDERLESS";
                                el.text = "WINDOW MODE: " + modeStr;
                            }
                            
                            if (el.text.find("AA METHOD") != std::string::npos) {
                                el.text = "AA METHOD: " + std::string(Settings::AA_METHOD_NAMES[*el.intValueRef]);
                            }
                            
                            if (el.text.find("RT Quality") != std::string::npos) {
                                el.text = "RT Quality: " + std::string(Settings::RT_QUALITY_NAMES[*el.intValueRef]);
                            }

                            if (el.text.find("Color Template") != std::string::npos) {
                                Settings::instance().applyColorTemplate(*el.intValueRef);
                                el.text = "Color Template: " + std::string(Settings::COLOR_TEMPLATE_NAMES[*el.intValueRef]);
                                pendingClick = [this]() {
                                    this->setupVideoSettingsMenu();
                                    if (onSettingsChanged) onSettingsChanged();
                                };
                                break;
                            }
                            
                            if (el.text.find("Shadows:") != std::string::npos && el.text.find("RT") == std::string::npos) {
                                el.text = "Shadows: " + std::string(Settings::SHADOW_METHOD_NAMES[*el.intValueRef]);
                            }
                            
                            // Handle preset selector - apply the preset after cycling and refresh UI
                            if (el.intValueRef == &Settings::instance().graphicsPreset) {
                                Settings::instance().applyPreset(*el.intValueRef);
                                // Refresh video settings UI to show updated values
                                pendingClick = [this]() {
                                    this->setupVideoSettingsMenu();
                                    if (onSettingsChanged) onSettingsChanged();
                                };
                                break; // Exit early since we rebuild after loop
                            }
                            
                            if (onSettingsChanged) onSettingsChanged();
                        } else {
                            pendingClick = el.onClick;
                            break; // Stop processing to avoid issues with vector modification
                        }
                    }
                }
            }
        } else {
            el.isHovered = false;
        }
    }


    
    // Right Click Handling
    if (rightMousePressed && !lastRightMousePressed) {
        for (auto& el : elements) {
            if (el.isHovered && el.isInventoryItem && el.onRightClick) {
                el.onRightClick();
            }
        }
    }
    
    // Map click handling - teleport on left click
    if (currentMenuState == MenuState::MAP && mousePressed && !lastMousePressed && !elements.empty()) {
        const auto& mapEl = elements[0];
        
        // Check if click is on the map
        if (mouseX >= mapEl.x && mouseX <= mapEl.x + mapEl.w &&
            mouseY >= mapEl.y && mouseY <= mapEl.y + mapEl.h) {
            
            // Convert screen coords to map coords
            float relX = (float)(mouseX - mapEl.x) / (float)mapEl.w; // 0 to 1
            float relY = (float)(mouseY - mapEl.y) / (float)mapEl.h; // 0 to 1
            
            // Convert to world coords
            float worldX = mapCenterX + (relX - 0.5f) * mapTextureSize * mapScale;
            float worldZ = mapCenterZ + (relY - 0.5f) * mapTextureSize * mapScale;
            
            // Teleport
            if (onTeleport) {
                onTeleport(worldX, worldZ);
                setMenuState(MenuState::NONE); // Close map after teleport
            }
        }
    }

    if (pendingClick) {
        pendingClick();
    }
    
    // Update tooltip state
    if (!hoveredTooltip.empty()) {
        if (currentTooltip == hoveredTooltip) {
            tooltipTimer += deltaTime;
        } else {
            currentTooltip = hoveredTooltip;
            tooltipTimer = 0.0f;
        }
    } else {
        currentTooltip.clear();
        tooltipTimer = 0.0f;
    }
    
    lastMousePressed = mousePressed;
    lastRightMousePressed = rightMousePressed;
}

void UIManager::render() {
    // Menu and Debug rendering
    if (isMenuOpen() || showDebug) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        uiShader.use();
        glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
        uiShader.setMat4("uProjection", projection);

        if (isMenuOpen()) {
            // Draw semi-transparent background
            drawRect(0, 0, (float)width, (float)height, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
            
            // Draw game title for main menu
            if (currentMenuState == MenuState::MAIN_MENU) {
                // Title: C++craft with stylish colors
                std::string title = "Bettercraft";
                float titleScale = 6.0f;
                float titleW = title.length() * 6.0f * titleScale;
                float titleX = (width - titleW) / 2.0f;
                float titleY = height * 0.15f;
                
                // Draw shadow
                drawText(titleX + 4, titleY + 4, titleScale, title, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
                // Draw main title with gradient-like effect (green for C++, white for craft)
                drawText(titleX, titleY, titleScale, title, glm::vec4(0.4f, 0.9f, 0.4f, 1.0f));
                
                // Subtitle
                std::string subtitle = "A CPP Voxel Engine Game";
                float subScale = 2.0f;
                float subW = subtitle.length() * 6.0f * subScale;
                float subX = (width - subW) / 2.0f;
                drawText(subX, titleY + titleScale * 12.0f, subScale, subtitle, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

                // Main menu tip (persists for session)
                if (!mainMenuTip.empty()) {
                    float tipScale = 1.6f;
                    float tipW = mainMenuTip.length() * 6.0f * tipScale;
                    float tipX = (width - tipW) / 2.0f;
                    float tipY = titleY + titleScale * 16.5f;
                    drawText(tipX, tipY, tipScale, mainMenuTip, glm::vec4(0.85f, 0.85f, 0.85f, 1.0f));
                }
            }
            
            // Special handling for MAP - render the map using colored rectangles
            if (currentMenuState == MenuState::MAP && !elements.empty()) {
                const auto& mapEl = elements[0];
                
                // Draw the map background
                drawRect(mapEl.x, mapEl.y, mapEl.w, mapEl.h, glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
                
                // Draw map pixels as small colored rectangles
                // We sample the heightmap at lower resolution for performance
                if (worldGenerator) {
                    int pixelRes = 128; // Sample resolution
                    float pixelSize = mapEl.w / pixelRes;
                    
                    for (int py = 0; py < pixelRes; py++) {
                        for (int px = 0; px < pixelRes; px++) {
                            // Convert pixel coords to world coords
                            float worldX = mapCenterX + (px - pixelRes / 2) * mapScale * (mapTextureSize / pixelRes);
                            float worldZ = mapCenterZ + (py - pixelRes / 2) * mapScale * (mapTextureSize / pixelRes);
                            
                            // Get height and biome
                            float h = worldGenerator->getHeight(worldX, worldZ);
                            BiomeType biome = worldGenerator->getBiome(worldX, worldZ);
                            
                            // Determine color
                            glm::vec4 color;
                            
                            if (h < 32) { // SEA_LEVEL
                                float depth = (32 - h) / 32.0f;
                                color = glm::vec4(0.08f + 0.15f * (1.0f - depth), 
                                                  0.3f + 0.3f * (1.0f - depth), 
                                                  0.7f + 0.2f * (1.0f - depth), 1.0f);
                            } else {
                                BiomeInfo biomeInfo = worldGenerator->getBiomeInfo(biome);
                                float heightFactor = std::min(h / 100.0f, 1.0f);
                                
                                switch (biome) {
                                    case BiomeType::OCEAN:
                                        color = glm::vec4(0.15f, 0.4f, 0.8f, 1.0f);
                                        break;
                                    case BiomeType::RIVER:
                                        color = glm::vec4(biomeInfo.mapColorR, biomeInfo.mapColorG, biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::PLAINS:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.7f + 0.3f * heightFactor),
                                                          biomeInfo.mapColorG * (0.7f + 0.3f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::DESERT:
                                        color = glm::vec4(biomeInfo.mapColorR, biomeInfo.mapColorG, biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::FOREST:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.8f + 0.2f * heightFactor),
                                                          biomeInfo.mapColorG * (0.8f + 0.2f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::BIRCH_FOREST:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.85f + 0.15f * heightFactor),
                                                          biomeInfo.mapColorG * (0.85f + 0.15f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::TAIGA:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.8f + 0.2f * heightFactor),
                                                          biomeInfo.mapColorG * (0.8f + 0.2f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::JUNGLE:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.75f + 0.25f * heightFactor),
                                                          biomeInfo.mapColorG * (0.75f + 0.25f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::SWAMP:
                                        color = glm::vec4(biomeInfo.mapColorR, biomeInfo.mapColorG, biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::SAVANNA:
                                        color = glm::vec4(biomeInfo.mapColorR * (0.85f + 0.15f * heightFactor),
                                                          biomeInfo.mapColorG * (0.85f + 0.15f * heightFactor),
                                                          biomeInfo.mapColorB, 1.0f);
                                        break;
                                    case BiomeType::MOUNTAINS:
                                        if (h > 120) {
                                            color = glm::vec4(0.94f, 0.96f, 0.98f, 1.0f); // Snow
                                        } else {
                                            float t = (h - 50.0f) / 70.0f;
                                            color = glm::vec4(0.4f + 0.3f * t, 0.4f + 0.3f * t, 0.43f + 0.27f * t, 1.0f);
                                        }
                                        break;
                                    case BiomeType::SNOWY_TUNDRA:
                                        color = glm::vec4(biomeInfo.mapColorR, biomeInfo.mapColorG, biomeInfo.mapColorB, 1.0f);
                                        break;
                                    default:
                                        color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                                        break;
                                }
                            }
                            
                            float rx = mapEl.x + px * pixelSize;
                            float ry = mapEl.y + py * pixelSize;
                            drawRect(rx, ry, pixelSize + 1, pixelSize + 1, color);
                        }
                    }
                    
                    // Draw player marker (red dot at center)
                    float markerSize = 8.0f;
                    float markerX = mapEl.x + mapEl.w / 2 - markerSize / 2;
                    float markerY = mapEl.y + mapEl.h / 2 - markerSize / 2;
                    drawRect(markerX, markerY, markerSize, markerSize, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
                    
                    // Draw border
                    drawRect(markerX - 1, markerY - 1, markerSize + 2, 2, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Top
                    drawRect(markerX - 1, markerY + markerSize - 1, markerSize + 2, 2, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Bottom
                }
            }

            for (const auto& el : elements) {
                glm::vec4 color = el.isHovered ? glm::vec4(0.5f, 0.7f, 0.5f, 0.95f) : glm::vec4(0.25f, 0.25f, 0.28f, 0.9f);
                glm::vec4 borderColor = el.isHovered ? glm::vec4(0.6f, 0.9f, 0.6f, 1.0f) : glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
                
                // Skip the map element itself (we rendered it above)
                if (currentMenuState == MenuState::MAP && &el == &elements[0]) {
                    continue;
                }
                
                // Handle labels (just text, no background)
                if (el.isLabel) {
                    float textScale = 1.8f;
                    glm::vec4 textColor = el.customColor.a > 0 ? el.customColor : glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
                    // Left-align labels
                    drawText(el.x, el.y + (el.h - 7.0f * textScale) / 2.0f, textScale, el.text, textColor);
                    continue;
                }
                
                // Handle headers (larger text with underline)
                if (el.isHeader) {
                    float textScale = 2.2f;
                    glm::vec4 textColor = el.customColor.a > 0 ? el.customColor : glm::vec4(0.5f, 0.9f, 0.5f, 1.0f);
                    float textW = el.text.length() * 6.0f * textScale;
                    float textX = el.x + (el.w - textW) / 2.0f;
                    drawText(textX, el.y, textScale, el.text, textColor);
                    // Draw underline
                    drawRect(el.x, el.y + textScale * 10.0f, el.w, 2, glm::vec4(textColor.r, textColor.g, textColor.b, 0.5f));
                    continue;
                }
                
                // Handle card-style elements (larger, rounded-look buttons)
                if (el.isCard) {
                    glm::vec4 cardColor = el.isHovered ? glm::vec4(0.35f, 0.45f, 0.35f, 0.95f) : glm::vec4(0.18f, 0.2f, 0.22f, 0.9f);
                    glm::vec4 cardBorder = el.isHovered ? glm::vec4(0.5f, 0.8f, 0.5f, 1.0f) : glm::vec4(0.3f, 0.35f, 0.38f, 1.0f);
                    
                    // Draw card background with thicker border
                    float borderWidth = 3.0f;
                    drawRect(el.x - borderWidth, el.y - borderWidth, el.w + borderWidth*2, el.h + borderWidth*2, cardBorder);
                    drawRect(el.x, el.y, el.w, el.h, cardColor);
                    
                    // Draw thumbnail if available
                    float textOffsetX = 0.0f;
                    if (el.textureId != 0) {
                        // Draw thumbnail border
                        float thumbBorder = 2.0f;
                        drawRect(el.thumbnailX - thumbBorder, el.thumbnailY - thumbBorder, 
                                 el.thumbnailW + thumbBorder*2, el.thumbnailH + thumbBorder*2, 
                                 glm::vec4(0.1f, 0.1f, 0.12f, 1.0f));
                        
                        // Draw the thumbnail image
                        drawTexturedRect(el.thumbnailX, el.thumbnailY, el.thumbnailW, el.thumbnailH, el.textureId);
                        
                        // Offset text to the right of thumbnail
                        textOffsetX = el.thumbnailW + 24.0f;
                    }
                    
                    // Draw text (left-aligned if thumbnail, centered otherwise)
                    float textScale = 2.5f;
                    float textW = el.text.length() * 6.0f * textScale;
                    float textX, textY;
                    
                    if (el.textureId != 0) {
                        // Left-align after thumbnail
                        textX = el.x + textOffsetX + 12.0f;
                        textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                    } else {
                        // Center if no thumbnail
                        textX = el.x + (el.w - textW) / 2.0f;
                        textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                    }
                    drawText(textX, textY, textScale, el.text, glm::vec4(1.0f));
                    continue;
                }
                
                if (el.isInventoryItem) {
                    // Draw slot background with border effect
                    glm::vec4 bgColor = el.isHovered ? glm::vec4(0.4f, 0.4f, 0.5f, 0.95f) : glm::vec4(0.15f, 0.15f, 0.15f, 0.9f);
                    glm::vec4 innerColor = el.isHovered ? glm::vec4(0.3f, 0.3f, 0.35f, 0.95f) : glm::vec4(0.22f, 0.22f, 0.22f, 0.9f);
                    
                    // Outer border
                    drawRect(el.x - 2, el.y - 2, el.w + 4, el.h + 4, glm::vec4(0.1f, 0.1f, 0.1f, 0.9f));
                    drawRect(el.x, el.y, el.w, el.h, bgColor);
                    drawRect(el.x + 2, el.y + 2, el.w - 4, el.h - 4, innerColor);
                    
                    // Get rotation for this inventory item
                    float rotation = 0.0f;
                    if (el.inventoryIndex >= 0 && el.inventoryIndex < 256) {
                        rotation = inventoryItemRotations[el.inventoryIndex];
                    }
                    
                    // Draw 3D isometric block or tool
                    if (el.isInventoryTool && el.itemType != ItemType::NONE) {
                        drawToolIcon(el.x + 4, el.y + 4, el.w - 8, el.itemType);
                    } else if (el.blockType != BlockType::AIR) {
                        drawBlockIcon(el.x + 4, el.y + 4, el.w - 8, el.blockType, rotation);
                    }
                    
                    // Draw selection highlight if this block is in current hotbar slot
                    if (!el.isInventoryTool && el.blockType == hotbar[selectedSlot]) {
                        glm::vec4 hl(0.9f, 0.8f, 0.3f, 1.0f); // Golden highlight
                        float t = 3.0f;
                        drawRect(el.x - 2, el.y - 2, el.w + 4, t, hl); // Top
                        drawRect(el.x - 2, el.y + el.h - t + 2, el.w + 4, t, hl); // Bottom
                        drawRect(el.x - 2, el.y - 2, t, el.h + 4, hl); // Left
                        drawRect(el.x + el.w - t + 2, el.y - 2, t, el.h + 4, hl); // Right
                    }
                    continue;
                }

                // Draw button with border for better look
                float borderWidth = 2.0f;
                drawRect(el.x - borderWidth, el.y - borderWidth, el.w + borderWidth*2, el.h + borderWidth*2, borderColor);
                drawRect(el.x, el.y, el.w, el.h, color);
                
                // Draw slider indicator
                if (el.isSlider) {
                    float val = 0.0f;
                    if (el.intValueRef) val = (float)*el.intValueRef;
                    else if (el.valueRef) val = *el.valueRef;
                    
                    float pct = (val - el.minVal) / (el.maxVal - el.minVal);
                    drawRect(el.x, el.y, el.w * pct, el.h, glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
                }

                // Draw text centered
                float textScale = 2.0f;
                float textW = el.text.length() * 6.0f * textScale; // Approx width
                float textX = el.x + (el.w - textW) / 2.0f;
                float textY = el.y + (el.h - 7.0f * textScale) / 2.0f;
                drawText(textX, textY, textScale, el.text, glm::vec4(1.0f));
            }
            
            // Render 3D model preview for Player Settings - AFTER all UI elements
            if (currentMenuState == MenuState::PLAYER_SETTINGS && previewModel) {
                // Use full screen for rendering - no clipping!
                // Render the 3D model directly to screen
                uiShader.unuse();
                
                // Use full screen viewport - no scissor clipping
                glViewport(0, 0, width, height);
                glDisable(GL_SCISSOR_TEST);  // Don't clip!
                
                // Clear just depth
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glClear(GL_DEPTH_BUFFER_BIT);
                glDisable(GL_BLEND);
                
                // Setup camera for 3D preview
                float aspectRatio = (float)width / (float)height;
                glm::mat4 proj3D = glm::perspective(glm::radians(30.0f), aspectRatio, 0.1f, 100.0f);
                
                // Camera looking at model - pull back so models fit nicely
                // Half-Life (index 0) needs more distance
                float camDist = (previewModelIndex == 0) ? 20.0f : 6.0f;
                float camHeight = (previewModelIndex == 0) ? 4.0f : 1.5f;
                float targetHeight = (previewModelIndex == 0) ? 2.5f : 0.7f;
                
                glm::vec3 camPos = glm::vec3(0.0f, camHeight, camDist);
                glm::vec3 target = glm::vec3(0.0f, targetHeight, 0.0f);
                glm::mat4 view3D = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
                
                // Model transform - rotate based on user drag
                glm::mat4 modelMat = glm::mat4(1.0f);
                modelMat = glm::rotate(modelMat, glm::radians(previewRotation), glm::vec3(0.0f, 1.0f, 0.0f));
                
                // Scale down all models - Half-Life much smaller, others also scaled
                float modelScale = (previewModelIndex == 0) ? 0.1f : 0.6f;
                modelMat = glm::scale(modelMat, glm::vec3(modelScale));
                
                // Use the model shader
                modelPreviewShader.use();
                modelPreviewShader.setMat4("uProjection", proj3D);
                modelPreviewShader.setMat4("uView", view3D);
                modelPreviewShader.setMat4("uPrevView", view3D);
                modelPreviewShader.setMat4("uPrevProjection", proj3D);
                modelPreviewShader.setVec3("uLightDir", glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)));
                modelPreviewShader.setVec3("uCameraPos", camPos);
                modelPreviewShader.setVec4("uBaseColor", glm::vec4(1.0f));
                modelPreviewShader.setInt("uDebugNoTexture", 0);
                modelPreviewShader.setInt("uDebugShowNormals", 0);
                modelPreviewShader.setInt("uUseShadows", 0);  // No shadows in preview
                modelPreviewShader.setFloat("uAlphaMultiplier", 1.0f);
                modelPreviewShader.setMat4("uLightSpaceMatrix", glm::mat4(1.0f));  // Identity for preview
                
                // Draw the model
                previewModel->draw(modelPreviewShader, modelMat, modelMat);
                
                modelPreviewShader.unuse();
                
                // Restore viewport to full screen
                glViewport(0, 0, width, height);
                glDisable(GL_SCISSOR_TEST);
                
                // Back to UI shader state
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                uiShader.use();
                uiShader.setMat4("uProjection", glm::ortho(0.0f, (float)width, (float)height, 0.0f));
            }
        }

        if (showDebug) {
            std::string fpsText = "FPS: " + std::to_string((int)currentFPS);
            std::string blockText = "Block: " + currentBlockName;
            
            drawText(10.0f, 30.0f, 2.0f, fpsText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            drawText(10.0f, 60.0f, 2.0f, blockText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            
            
            std::string posText = "XYZ: " + std::to_string((int)currentPlayerPos.x) + " " + 
                                  std::to_string((int)currentPlayerPos.y) + " " + 
                                  std::to_string((int)currentPlayerPos.z);
            drawText(10.0f, 90.0f, 2.0f, posText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            float speed = glm::length(currentPlayerVel);
            float hSpeed = glm::length(glm::vec2(currentPlayerVel.x, currentPlayerVel.z));
            std::string velText = "SPEED: " + std::to_string(speed).substr(0,4) + " (H: " + std::to_string(hSpeed).substr(0,4) + ")";
            drawText(10.0f, 120.0f, 2.0f, velText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            std::string timeText = "TIME: " + std::to_string(static_cast<int>(timeOfDay));
            drawText(10.0f, 150.0f, 2.0f, timeText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            
            // Debug Controls
            drawText(10.0f, 180.0f, 2.0f, "[F1] TOGGLE DEBUG", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 210.0f, 2.0f, "[F2] PAUSE TIME: " + std::string(isDayNightPaused ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 240.0f, 2.0f, "[F3] SHADOWS: " + std::string(Settings::instance().enableShadows ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 270.0f, 2.0f, "[ARROWS] CHANGE TIME", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

            // New debug toggles
            drawText(10.0f, 300.0f, 2.0f, "[F4] SHOW TAA METRICS: " + std::string(Settings::instance().debugShowTAA ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 330.0f, 2.0f, "[F8] NO TEXTURES: " + std::string(Settings::instance().debugNoTexture ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 360.0f, 2.0f, "[F6] WIREFRAME: " + std::string(Settings::instance().debugWireframe ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            drawText(10.0f, 390.0f, 2.0f, "[F7] SHOW NORMALS: " + std::string(Settings::instance().debugShowNormals ? "ON" : "OFF"), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

            // Show TAA metrics if enabled
            if (Settings::instance().debugShowTAA) {
                std::string taaMotion = "TAA Motion: " + std::to_string(lastTaaMotion).substr(0,6);
                std::string taaHistory = "TAA HistWeight: " + std::to_string(lastTaaHistoryWeight).substr(0,6);
                drawText(10.0f, 420.0f, 2.0f, taaMotion, glm::vec4(0.9f, 0.6f, 0.2f, 1.0f));
                drawText(10.0f, 450.0f, 2.0f, taaHistory, glm::vec4(0.9f, 0.6f, 0.2f, 1.0f));
            }
        }
        
        // Render tooltip if visible and timer exceeded delay
        if (!currentTooltip.empty() && tooltipTimer >= tooltipDelay && Settings::instance().enableTooltips) {
            float padding = 8.0f;
            float textScale = 1.5f;
            float tooltipW = currentTooltip.length() * 6.0f * textScale + padding * 2;
            float tooltipH = 7.0f * textScale + padding * 2;
            
            // Keep tooltip on screen
            float drawX = tooltipX;
            float drawY = tooltipY;
            if (drawX + tooltipW > width) drawX = width - tooltipW - 5;
            if (drawY + tooltipH > height) drawY = height - tooltipH - 5;
            
            // Draw tooltip background
            drawRect(drawX, drawY, tooltipW, tooltipH, glm::vec4(0.1f, 0.1f, 0.12f, 0.95f));
            // Draw border
            drawRect(drawX, drawY, tooltipW, 1, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX, drawY + tooltipH - 1, tooltipW, 1, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX, drawY, 1, tooltipH, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            drawRect(drawX + tooltipW - 1, drawY, 1, tooltipH, glm::vec4(0.4f, 0.4f, 0.45f, 1.0f));
            // Draw text
            drawText(drawX + padding, drawY + padding, textScale, currentTooltip, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
        }

        uiShader.unuse();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    
    // Always render HUD unless in Main Menu / Settings / Load Game (basically if we are 'in game' or 'inventory')
    // Specifically: NONE (playing) or INVENTORY or CHAT (chat overlay). Not IN_GAME_MENU (pause).
    if (currentMenuState == MenuState::NONE || currentMenuState == MenuState::INVENTORY || currentMenuState == MenuState::CHAT) {
        renderHUD();
        renderChat();
    }
}

void UIManager::renderHUD() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    // ========================================
    // HOTBAR - Modern Glass-like Design
    // ========================================
    float slotSize = 48.0f;
    float slotPadding = 4.0f;
    float slotGap = 3.0f;
    int slots = 9;
    float innerSize = slotSize - slotPadding * 2;
    float totalW = slots * slotSize + (slots - 1) * slotGap;
    float hotbarPadding = 6.0f;
    float hotbarW = totalW + hotbarPadding * 2;
    float hotbarH = slotSize + hotbarPadding * 2;
    float startX = (width - hotbarW) / 2.0f;
    float startY = height - hotbarH - 8.0f;
    
    // Hotbar background - dark glass effect
    drawRect(startX, startY, hotbarW, hotbarH, glm::vec4(0.05f, 0.05f, 0.08f, 0.85f));
    // Top highlight
    drawRect(startX, startY, hotbarW, 1.0f, glm::vec4(0.3f, 0.3f, 0.35f, 0.6f));
    // Bottom shadow
    drawRect(startX, startY + hotbarH - 1, hotbarW, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    // Left/right borders
    drawRect(startX, startY, 1.0f, hotbarH, glm::vec4(0.2f, 0.2f, 0.25f, 0.5f));
    drawRect(startX + hotbarW - 1, startY, 1.0f, hotbarH, glm::vec4(0.2f, 0.2f, 0.25f, 0.5f));
    
    float slotStartX = startX + hotbarPadding;
    float slotStartY = startY + hotbarPadding;
    
    for (int i = 0; i < slots; ++i) {
        float x = slotStartX + i * (slotSize + slotGap);
        float y = slotStartY;
        
        // Apply hover animation (slots bob up slightly when hovered)
        float hoverOffset = hotbarSlotHover[i] * 6.0f;
        float bounceOffset = hotbarSlotBounce[i] * 4.0f;
        y -= hoverOffset + bounceOffset;
        
        // Selection highlight
        if (i == selectedSlot) {
            // Outer glow
            float glowSize = 4.0f + selectionBounce * 2.0f;
            drawRect(x - glowSize, y - glowSize, slotSize + glowSize * 2, slotSize + glowSize * 2, 
                     glm::vec4(1.0f, 0.85f, 0.3f, 0.3f + selectionBounce * 0.2f));
            // Golden border
            drawRect(x - 2, y - 2, slotSize + 4, slotSize + 4, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
            drawRect(x - 1, y - 1, slotSize + 2, slotSize + 2, glm::vec4(0.9f, 0.75f, 0.2f, 1.0f));
        }
        
        // Slot background with subtle gradient effect
        glm::vec4 slotBg = (i == selectedSlot) ? 
            glm::vec4(0.2f, 0.2f, 0.22f, 0.95f) : 
            glm::vec4(0.12f, 0.12f, 0.14f, 0.9f);
        
        // Brighten on hover
        if (hotbarSlotHover[i] > 0.0f) {
            float h = hotbarSlotHover[i];
            slotBg = glm::vec4(slotBg.r + h * 0.1f, slotBg.g + h * 0.1f, slotBg.b + h * 0.12f, slotBg.a);
        }
        
        drawRect(x, y, slotSize, slotSize, slotBg);
        
        // Inner border (3D inset effect)
        drawRect(x, y, slotSize, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));  // top shadow
        drawRect(x, y, 1.0f, slotSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));  // left shadow
        drawRect(x, y + slotSize - 1, slotSize, 1.0f, glm::vec4(0.3f, 0.3f, 0.35f, 0.3f));  // bottom highlight
        drawRect(x + slotSize - 1, y, 1.0f, slotSize, glm::vec4(0.3f, 0.3f, 0.35f, 0.3f));  // right highlight
        
        // Draw item/block from hotbarSlots (supports both blocks and items)
        const HotbarSlot& slot = hotbarSlots[i];
        float iconX = x + slotPadding;
        float iconY = y + slotPadding;
        
        if (!slot.isEmpty()) {
            if (slot.isItem) {
                // Draw tool icon
                drawToolIcon(iconX, iconY, innerSize, slot.itemStack.type);
            } else if (slot.blockType != BlockType::AIR) {
                // Draw block with rotation on hover for spinning effect
                drawBlockIcon(iconX, iconY, innerSize, slot.blockType, hotbarSlotRotation[i]);
            }
        }
        
        // Slot number (subtle, bottom-right corner)
        float numX = x + slotSize - 12;
        float numY = y + slotSize - 14;
        glm::vec4 numColor = (i == selectedSlot) ? 
            glm::vec4(1.0f, 0.9f, 0.6f, 0.9f) : 
            glm::vec4(0.6f, 0.6f, 0.65f, 0.7f);
        drawText(numX, numY, 0.4f, std::to_string(i + 1), numColor);
    }
    
    // ========================================
    // STATS BARS - Above Hotbar
    // ========================================
    float statsY = startY - 32.0f;
    float barHeight = 10.0f;
    float barWidth = (totalW - 20.0f) / 2.0f;
    float iconSize = 14.0f;
    
    // Health Bar (left side)
    float healthX = startX + hotbarPadding;
    
    // Health icon (heart shape approximation)
    glm::vec4 heartColor = glm::vec4(0.9f, 0.15f, 0.2f, 1.0f);
    if (healthFlash > 0.0f) {
        heartColor = glm::mix(heartColor, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), healthFlash);
    }
    // Simple heart icon made of squares
    drawRect(healthX + 2, statsY + 2, iconSize - 4, iconSize - 4, heartColor);
    drawRect(healthX, statsY + 4, iconSize, iconSize - 6, heartColor);
    drawRect(healthX + 4, statsY, iconSize - 8, iconSize - 2, heartColor);
    
    // Health bar background
    float hBarX = healthX + iconSize + 6;
    drawRect(hBarX, statsY + 1, barWidth, barHeight, glm::vec4(0.15f, 0.05f, 0.05f, 0.9f));
    // Health bar border
    drawRect(hBarX, statsY + 1, barWidth, 1.0f, glm::vec4(0.4f, 0.1f, 0.1f, 1.0f));
    drawRect(hBarX, statsY + barHeight, barWidth, 1.0f, glm::vec4(0.2f, 0.05f, 0.05f, 1.0f));
    
    // Health fill with gradient
    float healthPct = playerHealth / 20.0f;
    float healthFillW = (barWidth - 4) * healthPct;
    if (healthFillW > 0) {
        glm::vec4 healthColorMain = glm::vec4(0.85f, 0.15f, 0.2f, 1.0f);
        glm::vec4 healthColorBright = glm::vec4(1.0f, 0.3f, 0.35f, 1.0f);
        if (healthFlash > 0.0f) {
            healthColorMain = glm::mix(healthColorMain, glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), healthFlash);
        }
        drawRect(hBarX + 2, statsY + 3, healthFillW, barHeight - 4, healthColorMain);
        // Top shine
        drawRect(hBarX + 2, statsY + 3, healthFillW, 2.0f, healthColorBright);
    }
    
    // Health text
    std::string healthText = std::to_string(playerHealth) + "/20";
    drawText(hBarX + barWidth / 2 - healthText.length() * 3, statsY + 2, 0.4f, healthText, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    
    // Hunger/Food Bar (right side)
    float foodX = startX + hotbarW - hotbarPadding - barWidth - iconSize - 6;
    
    // Food icon (drumstick shape approximation)
    glm::vec4 foodColor = glm::vec4(0.7f, 0.45f, 0.2f, 1.0f);
    drawRect(foodX + 2, statsY + 3, iconSize - 4, iconSize - 6, foodColor);
    drawRect(foodX + iconSize - 6, statsY + 5, 4, iconSize - 8, glm::vec4(0.9f, 0.85f, 0.7f, 1.0f)); // bone
    
    // Food bar
    float fBarX = foodX + iconSize + 6;
    drawRect(fBarX, statsY + 1, barWidth, barHeight, glm::vec4(0.1f, 0.08f, 0.03f, 0.9f));
    drawRect(fBarX, statsY + 1, barWidth, 1.0f, glm::vec4(0.3f, 0.2f, 0.1f, 1.0f));
    drawRect(fBarX, statsY + barHeight, barWidth, 1.0f, glm::vec4(0.15f, 0.1f, 0.05f, 1.0f));
    
    // Food fill
    float foodPct = playerFood / 20.0f;
    float foodFillW = (barWidth - 4) * foodPct;
    if (foodFillW > 0) {
        drawRect(fBarX + 2, statsY + 3, foodFillW, barHeight - 4, glm::vec4(0.65f, 0.4f, 0.15f, 1.0f));
        drawRect(fBarX + 2, statsY + 3, foodFillW, 2.0f, glm::vec4(0.8f, 0.55f, 0.25f, 1.0f));
    }
    
    // Food text
    std::string foodText = std::to_string(playerFood) + "/20";
    drawText(fBarX + barWidth / 2 - foodText.length() * 3, statsY + 2, 0.4f, foodText, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    
    // ========================================
    // XP BAR - Below stats, above hotbar
    // ========================================
    float xpY = startY - 14.0f;
    float xpH = 6.0f;
    float xpW = totalW;
    float xpX = startX + hotbarPadding;
    
    // XP bar background
    drawRect(xpX, xpY, xpW, xpH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    drawRect(xpX, xpY, xpW, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    
    // XP fill with glow effect
    float xpFillW = xpW * playerXP;
    if (xpFillW > 0) {
        glm::vec4 xpColor = glm::vec4(0.3f, 0.9f, 0.3f, 1.0f);
        if (xpBarGlow > 0.0f) {
            xpColor = glm::mix(xpColor, glm::vec4(0.6f, 1.0f, 0.6f, 1.0f), xpBarGlow);
        }
        drawRect(xpX + 1, xpY + 1, xpFillW - 2, xpH - 2, xpColor);
        // Shine
        drawRect(xpX + 1, xpY + 1, xpFillW - 2, 2.0f, glm::vec4(0.5f, 1.0f, 0.5f, 0.8f));
    }
    
    // Level indicator (centered above XP bar)
    if (playerLevel > 0) {
        std::string levelText = std::to_string(playerLevel);
        float levelX = width / 2.0f - levelText.length() * 4;
        float levelY = xpY - 14.0f;
        // Shadow
        drawText(levelX + 1, levelY + 1, 0.5f, levelText, glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
        // Text with glow
        glm::vec4 levelColor = glm::vec4(0.5f, 1.0f, 0.5f, 1.0f);
        if (xpBarGlow > 0.0f) {
            levelColor = glm::mix(levelColor, glm::vec4(1.0f, 1.0f, 0.5f, 1.0f), xpBarGlow);
        }
        drawText(levelX, levelY, 0.5f, levelText, levelColor);
    }
    
    // ========================================
    // GAME MODE INDICATOR
    // ========================================
    if (isCreativeMode) {
        std::string modeText = "Creative Mode";
        float modeTextW = modeText.length() * 7.0f;
        float modeX = 12.0f;
        float modeY = 12.0f;
        
        // Background pill
        drawRect(modeX, modeY, modeTextW + 16.0f, 24.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
        drawRect(modeX, modeY, modeTextW + 16.0f, 1.0f, glm::vec4(0.3f, 0.7f, 1.0f, 0.5f));
        
        // Icon (star-like)
        drawRect(modeX + 6, modeY + 8, 8, 8, glm::vec4(0.3f, 0.8f, 1.0f, 1.0f));
        
        // Text
        drawText(modeX + 18, modeY + 6, 0.45f, modeText, glm::vec4(0.4f, 0.85f, 1.0f, 1.0f));
    }
    
    uiShader.unuse();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void UIManager::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    
    uiShader.setMat4("uModel", model);
    uiShader.setVec4("uColor", color);
    
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UIManager::drawTexturedRect(float x, float y, float w, float h, GLuint textureId) {
    if (textureId == 0) return;
    
    texturedShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    texturedShader.setMat4("uProjection", projection);
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    texturedShader.setMat4("uModel", model);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    texturedShader.setInt("uTexture", 0);
    
    glBindVertexArray(texturedVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // Switch back to UI shader for subsequent draws
    uiShader.use();
    uiShader.setMat4("uProjection", projection);
}

void UIManager::drawRoundedRect(float x, float y, float w, float h, float radius, const glm::vec4& color) {
    // For now, just draw a regular rect - true rounded corners would need more vertices
    // We can simulate with layered rects
    drawRect(x + radius, y, w - radius * 2, h, color);
    drawRect(x, y + radius, w, h - radius * 2, color);
    // Corner fills (approximation)
    drawRect(x, y, radius, radius, color);
    drawRect(x + w - radius, y, radius, radius, color);
    drawRect(x, y + h - radius, radius, radius, color);
    drawRect(x + w - radius, y + h - radius, radius, radius, color);
}

void UIManager::drawGradientRect(float x, float y, float w, float h, const glm::vec4& colorTop, const glm::vec4& colorBottom) {
    // Simulate gradient with multiple horizontal strips
    int strips = 8;
    float stripH = h / strips;
    for (int i = 0; i < strips; ++i) {
        float t = (float)i / (strips - 1);
        glm::vec4 c = glm::mix(colorTop, colorBottom, t);
        drawRect(x, y + i * stripH, w, stripH + 1, c);  // +1 to avoid gaps
    }
}

void UIManager::updateHUDAnimations(float deltaTime, double mouseX, double mouseY) {
    // Calculate hotbar slot positions for hover detection
    float slotSize = 48.0f;
    float slotGap = 3.0f;
    int slots = 9;
    float totalW = slots * slotSize + (slots - 1) * slotGap;
    float hotbarPadding = 6.0f;
    float hotbarW = totalW + hotbarPadding * 2;
    float hotbarH = slotSize + hotbarPadding * 2;
    float startX = (width - hotbarW) / 2.0f;
    float startY = height - hotbarH - 8.0f;
    float slotStartX = startX + hotbarPadding;
    float slotStartY = startY + hotbarPadding;
    
    // Detect which slot is hovered
    int newHoveredSlot = -1;
    for (int i = 0; i < slots; ++i) {
        float x = slotStartX + i * (slotSize + slotGap);
        float y = slotStartY;
        
        if (mouseX >= x && mouseX <= x + slotSize &&
            mouseY >= y - 10 && mouseY <= y + slotSize + 10) {  // Slightly larger hit area
            newHoveredSlot = i;
            break;
        }
    }
    
    // Update hover animations
    float hoverSpeed = 8.0f;
    float rotationSpeed = 45.0f;  // Degrees per second when hovered
    for (int i = 0; i < 9; ++i) {
        float targetHover = (i == newHoveredSlot) ? 1.0f : 0.0f;
        float diff = targetHover - hotbarSlotHover[i];
        hotbarSlotHover[i] += diff * deltaTime * hoverSpeed;
        hotbarSlotHover[i] = std::clamp(hotbarSlotHover[i], 0.0f, 1.0f);
        
        // Bounce animation decays
        if (hotbarSlotBounce[i] > 0.0f) {
            hotbarSlotBounce[i] -= deltaTime * 4.0f;
            if (hotbarSlotBounce[i] < 0.0f) hotbarSlotBounce[i] = 0.0f;
        }
        
        // Rotation animation for hovered slots - spin slowly when hovered
        if (i == newHoveredSlot) {
            hotbarSlotRotation[i] += deltaTime * rotationSpeed;
            if (hotbarSlotRotation[i] >= 360.0f) hotbarSlotRotation[i] -= 360.0f;
        } else {
            // Smoothly return to default rotation
            if (hotbarSlotRotation[i] > 0.0f) {
                hotbarSlotRotation[i] -= deltaTime * rotationSpeed * 2.0f;
                if (hotbarSlotRotation[i] < 0.0f) hotbarSlotRotation[i] = 0.0f;
            }
        }
    }
    
    // Update inventory item rotations when inventory is open
    if (currentMenuState == MenuState::INVENTORY) {
        // Find which inventory item is hovered
        int newInventoryHovered = -1;
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& el = elements[i];
            if (el.isInventoryItem && el.isHovered && el.inventoryIndex >= 0) {
                newInventoryHovered = el.inventoryIndex;
                break;
            }
        }
        inventoryHoveredIndex = newInventoryHovered;
        
        // Update rotations for inventory items
        for (int i = 0; i < 256; ++i) {
            if (i == inventoryHoveredIndex) {
                inventoryItemRotations[i] += deltaTime * rotationSpeed;
                if (inventoryItemRotations[i] >= 360.0f) inventoryItemRotations[i] -= 360.0f;
            } else {
                // Smoothly return to default rotation
                if (inventoryItemRotations[i] > 0.0f) {
                    inventoryItemRotations[i] -= deltaTime * rotationSpeed * 2.0f;
                    if (inventoryItemRotations[i] < 0.0f) inventoryItemRotations[i] = 0.0f;
                }
            }
        }
    }
    
    hoveredSlot = newHoveredSlot;
    
    // Selection bounce animation
    static int lastSelectedSlot = -1;
    if (selectedSlot != lastSelectedSlot) {
        selectionBounce = 1.0f;
        hotbarSlotBounce[selectedSlot] = 1.0f;
        lastSelectedSlot = selectedSlot;
    }
    if (selectionBounce > 0.0f) {
        selectionBounce -= deltaTime * 5.0f;
        if (selectionBounce < 0.0f) selectionBounce = 0.0f;
    }
    
    // Health flash decays
    if (healthFlash > 0.0f) {
        healthFlash -= deltaTime * 3.0f;
        if (healthFlash < 0.0f) healthFlash = 0.0f;
    }
    
    // XP glow decays
    if (xpBarGlow > 0.0f) {
        xpBarGlow -= deltaTime * 2.0f;
        if (xpBarGlow < 0.0f) xpBarGlow = 0.0f;
    }
    
    // Track health changes for flash effect
    static int lastHealth = 20;
    if (playerHealth < lastHealth) {
        healthFlash = 1.0f;  // Trigger flash when taking damage
    }
    lastHealth = playerHealth;
    
    // Track XP changes for glow effect
    static float lastXP = 0.0f;
    if (playerXP > lastXP) {
        xpBarGlow = 1.0f;  // Trigger glow when gaining XP
    }
    lastXP = playerXP;
}

int UIManager::getBlockTextureIndex(BlockType type, int face) {
    // face: 0=top, 1=side, 2=bottom
    // Returns texture atlas index (0-255 for 16x16 atlas)
    // Use ResourcePackManager for actual texture lookup when available
    
    switch (type) {
        // Original blocks
        case BlockType::GRASS:
            if (face == 0) return 0;       // Top - grass top
            if (face == 2) return 2;       // Bottom - dirt
            return 3;                      // Side - grass side
        case BlockType::DIRT:
            return 2;                      // All faces - dirt
        case BlockType::STONE:
            return 1;                      // All faces - stone
        case BlockType::SAND:
            return 18;                     // All faces - sand
        case BlockType::GRAVEL:
            return 19;                     // All faces - gravel
        case BlockType::WOOD:
            return 4;                      // All faces - planks
        case BlockType::LEAVES:
            return 52;                     // All faces - leaves
        case BlockType::LOG:
            if (face == 0 || face == 2) return 21; // Top/Bottom - log top
            return 20;                     // Side - log side
        case BlockType::SNOW:
            return 66;                     // Snow texture
        case BlockType::SANDSTONE:
            if (face == 0 || face == 2) return 176; // sandstone top
            return 192;                    // sandstone side
        case BlockType::WATER:
            return 205;                    // Water texture
        case BlockType::ICE:
            return 67;                     // Ice texture
        case BlockType::BEDROCK:
            return 17;                     // Bedrock
        case BlockType::TALL_GRASS:
            return 39;                     // Tall grass
        case BlockType::ROSE:
            return 12;                     // Rose
            
        // Ores
        case BlockType::COBBLESTONE:
            return 16;                     // Cobblestone
        case BlockType::COAL_ORE:
            return 34;                     // Coal ore
        case BlockType::IRON_ORE:
            return 33;                     // Iron ore
        case BlockType::GOLD_ORE:
            return 32;                     // Gold ore
        case BlockType::DIAMOND_ORE:
            return 50;                     // Diamond ore
        case BlockType::EMERALD_ORE:
            return 171;                    // Emerald ore
        case BlockType::REDSTONE_ORE:
            return 51;                     // Redstone ore
        case BlockType::LAPIS_ORE:
            return 160;                    // Lapis ore
            
        // Stone variants
        case BlockType::MOSSY_COBBLESTONE:
            return 36;                     // Mossy cobblestone
        case BlockType::STONE_BRICKS:
            return 54;                     // Stone bricks
        case BlockType::MOSSY_STONE_BRICKS:
            return 100;                    // Mossy stone bricks
        case BlockType::CRACKED_STONE_BRICKS:
            return 118;                    // Cracked stone bricks
        case BlockType::CHISELED_STONE_BRICKS:
            return 98;                     // Chiseled stone bricks
            
        // Mineral blocks
        case BlockType::IRON_BLOCK:
            return 22;                     // Iron block
        case BlockType::GOLD_BLOCK:
            return 23;                     // Gold block
        case BlockType::DIAMOND_BLOCK:
            return 24;                     // Diamond block
        case BlockType::EMERALD_BLOCK:
            return 25;                     // Emerald block
        case BlockType::REDSTONE_BLOCK:
            return 215;                    // Redstone block
            
        // Building blocks
        case BlockType::BRICKS:
            return 7;                      // Bricks
        case BlockType::OBSIDIAN:
            return 37;                     // Obsidian
        case BlockType::GLASS:
            return 49;                     // Glass
        case BlockType::BOOKSHELF:
            if (face == 0 || face == 2) return 4; // Top/Bottom - planks
            return 35;                     // Side - bookshelf
        case BlockType::TNT:
            if (face == 0) return 9;       // Top
            if (face == 2) return 10;      // Bottom
            return 8;                      // Side
        case BlockType::GLOWSTONE:
            return 105;                    // Glowstone
        case BlockType::REDSTONE_LAMP:
            return 123;                    // Redstone lamp
            
        // Wood - Planks
        case BlockType::OAK_PLANKS:
            return 4;                      // Oak planks
        case BlockType::SPRUCE_PLANKS:
            return 198;                    // Spruce planks
        case BlockType::BIRCH_PLANKS:
            return 214;                    // Birch planks
        case BlockType::JUNGLE_PLANKS:
            return 199;                    // Jungle planks
            
        // Wood - Logs
        case BlockType::OAK_LOG:
            if (face == 0 || face == 2) return 21; // Top
            return 20;                     // Side
        case BlockType::SPRUCE_LOG:
            if (face == 0 || face == 2) return 117; // Spruce log top
            return 116;                    // Spruce log side
        case BlockType::BIRCH_LOG:
            if (face == 0 || face == 2) return 117; // Birch log top
            return 117;                    // Birch log side
        case BlockType::JUNGLE_LOG:
            if (face == 0 || face == 2) return 153; // Jungle log top
            return 153;                    // Jungle log side
            
        // Wood - Leaves
        case BlockType::OAK_LEAVES:
            return 52;                     // Oak leaves
        case BlockType::SPRUCE_LEAVES:
            return 132;                    // Spruce leaves
        case BlockType::BIRCH_LEAVES:
            return 52;                     // Use oak leaves
        case BlockType::JUNGLE_LEAVES:
            return 52;                     // Use oak leaves
            
        // Wool colors (using wool row starting at index 64)
        case BlockType::WHITE_WOOL:
            return 64;                     // White wool
        case BlockType::ORANGE_WOOL:
            return 210;                    // Orange wool
        case BlockType::MAGENTA_WOOL:
            return 194;                    // Magenta wool
        case BlockType::LIGHT_BLUE_WOOL:
            return 178;                    // Light blue wool
        case BlockType::YELLOW_WOOL:
            return 162;                    // Yellow wool
        case BlockType::LIME_WOOL:
            return 146;                    // Lime wool
        case BlockType::PINK_WOOL:
            return 130;                    // Pink wool
        case BlockType::GRAY_WOOL:
            return 114;                    // Gray wool
        case BlockType::LIGHT_GRAY_WOOL:
            return 225;                    // Light gray wool
        case BlockType::CYAN_WOOL:
            return 209;                    // Cyan wool
        case BlockType::PURPLE_WOOL:
            return 193;                    // Purple wool
        case BlockType::BLUE_WOOL:
            return 177;                    // Blue wool
        case BlockType::BROWN_WOOL:
            return 161;                    // Brown wool
        case BlockType::GREEN_WOOL:
            return 145;                    // Green wool
        case BlockType::RED_WOOL:
            return 129;                    // Red wool
        case BlockType::BLACK_WOOL:
            return 113;                    // Black wool
            
        case BlockType::CHISELED_SANDSTONE:
            if (face == 0 || face == 2) return 176;
            return 229;                    // Chiseled sandstone side
            
        // Misc blocks
        case BlockType::CLAY:
            return 53;                     // Clay
        case BlockType::SPONGE:
            return 48;                     // Sponge
        case BlockType::COBWEB:
            return 11;                     // Cobweb
        case BlockType::CRAFTING_TABLE:
            if (face == 0) return 43;      // Crafting table top
            if (face == 2) return 4;       // Bottom - planks
            return 59;                     // Side
        case BlockType::NOTE_BLOCK:
            return 74;                     // Note block
        case BlockType::JUKEBOX:
            if (face == 0) return 75;      // Jukebox top
            return 74;                     // Sides - note block
        case BlockType::FARMLAND:
            if (face == 0) return 86;      // Farmland top
            return 2;                      // Dirt sides
        case BlockType::SUGAR_CANE:
            return 73;                     // Sugar cane
            
        default:
            return 1;                      // Default to stone
    }
}

void UIManager::drawBlockIcon(float x, float y, float size, BlockType type, float extraRotation) {
    if (type == BlockType::AIR || blockAtlasTexture == 0) return;
    
    blockIconShader.use();
    
    // Setup projection - orthographic with some depth
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -100.0f, 100.0f);
    blockIconShader.setMat4("uProjection", projection);
    
    // Setup model matrix - position, scale, and rotate for isometric view
    glm::mat4 model = glm::mat4(1.0f);
    // Move to center of the slot
    model = glm::translate(model, glm::vec3(x + size * 0.5f, y + size * 0.5f, 0.0f));
    // Scale to fit in slot
    model = glm::scale(model, glm::vec3(size * 0.6f, size * 0.6f, size * 0.6f));
    // Rotate for isometric view (like Minecraft inventory) - tilt FORWARD to show top
    model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Tilt forward (top visible)
    
    // User requested "rotate cube to left to face me on that edge"
    // Standard Minecraft view shows Front and Right faces.
    // +45 deg shows Front+Left.
    // -45 (315) deg shows Front+Right.
    // Add extraRotation for hover spin effect
    model = glm::rotate(model, glm::radians(315.0f + extraRotation), glm::vec3(0.0f, 1.0f, 0.0f));   // Rotate to show Front+Right + spin
    
    blockIconShader.setMat4("uModel", model);
    
    // Set atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    blockIconShader.setInt("uTexture", 0);
    
    // Set atlas cell size (16x16 atlas)
    float cellSize = 1.0f / 16.0f;
    blockIconShader.setFloat("uCellSize", cellSize);
    
    // Get texture indices for each face
    int topIdx = getBlockTextureIndex(type, 0);
    int sideIdx = getBlockTextureIndex(type, 1);
    int bottomIdx = getBlockTextureIndex(type, 2);
    
    // Calculate atlas offsets for each face
    auto idxToOffset = [cellSize](int idx) -> glm::vec2 {
        float col = float(idx % 16);
        float row = float(idx / 16);
        row = 15.0f - row; // Flip Y because texture is flipped
        return glm::vec2(col * cellSize, row * cellSize);
    };
    
    glm::vec2 offsets[3] = {
        idxToOffset(topIdx),    // top
        idxToOffset(sideIdx),   // side
        idxToOffset(bottomIdx)  // bottom
    };
    
    blockIconShader.setVec2("uAtlasOffset[0]", offsets[0]);
    blockIconShader.setVec2("uAtlasOffset[1]", offsets[1]);
    blockIconShader.setVec2("uAtlasOffset[2]", offsets[2]);
    
    // Enable depth test for proper face ordering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);  // Clear depth to avoid stale values
    
    // Disable culling - we want to see all faces of the block icon
    glDisable(GL_CULL_FACE);
    
    glBindVertexArray(blockIconVao);
    glDrawArrays(GL_TRIANGLES, 0, 36); // 6 faces * 6 vertices
    glBindVertexArray(0);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);  // Re-enable for other rendering
    
    // IMPORTANT: Reset texture binding to avoid affecting entity rendering
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Switch back to UI shader
    uiShader.use();
    glm::mat4 uiProjection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", uiProjection);
}

void UIManager::drawToolIcon(float x, float y, float size, ItemType type) {
    if (type == ItemType::NONE) return;
    
    // Draw a simplified tool icon using basic shapes
    // Color based on material
    glm::vec4 handleColor(0.5f, 0.35f, 0.2f, 1.0f);  // Wood handle
    glm::vec4 headColor;
    
    // ItemType enum is organized by TOOL TYPE first, then MATERIAL:
    // SWORD_WOOD=1, SWORD_STONE=2, SWORD_GOLD=3, SWORD_DIAMOND=4
    // PICKAXE_WOOD=5, PICKAXE_STONE=6, PICKAXE_GOLD=7, PICKAXE_DIAMOND=8
    // AXE_WOOD=9, AXE_STONE=10, AXE_GOLD=11, AXE_DIAMOND=12
    // SHOVEL_WOOD=13, SHOVEL_STONE=14, SHOVEL_GOLD=15, SHOVEL_DIAMOND=16
    
    int typeVal = static_cast<int>(type);
    int toolType = (typeVal - 1) / 4;   // 0=sword, 1=pickaxe, 2=axe, 3=shovel
    int material = (typeVal - 1) % 4;   // 0=wood, 1=stone, 2=gold, 3=diamond
    
    // Determine material color
    switch (material) {
        case 0: // Wood
            headColor = glm::vec4(0.6f, 0.45f, 0.25f, 1.0f);
            break;
        case 1: // Stone
            headColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            break;
        case 2: // Gold
            headColor = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);
            break;
        case 3: // Diamond
            headColor = glm::vec4(0.3f, 0.9f, 0.9f, 1.0f);
            break;
        default:
            headColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            break;
    }
    
    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;
    float handleW = size * 0.12f;
    float handleH = size * 0.5f;
    
    if (toolType == 0) {
        // Sword - vertical blade with handle
        // Handle
        drawRect(cx - handleW * 0.5f, cy + size * 0.1f, handleW, handleH, handleColor);
        // Guard
        drawRect(cx - size * 0.15f, cy, size * 0.3f, handleW, headColor);
        // Blade
        drawRect(cx - handleW * 0.7f, cy - size * 0.35f, handleW * 1.4f, size * 0.4f, headColor);
        // Tip
        drawRect(cx - handleW * 0.5f, cy - size * 0.4f, handleW, size * 0.08f, headColor);
    } else if (toolType == 1) {
        // Pickaxe - diagonal with double head
        // Handle (diagonal)
        drawRect(cx - handleW * 0.5f, cy - size * 0.1f, handleW, handleH * 1.1f, handleColor);
        // Head (horizontal bar)
        drawRect(cx - size * 0.35f, cy - size * 0.25f, size * 0.7f, size * 0.15f, headColor);
        // Tips
        drawRect(cx - size * 0.38f, cy - size * 0.35f, size * 0.12f, size * 0.12f, headColor);
        drawRect(cx + size * 0.26f, cy - size * 0.35f, size * 0.12f, size * 0.12f, headColor);
    } else if (toolType == 2) {
        // Axe - handle with single head
        // Handle
        drawRect(cx - handleW * 0.5f, cy, handleW, handleH, handleColor);
        // Axe head
        drawRect(cx - size * 0.05f, cy - size * 0.35f, size * 0.3f, size * 0.35f, headColor);
        // Blade edge
        drawRect(cx + size * 0.18f, cy - size * 0.3f, size * 0.1f, size * 0.25f, headColor);
    } else {
        // Shovel - handle with flat head
        // Handle
        drawRect(cx - handleW * 0.5f, cy - size * 0.05f, handleW, handleH * 1.1f, handleColor);
        // Shovel head
        drawRect(cx - size * 0.15f, cy - size * 0.4f, size * 0.3f, size * 0.35f, headColor);
        // Round tip approximation
        drawRect(cx - size * 0.12f, cy - size * 0.45f, size * 0.24f, size * 0.08f, headColor);
    }
}

std::string UIManager::getBlockName(BlockType type) {
    switch (type) {
        case BlockType::AIR: return "Air";
        case BlockType::GRASS: return "Grass Block";
        case BlockType::DIRT: return "Dirt";
        case BlockType::STONE: return "Stone";
        case BlockType::SAND: return "Sand";
        case BlockType::WATER: return "Water";
        case BlockType::WOOD: return "Wood Planks";
        case BlockType::LEAVES: return "Leaves";
        case BlockType::SNOW: return "Snow Block";
        case BlockType::ICE: return "Ice";
        case BlockType::GRAVEL: return "Gravel";
        case BlockType::SANDSTONE: return "Sandstone";
        case BlockType::LOG: return "Oak Log";
        case BlockType::TALL_GRASS: return "Tall Grass";
        case BlockType::ROSE: return "Flower";
        case BlockType::BEDROCK: return "Bedrock";
        
        // Ores
        case BlockType::COBBLESTONE: return "Cobblestone";
        case BlockType::COAL_ORE: return "Coal Ore";
        case BlockType::IRON_ORE: return "Iron Ore";
        case BlockType::GOLD_ORE: return "Gold Ore";
        case BlockType::DIAMOND_ORE: return "Diamond Ore";
        case BlockType::EMERALD_ORE: return "Emerald Ore";
        case BlockType::REDSTONE_ORE: return "Redstone Ore";
        case BlockType::LAPIS_ORE: return "Lapis Lazuli Ore";
        
        // Stone variants
        case BlockType::MOSSY_COBBLESTONE: return "Mossy Cobblestone";
        case BlockType::STONE_BRICKS: return "Stone Bricks";
        case BlockType::MOSSY_STONE_BRICKS: return "Mossy Stone Bricks";
        case BlockType::CRACKED_STONE_BRICKS: return "Cracked Stone Bricks";
        case BlockType::CHISELED_STONE_BRICKS: return "Chiseled Stone Bricks";
        
        // Mineral blocks
        case BlockType::IRON_BLOCK: return "Block of Iron";
        case BlockType::GOLD_BLOCK: return "Block of Gold";
        case BlockType::DIAMOND_BLOCK: return "Block of Diamond";
        case BlockType::EMERALD_BLOCK: return "Block of Emerald";
        case BlockType::REDSTONE_BLOCK: return "Block of Redstone";
        
        // Building blocks
        case BlockType::BRICKS: return "Bricks";
        case BlockType::OBSIDIAN: return "Obsidian";
        case BlockType::GLASS: return "Glass";
        case BlockType::BOOKSHELF: return "Bookshelf";
        case BlockType::TNT: return "TNT";
        case BlockType::GLOWSTONE: return "Glowstone";
        case BlockType::REDSTONE_LAMP: return "Redstone Lamp";
        
        // Wood - Planks
        case BlockType::OAK_PLANKS: return "Oak Planks";
        case BlockType::SPRUCE_PLANKS: return "Spruce Planks";
        case BlockType::BIRCH_PLANKS: return "Birch Planks";
        case BlockType::JUNGLE_PLANKS: return "Jungle Planks";
        
        // Wood - Logs
        case BlockType::OAK_LOG: return "Oak Log";
        case BlockType::SPRUCE_LOG: return "Spruce Log";
        case BlockType::BIRCH_LOG: return "Birch Log";
        case BlockType::JUNGLE_LOG: return "Jungle Log";
        
        // Wood - Leaves
        case BlockType::OAK_LEAVES: return "Oak Leaves";
        case BlockType::SPRUCE_LEAVES: return "Spruce Leaves";
        case BlockType::BIRCH_LEAVES: return "Birch Leaves";
        case BlockType::JUNGLE_LEAVES: return "Jungle Leaves";
        
        // Wool
        case BlockType::WHITE_WOOL: return "White Wool";
        case BlockType::ORANGE_WOOL: return "Orange Wool";
        case BlockType::MAGENTA_WOOL: return "Magenta Wool";
        case BlockType::LIGHT_BLUE_WOOL: return "Light Blue Wool";
        case BlockType::YELLOW_WOOL: return "Yellow Wool";
        case BlockType::LIME_WOOL: return "Lime Wool";
        case BlockType::PINK_WOOL: return "Pink Wool";
        case BlockType::GRAY_WOOL: return "Gray Wool";
        case BlockType::LIGHT_GRAY_WOOL: return "Light Gray Wool";
        case BlockType::CYAN_WOOL: return "Cyan Wool";
        case BlockType::PURPLE_WOOL: return "Purple Wool";
        case BlockType::BLUE_WOOL: return "Blue Wool";
        case BlockType::BROWN_WOOL: return "Brown Wool";
        case BlockType::GREEN_WOOL: return "Green Wool";
        case BlockType::RED_WOOL: return "Red Wool";
        case BlockType::BLACK_WOOL: return "Black Wool";
        
        case BlockType::CHISELED_SANDSTONE: return "Chiseled Sandstone";
        
        // Misc blocks
        case BlockType::CLAY: return "Clay";
        case BlockType::SPONGE: return "Sponge";
        case BlockType::COBWEB: return "Cobweb";
        case BlockType::CRAFTING_TABLE: return "Crafting Table";
        case BlockType::NOTE_BLOCK: return "Note Block";
        case BlockType::JUKEBOX: return "Jukebox";
        case BlockType::FARMLAND: return "Farmland";
        case BlockType::SUGAR_CANE: return "Sugar Cane";
        
        default: return "Unknown";
    }
}

std::string UIManager::getItemName(ItemType type) {
    switch (type) {
        case ItemType::NONE: return "None";
        case ItemType::SWORD_WOOD: return "Wooden Sword";
        case ItemType::PICKAXE_WOOD: return "Wooden Pickaxe";
        case ItemType::AXE_WOOD: return "Wooden Axe";
        case ItemType::SHOVEL_WOOD: return "Wooden Shovel";
        case ItemType::SWORD_STONE: return "Stone Sword";
        case ItemType::PICKAXE_STONE: return "Stone Pickaxe";
        case ItemType::AXE_STONE: return "Stone Axe";
        case ItemType::SHOVEL_STONE: return "Stone Shovel";
        case ItemType::SWORD_GOLD: return "Golden Sword";
        case ItemType::PICKAXE_GOLD: return "Golden Pickaxe";
        case ItemType::AXE_GOLD: return "Golden Axe";
        case ItemType::SHOVEL_GOLD: return "Golden Shovel";
        case ItemType::SWORD_DIAMOND: return "Diamond Sword";
        case ItemType::PICKAXE_DIAMOND: return "Diamond Pickaxe";
        case ItemType::AXE_DIAMOND: return "Diamond Axe";
        case ItemType::SHOVEL_DIAMOND: return "Diamond Shovel";
        default: return "Unknown";
    }
}

GLuint UIManager::loadWorldPreviewTexture(const std::string& worldName) {
    // Check cache first
    auto it = worldPreviewTextures.find(worldName);
    if (it != worldPreviewTextures.end()) {
        return it->second;
    }
    
    // Check if preview file exists
    if (!WorldSerializer::hasScreenshot(worldName)) {
        return 0;
    }
    
    std::string path = WorldSerializer::getScreenshotPath(worldName);
    
    int imgWidth, imgHeight, channels;
    unsigned char* data = stbi_load(path.c_str(), &imgWidth, &imgHeight, &channels, 3);
    
    if (!data) {
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imgWidth, imgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    
    // Cache the texture
    worldPreviewTextures[worldName] = texture;
    
    return texture;
}

void UIManager::clearWorldPreviewTextures() {
    for (auto& pair : worldPreviewTextures) {
        if (pair.second != 0) {
            glDeleteTextures(1, &pair.second);
        }
    }
    worldPreviewTextures.clear();
}

void UIManager::drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color) {
    float cursorX = x;
    
    // 5x7 pixel font style
    static const uint8_t font[][5] = {
        {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    };
    
    static const uint8_t nums[][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    };

    for (char c : text) {
        int idx = -1;
        bool isNum = false;
        
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a';
        else if (c >= '0' && c <= '9') { idx = c - '0'; isNum = true; }
        
        if (idx >= 0) {
            const uint8_t* glyph = isNum ? nums[idx] : font[idx];
            for (int col = 0; col < 5; col++) {
                uint8_t colData = glyph[col];
                for (int row = 0; row < 7; row++) {
                    if ((colData >> row) & 1) {
                        drawRect(cursorX + col*scale, y + row*scale, scale, scale, color);
                    }
                }
            }
        } else if (c == ':') {
             drawRect(cursorX + 2*scale, y + 1*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 3*scale, scale, scale, color);
        } else if (c == '.') {
             drawRect(cursorX + 2*scale, y + 4*scale, scale, scale, color);
        } else if (c == '-') {
             drawRect(cursorX + 1*scale, y + 3*scale, 3*scale, scale, color);
        } else if (c == '[') {
             drawRect(cursorX + 1*scale, y + 0*scale, scale, 7*scale, color);
             drawRect(cursorX + 2*scale, y + 0*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 6*scale, scale, scale, color);
        } else if (c == ']') {
             drawRect(cursorX + 3*scale, y + 0*scale, scale, 7*scale, color);
             drawRect(cursorX + 2*scale, y + 0*scale, scale, scale, color);
             drawRect(cursorX + 2*scale, y + 6*scale, scale, scale, color);
        }
        
        cursorX += 6 * scale;
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

void UIManager::generateMapTexture() {
    if (!worldGenerator) return;
    
    // Create map texture if not exists
    if (mapTexture == 0) {
        glGenTextures(1, &mapTexture);
    }
    
    // Generate pixel data
    std::vector<unsigned char> pixels(mapTextureSize * mapTextureSize * 3);
    
    for (int py = 0; py < mapTextureSize; py++) {
        for (int px = 0; px < mapTextureSize; px++) {
            // Convert pixel coords to world coords
            float worldX = mapCenterX + (px - mapTextureSize / 2) * mapScale;
            float worldZ = mapCenterZ + (py - mapTextureSize / 2) * mapScale;
            
            // Get height and biome
            float terrainHeight = worldGenerator->getHeight(worldX, worldZ);
            BiomeType biome = worldGenerator->getBiome(worldX, worldZ);
            
            // Determine color based on biome and height
            unsigned char r, g, b;
            
            // Sea level check
            if (terrainHeight < 32) { // SEA_LEVEL
                // Water - deeper = darker blue
                float depth = (32 - terrainHeight) / 32.0f;
                r = static_cast<unsigned char>(20 + 40 * (1.0f - depth));
                g = static_cast<unsigned char>(80 + 80 * (1.0f - depth));
                b = static_cast<unsigned char>(180 + 50 * (1.0f - depth));
            } else {
                // Land - color by biome using BiomeInfo map colors
                BiomeInfo biomeInfo = worldGenerator->getBiomeInfo(biome);
                float heightFactor = std::min(terrainHeight / 100.0f, 1.0f);
                
                switch (biome) {
                    case BiomeType::OCEAN:
                        r = 40; g = 100; b = 200;
                        break;
                    case BiomeType::RIVER:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::PLAINS:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.7f + 0.3f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.7f + 0.3f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::DESERT:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::FOREST:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.8f + 0.2f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.8f + 0.2f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::BIRCH_FOREST:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.85f + 0.15f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.85f + 0.15f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::TAIGA:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.8f + 0.2f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.8f + 0.2f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::JUNGLE:
                        // Jungle - lush dark green
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.75f + 0.25f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.75f + 0.25f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::SWAMP:
                        // Swamp - murky brownish green
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::SAVANNA:
                        // Savanna - dry tan/yellow
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255 * (0.85f + 0.15f * heightFactor));
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255 * (0.85f + 0.15f * heightFactor));
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    case BiomeType::MOUNTAINS:
                        // Gray stone, whiter at peaks
                        if (terrainHeight > 120) {
                            // Snow caps
                            r = 240; g = 245; b = 250;
                        } else {
                            float t = (terrainHeight - 50.0f) / 70.0f;
                            r = static_cast<unsigned char>(100 + 80 * t);
                            g = static_cast<unsigned char>(100 + 80 * t);
                            b = static_cast<unsigned char>(110 + 70 * t);
                        }
                        break;
                    case BiomeType::SNOWY_TUNDRA:
                        r = static_cast<unsigned char>(biomeInfo.mapColorR * 255);
                        g = static_cast<unsigned char>(biomeInfo.mapColorG * 255);
                        b = static_cast<unsigned char>(biomeInfo.mapColorB * 255);
                        break;
                    default:
                        r = 128; g = 128; b = 128;
                        break;
                }
            }
            
            int idx = (py * mapTextureSize + px) * 3;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
        }
    }
    
    // Upload texture
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mapTextureSize, mapTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void UIManager::setupMapMenu() {
    elements.clear();
    
    // Center the player position for the map
    mapCenterX = currentPlayerPos.x;
    mapCenterZ = currentPlayerPos.z;
    
    // Generate the map texture
    generateMapTexture();
    
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    
    // Map display size (as large as possible while fitting on screen)
    float mapDisplaySize = std::min(width, height) * 0.85f;
    float mapX = centerX - mapDisplaySize / 2;
    float mapY = centerY - mapDisplaySize / 2;
    
    // Map element - clicking on it will teleport
    UIElement mapElement;
    mapElement.x = mapX;
    mapElement.y = mapY;
    mapElement.w = mapDisplaySize;
    mapElement.h = mapDisplaySize;
    mapElement.text = ""; // No text, we'll render the texture
    mapElement.isHovered = false;
    mapElement.onClick = nullptr; // Handle click specially in update
    elements.push_back(mapElement);
    
    // Close button
    float btnW = 200.0f;
    float btnH = 40.0f;
    elements.push_back({
        centerX - btnW / 2, mapY + mapDisplaySize + 10, btnW, btnH, 
        "CLOSE (M)", false, [this]() {
            setMenuState(MenuState::NONE);
        }
    });
    
    // Zoom controls
    elements.push_back({
        mapX - 50, centerY - 20, 40, 40, "+", false, [this]() {
            mapScale = std::max(1.0f, mapScale / 2.0f);
            generateMapTexture();
        }
    });
    elements.push_back({
        mapX - 50, centerY + 30, 40, 40, "-", false, [this]() {
            mapScale = std::min(64.0f, mapScale * 2.0f);
            generateMapTexture();
        }
    });
    
    // Info labels
    UIElement scaleLabel = {mapX, mapY - 25, 300, 20, "Scale: " + std::to_string(static_cast<int>(mapScale)) + " blocks/pixel", false, nullptr};
    elements.push_back(scaleLabel);
    
    UIElement coordLabel = {mapX, mapY - 50, 400, 20, "Center: X=" + std::to_string(static_cast<int>(mapCenterX)) + " Z=" + std::to_string(static_cast<int>(mapCenterZ)), false, nullptr};
    elements.push_back(coordLabel);
    
    UIElement helpLabel = {centerX - 150, mapY + mapDisplaySize + 55, 300, 20, "Click on map to teleport", false, nullptr};
    elements.push_back(helpLabel);
}

void UIManager::setupMultiplayerMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Multiplayer";
    title.isLabel = true;
    elements.push_back(title);
    
    // Consistent button styling (matching main menu)
    float btnW = 280.0f;
    float btnH = 50.0f;
    float gap = 12.0f;
    float startY = height / 2.0f - btnH - gap/2;
    
    // Host Game Button
    UIElement hostBtn;
    hostBtn.x = centerX - btnW/2;
    hostBtn.y = startY;
    hostBtn.w = btnW;
    hostBtn.h = btnH;
    hostBtn.text = "Host Game";
    hostBtn.tooltip = "Create a server for others to join";
    hostBtn.onClick = [this]() { setMenuState(MenuState::HOST_GAME); };
    elements.push_back(hostBtn);
    
    // Join Game Button
    UIElement joinBtn;
    joinBtn.x = centerX - btnW/2;
    joinBtn.y = startY + btnH + gap;
    joinBtn.w = btnW;
    joinBtn.h = btnH;
    joinBtn.text = "Join Game";
    joinBtn.tooltip = "Connect to an existing server";
    joinBtn.onClick = [this]() { setMenuState(MenuState::JOIN_GAME); };
    elements.push_back(joinBtn);

    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupHostGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Host Game";
    title.isLabel = true;
    elements.push_back(title);
    
    float inputW = 320.0f;
    float inputH = 42.0f;
    float gap = 20.0f;
    float startY = height / 2.0f - 100;
    
    // Load values from settings - use playerNickname from Player Settings
    auto& settings = Settings::instance();
    playerName = settings.playerNickname; // Always use Player Settings nickname
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name display (read-only, shows from Player Settings)
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name (from Player Settings):";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name display (non-editable)
    UIElement nameDisplay;
    nameDisplay.x = centerX - inputW/2;
    nameDisplay.y = startY + 25;
    nameDisplay.w = inputW;
    nameDisplay.h = inputH;
    nameDisplay.text = settings.playerNickname;
    nameDisplay.isLabel = true;
    nameDisplay.customColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    elements.push_back(nameDisplay);
    
    // Port Label
    UIElement portLabel;
    portLabel.x = centerX - inputW/2;
    portLabel.y = startY + inputH + gap + 25;
    portLabel.w = inputW;
    portLabel.h = 20;
    portLabel.text = "Server Port:";
    portLabel.isLabel = true;
    elements.push_back(portLabel);
    
    // Port Input
    UIElement portInput;
    portInput.x = centerX - inputW/2;
    portInput.y = startY + inputH + gap + 50;
    portInput.w = inputW;
    portInput.h = inputH;
    portInput.text = serverPort;
    portInput.isInput = true;
    portInput.textRef = &serverPort;
    portInput.tooltip = "Port to host on (default: 25565)";
    elements.push_back(portInput);
    
    // Host button
    UIElement hostBtn;
    hostBtn.x = centerX - 120;
    hostBtn.y = startY + (inputH + gap)*2 + 70;
    hostBtn.w = 240;
    hostBtn.h = 50;
    hostBtn.text = "Start Hosting";
    hostBtn.tooltip = "Create server and wait for players";
    hostBtn.onClick = [this]() {
        if (onHostGame) {
            int port = std::stoi(serverPort.empty() ? "25565" : serverPort);
            Settings::instance().lastPlayerName = playerName;
            Settings::instance().lastServerPort = port;
            onHostGame(playerName, port);
        }
    };
    elements.push_back(hostBtn);
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MULTIPLAYER); };
    elements.push_back(back);
}

void UIManager::setupJoinGameMenu() {
    elements.clear();
    float centerX = width / 2.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 100;
    title.y = 50;
    title.w = 200;
    title.h = 35;
    title.text = "Join Game";
    title.isLabel = true;
    elements.push_back(title);
    
    float inputW = 320.0f;
    float inputH = 42.0f;
    float gap = 18.0f;
    float btnW = 240.0f;
    float btnH = 50.0f;
    float startY = height / 2.0f - 140;
    
    // Load values from settings - use playerNickname from Player Settings
    auto& settings = Settings::instance();
    playerName = settings.playerNickname; // Always use Player Settings nickname
    if (serverAddress.empty() || serverAddress == "localhost") {
        serverAddress = settings.lastServerAddress;
    }
    if (serverPort.empty() || serverPort == "25565") {
        serverPort = std::to_string(settings.lastServerPort);
    }
    
    // Player Name display (read-only, shows from Player Settings)
    UIElement nameLabel;
    nameLabel.x = centerX - inputW/2;
    nameLabel.y = startY;
    nameLabel.w = inputW;
    nameLabel.h = 20;
    nameLabel.text = "Your Name (from Player Settings):";
    nameLabel.isLabel = true;
    elements.push_back(nameLabel);
    
    // Player Name display (non-editable)
    UIElement nameDisplay;
    nameDisplay.x = centerX - inputW/2;
    nameDisplay.y = startY + 25;
    nameDisplay.w = inputW;
    nameDisplay.h = inputH;
    nameDisplay.text = settings.playerNickname;
    nameDisplay.isLabel = true;
    nameDisplay.customColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    elements.push_back(nameDisplay);
    
    // Server Address Label
    UIElement addrLabel;
    addrLabel.x = centerX - inputW/2;
    addrLabel.y = startY + inputH + gap + 25;
    addrLabel.w = inputW;
    addrLabel.h = 20;
    addrLabel.text = "Server Address:";
    addrLabel.isLabel = true;
    elements.push_back(addrLabel);
    
    // Server Address Input
    UIElement addrInput;
    addrInput.x = centerX - inputW/2;
    addrInput.y = startY + inputH + gap + 50;
    addrInput.w = inputW;
    addrInput.h = inputH;
    addrInput.text = serverAddress;
    addrInput.isInput = true;
    addrInput.textRef = &serverAddress;
    addrInput.tooltip = "IP address or hostname of the server";
    elements.push_back(addrInput);
    
    // Port Label
    UIElement portLabel;
    portLabel.x = centerX - inputW/2;
    portLabel.y = startY + (inputH + gap)*2 + 50;
    portLabel.w = inputW;
    portLabel.h = 20;
    portLabel.text = "Server Port:";
    portLabel.isLabel = true;
    elements.push_back(portLabel);
    
    // Port Input
    UIElement portInput;
    portInput.x = centerX - inputW/2;
    portInput.y = startY + (inputH + gap)*2 + 75;
    portInput.w = inputW;
    portInput.h = inputH;
    portInput.text = serverPort;
    portInput.isInput = true;
    portInput.textRef = &serverPort;
    portInput.tooltip = "Port to connect to (default: 25565)";
    elements.push_back(portInput);
    
    // Status display
    if (!networkStatus.empty()) {
        UIElement statusLabel;
        statusLabel.x = centerX - inputW/2;
        statusLabel.y = startY + (inputH + gap)*3 + 90;
        statusLabel.w = inputW;
        statusLabel.h = 20;
        statusLabel.text = networkStatus;
        statusLabel.isLabel = true;
        statusLabel.customColor = glm::vec4(1.0f, 0.7f, 0.3f, 1.0f);
        elements.push_back(statusLabel);
    }
    
    // Join button
    UIElement joinBtn;
    joinBtn.x = centerX - btnW/2;
    joinBtn.y = startY + (inputH + gap)*3 + 110;
    joinBtn.w = btnW;
    joinBtn.h = btnH;
    joinBtn.text = "JOIN";
    joinBtn.onClick = [this]() {
        if (onJoinGame) {
            int port = std::stoi(serverPort.empty() ? "25565" : serverPort);
            // Save last used values
            Settings::instance().lastPlayerName = playerName;
            Settings::instance().lastServerAddress = serverAddress;
            Settings::instance().lastServerPort = port;
            onJoinGame(playerName, serverAddress, port);
        }
    };
    elements.push_back(joinBtn);
    
    // Back button
    UIElement backBtn;
    backBtn.x = centerX - btnW/2;
    backBtn.y = startY + (inputH + gap)*3 + 110 + btnH + gap;
    backBtn.w = btnW;
    backBtn.h = btnH;
    backBtn.text = "BACK";
    backBtn.onClick = [this]() { setMenuState(MenuState::MULTIPLAYER); };
    elements.push_back(backBtn);
}

void UIManager::setupAboutMenu() {
    elements.clear();
    
    float centerX = width / 2.0f;
    float startY = 80.0f;
    float lineHeight = 35.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 40;
    title.text = "About Bettercraft";
    title.isHeader = true;
    elements.push_back(title);
    startY += 70.0f;
    
    // Project description
    UIElement desc1;
    desc1.x = centerX - 200;
    desc1.y = startY;
    desc1.w = 400;
    desc1.h = 25;
    desc1.text = "A Minecraft clone written in C++";
    desc1.isLabel = true;
    desc1.customColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    elements.push_back(desc1);
    startY += lineHeight;
    
    UIElement desc2;
    desc2.x = centerX - 200;
    desc2.y = startY;
    desc2.w = 400;
    desc2.h = 25;
    desc2.text = "Built with OpenGL 4.5, GLFW, GLM";
    desc2.isLabel = true;
    desc2.customColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    elements.push_back(desc2);
    startY += lineHeight * 2;
    
    // Developer section
    UIElement devHeader;
    devHeader.x = centerX - 100;
    devHeader.y = startY;
    devHeader.w = 200;
    devHeader.h = 30;
    devHeader.text = "Developer";
    devHeader.isHeader = true;
    devHeader.customColor = glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);
    elements.push_back(devHeader);
    startY += 50.0f;
    
    UIElement devName;
    devName.x = centerX - 150;
    devName.y = startY;
    devName.w = 300;
    devName.h = 25;
    devName.text = "Krupanjac";
    devName.isLabel = true;
    devName.customColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    elements.push_back(devName);
    startY += lineHeight * 1.5f;
    
    // Links section
    UIElement linksHeader;
    linksHeader.x = centerX - 100;
    linksHeader.y = startY;
    linksHeader.w = 200;
    linksHeader.h = 30;
    linksHeader.text = "Links";
    linksHeader.isHeader = true;
    linksHeader.customColor = glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);
    elements.push_back(linksHeader);
    startY += 50.0f;
    
    UIElement website;
    website.x = centerX - 150;
    website.y = startY;
    website.w = 300;
    website.h = 25;
    website.text = "Website: krupanjac.dev";
    website.isLabel = true;
    website.customColor = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    elements.push_back(website);
    startY += lineHeight;
    
    UIElement github;
    github.x = centerX - 200;
    github.y = startY;
    github.w = 400;
    github.h = 25;
    github.text = "GitHub: Krupanjac/minecraft-cpp";
    github.isLabel = true;
    github.customColor = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    elements.push_back(github);
    startY += lineHeight * 2;
    
    // Version info
    UIElement version;
    version.x = centerX - 100;
    version.y = startY;
    version.w = 200;
    version.h = 25;
    version.text = "Version 1.0";
    version.isLabel = true;
    version.customColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    elements.push_back(version);
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { setMenuState(MenuState::MAIN_MENU); };
    elements.push_back(back);
}

void UIManager::setupAudioSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    float centerX = width / 2.0f;
    float startY = 100.0f;
    float lineHeight = 55.0f;
    float sliderW = 400.0f;
    float sliderH = 30.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 50;
    title.text = "AUDIO SETTINGS";
    elements.push_back(title);
    startY += 70.0f;
    
    // Master Volume
    UIElement master;
    master.x = centerX - sliderW/2;
    master.y = startY;
    master.w = sliderW;
    master.h = sliderH;
    master.text = "Master Volume: " + std::to_string(static_cast<int>(s.masterVolume * 100)) + "%";
    master.isSlider = true;
    master.valueRef = &s.masterVolume;
    master.minVal = 0.0f;
    master.maxVal = 1.0f;
    elements.push_back(master);
    startY += lineHeight;
    
    // Music Volume
    UIElement music;
    music.x = centerX - sliderW/2;
    music.y = startY;
    music.w = sliderW;
    music.h = sliderH;
    music.text = "Music Volume: " + std::to_string(static_cast<int>(s.musicVolume * 100)) + "%";
    music.isSlider = true;
    music.valueRef = &s.musicVolume;
    music.minVal = 0.0f;
    music.maxVal = 1.0f;
    elements.push_back(music);
    startY += lineHeight;
    
    // Sound Effects Volume
    UIElement sounds;
    sounds.x = centerX - sliderW/2;
    sounds.y = startY;
    sounds.w = sliderW;
    sounds.h = sliderH;
    sounds.text = "Sound Effects: " + std::to_string(static_cast<int>(s.soundVolume * 100)) + "%";
    sounds.isSlider = true;
    sounds.valueRef = &s.soundVolume;
    sounds.minVal = 0.0f;
    sounds.maxVal = 1.0f;
    elements.push_back(sounds);
    startY += lineHeight;
    
    // Ambient Volume
    UIElement ambient;
    ambient.x = centerX - sliderW/2;
    ambient.y = startY;
    ambient.w = sliderW;
    ambient.h = sliderH;
    ambient.text = "Ambient Volume: " + std::to_string(static_cast<int>(s.ambientVolume * 100)) + "%";
    ambient.isSlider = true;
    ambient.valueRef = &s.ambientVolume;
    ambient.minVal = 0.0f;
    ambient.maxVal = 1.0f;
    elements.push_back(ambient);
    startY += lineHeight + 20.0f;
    
    // Back button
    UIElement back;
    back.x = centerX - 150;
    back.y = startY;
    back.w = 300;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { 
        // Apply audio settings
        auto& settings = Settings::instance();
        Audio::AudioManager::instance().setMasterVolume(settings.masterVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, settings.musicVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, settings.soundVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, settings.ambientVolume);
        Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::WEATHER, settings.ambientVolume);
        setMenuState(MenuState::SETTINGS); 
    };
    elements.push_back(back);
}

void UIManager::setupPlayerSettingsMenu() {
    elements.clear();
    
    auto& s = Settings::instance();
    
    float centerX = width / 2.0f;
    float startY = 80.0f;
    float lineHeight = 50.0f;
    float btnW = 280.0f;
    float btnH = 40.0f;
    
    // Title
    UIElement title;
    title.x = centerX - 150;
    title.y = startY;
    title.w = 300;
    title.h = 40;
    title.text = "Player Settings";
    title.isHeader = true;
    elements.push_back(title);
    startY += 70.0f;
    
    // Load preview model on menu setup
    loadPreviewModel(s.playerModelIndex);
    
    // Nickname input
    UIElement nicknameLabel;
    nicknameLabel.x = 80.0f;
    nicknameLabel.y = startY;
    nicknameLabel.w = 150;
    nicknameLabel.h = 30;
    nicknameLabel.text = "Nickname:";
    nicknameLabel.isLabel = true;
    elements.push_back(nicknameLabel);
    
    UIElement nicknameInput;
    nicknameInput.x = 240.0f;
    nicknameInput.y = startY - 5;
    nicknameInput.w = 240;
    nicknameInput.h = btnH;
    nicknameInput.text = s.playerNickname;
    nicknameInput.isInput = true;
    nicknameInput.textRef = &s.playerNickname;
    nicknameInput.tooltip = "Your in-game name";
    elements.push_back(nicknameInput);
    startY += lineHeight;
    
    // Player Model selection
    UIElement modelLabel;
    modelLabel.x = 80.0f;
    modelLabel.y = startY;
    modelLabel.w = 150;
    modelLabel.h = 30;
    modelLabel.text = "Player Model:";
    modelLabel.isLabel = true;
    elements.push_back(modelLabel);
    startY += lineHeight * 0.7f;
    
    // Model buttons - cycle through available models
    for (int i = 0; i < Settings::NUM_PLAYER_MODELS; i++) {
        UIElement modelBtn;
        modelBtn.x = 80.0f;
        modelBtn.y = startY;
        modelBtn.w = btnW;
        modelBtn.h = btnH;
        
        std::string prefix = (s.playerModelIndex == i) ? "> " : "  ";
        std::string suffix = (s.playerModelIndex == i) ? " <" : "";
        modelBtn.text = prefix + std::string(Settings::PLAYER_MODEL_NAMES[i]) + suffix;
        
        if (s.playerModelIndex == i) {
            modelBtn.customColor = glm::vec4(0.3f, 0.6f, 0.3f, 1.0f);
        }
        
        int modelIndex = i;
        modelBtn.onClick = [this, modelIndex]() {
            Settings::instance().playerModelIndex = modelIndex;
            if (onSettingsChanged) onSettingsChanged();
            setupPlayerSettingsMenu(); // Refresh to show selection and preview
        };
        
        elements.push_back(modelBtn);
        startY += btnH + 5.0f;
    }
    
    startY += 20.0f;
    
    // Back button
    UIElement back;
    back.x = centerX - 100;
    back.y = height - 80;
    back.w = 200;
    back.h = 45;
    back.text = "Back";
    back.onClick = [this]() { 
        Settings::instance().save();
        setMenuState(MenuState::SETTINGS); 
    };
    elements.push_back(back);
}

void UIManager::addChatMessage(const std::string& senderName, const std::string& message) {
    ChatEntry entry;
    entry.playerName = senderName;
    entry.message = message;
    entry.timestamp = 0.0f; // Will be updated in render
    chatMessages.push_back(entry);
    
    // Keep only last N messages based on settings
    int maxMessages = Settings::instance().maxChatMessages;
    while (chatMessages.size() > static_cast<size_t>(maxMessages)) {
        chatMessages.erase(chatMessages.begin());
    }
}

void UIManager::openChat() {
    chatInput.clear();
    currentMenuState = MenuState::CHAT;
}

void UIManager::closeChat() {
    chatInput.clear();
    currentMenuState = MenuState::NONE;
}

void UIManager::renderChat() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    float chatX = 10.0f;
    float chatY = height - 150.0f; // Above hotbar
    float lineHeight = 20.0f;
    float textScale = 1.5f;
    
    // Use settings for fade delay
    float fadeDelay = Settings::instance().chatFadeDelay;
    
    // Update timestamps and render recent messages
    int visibleCount = 0;
    int maxVisible = 10;
    
    // Render messages from bottom to top (newest at bottom)
    for (int i = static_cast<int>(chatMessages.size()) - 1; i >= 0 && visibleCount < maxVisible; i--) {
        auto& msg = chatMessages[i];
        msg.timestamp += 0.016f; // Approximate frame time
        
        // Only show recent messages when chat is closed, show all when open
        bool shouldShow = (currentMenuState == MenuState::CHAT) || (msg.timestamp < fadeDelay);
        
        if (shouldShow) {
            float alpha = 1.0f;
            if (currentMenuState != MenuState::CHAT && msg.timestamp > fadeDelay - 2.0f) {
                alpha = (fadeDelay - msg.timestamp) / 2.0f;
            }
            
            float y = chatY - (visibleCount * lineHeight);
            
            // Draw background for readability
            std::string fullMsg = "<" + msg.playerName + "> " + msg.message;
            float msgWidth = fullMsg.length() * 6.0f * textScale + 10.0f;
            drawRect(chatX - 5, y - 2, msgWidth, lineHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f * alpha));
            
            // Draw player name in yellow, message in white
            std::string nameStr = "<" + msg.playerName + "> ";
            drawText(chatX, y, textScale, nameStr, glm::vec4(1.0f, 1.0f, 0.3f, alpha));
            
            float nameWidth = nameStr.length() * 6.0f * textScale;
            drawText(chatX + nameWidth, y, textScale, msg.message, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
            
            visibleCount++;
        }
    }
    
    // Render chat input when chat is open
    if (currentMenuState == MenuState::CHAT) {
        float inputY = chatY + lineHeight + 5.0f;
        float inputW = width * 0.4f;
        float inputH = 25.0f;
        
        // Input background
        drawRect(chatX - 5, inputY, inputW, inputH, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
        
        // Input text with cursor
        std::string displayText = chatInput + "_";
        drawText(chatX, inputY + 4, textScale, displayText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        
        // Help text
        drawText(chatX, inputY + inputH + 5, 1.2f, "Press ENTER to send, ESC to cancel", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    }
    
    uiShader.unuse();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
void UIManager::loadPreviewModel(int modelIndex) {
    if (modelIndex < 0 || modelIndex >= Settings::NUM_PLAYER_MODELS) return;
    if (previewModelIndex == modelIndex && previewModel) return; // Already loaded
    
    previewModelIndex = modelIndex;
    previewRotation = 0.0f;
    
    try {
        std::string path = Settings::PLAYER_MODEL_PATHS[modelIndex];
        std::cout << "Loading preview model: " << path << std::endl;
        previewModel = std::make_shared<ModelSystem::Model>(path);
        std::cout << "Preview model loaded successfully" << std::endl;
        
        // Try to play idle animation
        auto animations = previewModel->getAnimationNames();
        std::cout << "Available animations: " << animations.size() << std::endl;
        for (const auto& anim : animations) {
            std::cout << "  - " << anim << std::endl;
            // Look for idle animation (case insensitive check)
            std::string lowerAnim = anim;
            for (auto& c : lowerAnim) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowerAnim.find("idle") != std::string::npos) {
                previewModel->playAnimation(anim, true);
                std::cout << "Playing animation: " << anim << std::endl;
                break;
            }
        }
        // If no idle found, play first animation if available
        if (previewModel->getCurrentAnimation().empty() && !animations.empty()) {
            previewModel->playAnimation(animations[0], true);
            std::cout << "Playing first animation: " << animations[0] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load preview model: " << e.what() << std::endl;
        previewModel = nullptr;
    }
}

void UIManager::renderModelPreview() {
    if (!previewModel) return;
    if (previewFBO == 0 || previewTexture == 0) return;
    
    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Bind preview FBO
    glBindFramebuffer(GL_FRAMEBUFFER, previewFBO);
    
    // Set draw buffers
    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);
    
    glViewport(0, 0, previewWidth, previewHeight);
    
    // Clear with semi-transparent dark background
    glClearColor(0.1f, 0.1f, 0.15f, 0.9f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    // Setup camera matrices for preview
    float aspectRatio = (float)previewWidth / (float)previewHeight;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    
    // Camera position - looking at model from front, positioned to see full character
    // Character models are typically 1.8 units tall, centered at origin
    glm::vec3 cameraPos = glm::vec3(0.0f, 1.2f, 2.5f);
    glm::vec3 target = glm::vec3(0.0f, 0.9f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Model transform with rotation - rotate around Y axis and face camera
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(previewRotation + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(1.0f));
    
    // Setup shader
    modelPreviewShader.use();
    modelPreviewShader.setMat4("uProjection", projection);
    modelPreviewShader.setMat4("uView", view);
    modelPreviewShader.setMat4("uPrevView", view);
    modelPreviewShader.setMat4("uPrevProjection", projection);
    
    // Simple lighting from above-front
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.5f));
    modelPreviewShader.setVec3("uLightDir", lightDir);
    modelPreviewShader.setVec3("uCameraPos", cameraPos);
    modelPreviewShader.setVec4("uBaseColor", glm::vec4(1.0f));
    modelPreviewShader.setInt("uDebugNoTexture", 0);
    modelPreviewShader.setInt("uDebugShowNormals", 0);
    
    // Draw the model
    previewModel->draw(modelPreviewShader, modelMatrix, modelMatrix);
    
    modelPreviewShader.unuse();
    
    // Unbind FBO and restore viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void UIManager::renderLoadingTip(const std::string& text) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);

    // Draw "Loading..." text above the progress bar
    float loadingScale = 2.0f;
    std::string loadingText = "Loading...";
    float loadingW = loadingText.length() * 6.0f * loadingScale;
    float loadingX = (width - loadingW) / 2.0f;
    float loadingY = height * 0.40f;
    drawText(loadingX, loadingY, loadingScale, loadingText, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Draw tip text below the progress bar
    if (!text.empty()) {
        float tipScale = 1.4f;
        float textW = text.length() * 6.0f * tipScale;
        float x = (width - textW) / 2.0f;
        float y = height * 0.65f;
        drawText(x, y, tipScale, text, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    }

    uiShader.unuse();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UIManager::renderConsole() {
    auto& console = Console::instance();
    if (!console.isVisible()) return;
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);
    
    // Console dimensions
    float consoleHeight = height * 0.4f;  // 40% of screen height
    float padding = 8.0f;
    float lineHeight = 16.0f;
    float fontScale = 1.5f;
    int visibleLines = static_cast<int>((consoleHeight - padding * 3 - lineHeight) / lineHeight);
    
    // Background
    drawRect(0, 0, (float)width, consoleHeight, glm::vec4(0.05f, 0.05f, 0.1f, 0.92f));
    
    // Border at bottom
    drawRect(0, consoleHeight - 2, (float)width, 2, glm::vec4(0.3f, 0.3f, 0.5f, 1.0f));
    
    // Input area background
    float inputY = consoleHeight - lineHeight - padding;
    drawRect(padding, inputY, width - padding * 2, lineHeight, glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
    
    // Get messages from console
    const auto& messages = console.getMessages();
    int scrollOffset = console.getScrollOffset();
    
    int startIdx = std::max(0, (int)messages.size() - visibleLines - scrollOffset);
    int endIdx = std::min((int)messages.size(), startIdx + visibleLines);
    
    float y = padding;
    for (int i = startIdx; i < endIdx; i++) {
        const auto& entry = messages[i];
        drawText(padding, y, fontScale, entry.text, entry.color);
        y += lineHeight;
    }
    
    // Scroll indicator
    if (messages.size() > (size_t)visibleLines) {
        float scrollPercent = 1.0f - (float)scrollOffset / (float)(messages.size() - visibleLines);
        float indicatorH = std::max(20.0f, (consoleHeight - lineHeight - padding * 3) * visibleLines / messages.size());
        float indicatorY = padding + (consoleHeight - lineHeight - padding * 3 - indicatorH) * scrollPercent;
        drawRect(width - 8, indicatorY, 4, indicatorH, glm::vec4(0.5f, 0.5f, 0.6f, 0.7f));
    }
    
    // Render input prompt with cursor
    std::string prompt = "> " + console.getInputText();
    
    // Cursor blink
    static float lastTime = 0.0f;
    float currentTime = static_cast<float>(glfwGetTime());
    if (fmod(currentTime, 1.0f) < 0.5f) {
        prompt.insert(2 + console.getCursorPos(), "|");
    }
    
    drawText(padding + 4, inputY + 2, fontScale, prompt, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
    
    uiShader.unuse();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}