#include "UIManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

UIManager::UIManager() : vao(0), vbo(0), width(1280), height(720) {}

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

    // Setup quad VAO
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

    setupMainMenu();
}

void UIManager::handleResize(int w, int h) {
    width = w;
    height = h;
    // Re-setup menu to center elements
    if (menuOpen) setupMainMenu(); 
}

void UIManager::setMenuState(bool isOpen) {
    menuOpen = isOpen;
    if (menuOpen) {
        setupMainMenu();
    }
}

void UIManager::setupMainMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 200.0f;
    float btnH = 40.0f;
    float gap = 10.0f;

    // Resume
    elements.push_back({cx - btnW/2, cy - btnH - gap, btnW, btnH, "RESUME", false, [this]() {
        setMenuState(false);
    }});

    // Settings
    elements.push_back({cx - btnW/2, cy, btnW, btnH, "SETTINGS", false, [this]() {
        setupSettingsMenu();
    }});

    // Exit
    elements.push_back({cx - btnW/2, cy + btnH + gap, btnW, btnH, "EXIT", false, []() {
        exit(0);
    }});
}

void UIManager::setupSettingsMenu() {
    elements.clear();
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float btnW = 300.0f;
    float btnH = 30.0f;
    float gap = 10.0f;
    float startY = cy - 150.0f;

    auto& s = Settings::instance();

    // Render Distance
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "RENDER DIST: " + std::to_string(s.renderDistance), false, nullptr, true, nullptr, &s.renderDistance, 2.0f, 32.0f});
    startY += btnH + gap;

    // FOV
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "FOV: " + std::to_string((int)s.fov), false, nullptr, true, &s.fov, nullptr, 30.0f, 110.0f});
    startY += btnH + gap;

    // Mouse Sensitivity
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "SENSITIVITY: " + std::to_string(s.mouseSensitivity).substr(0, 4), false, nullptr, true, &s.mouseSensitivity, nullptr, 0.01f, 1.0f});
    startY += btnH + gap;

    // AO Strength
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "AO STRENGTH: " + std::to_string(s.aoStrength).substr(0, 3), false, nullptr, true, &s.aoStrength, nullptr, 0.0f, 2.0f});
    startY += btnH + gap;

    // Back
    startY += 20.0f;
    elements.push_back({cx - btnW/2, startY, btnW, btnH, "BACK", false, [this]() {
        Settings::instance().save();
        setupMainMenu();
    }});
}

void UIManager::update(float deltaTime, double mouseX, double mouseY, bool mousePressed) {
    if (!menuOpen) return;

    std::function<void()> pendingClick = nullptr;

    for (auto& el : elements) {
        // Hit test
        if (mouseX >= el.x && mouseX <= el.x + el.w &&
            mouseY >= el.y && mouseY <= el.y + el.h) {
            
            el.isHovered = true;
            
            if (mousePressed) {
                if (el.isSlider) {
                    // Calculate slider value
                    float pct = (float)(mouseX - el.x) / el.w;
                    float val = el.minVal + pct * (el.maxVal - el.minVal);
                    val = std::max(el.minVal, std::min(el.maxVal, val));
                    
                    if (el.intValueRef) {
                        *el.intValueRef = (int)val;
                        el.text = "RENDER DIST: " + std::to_string(*el.intValueRef);
                    } else if (el.valueRef) {
                        *el.valueRef = val;
                        // Update text
                        if (el.text.find("FOV") != std::string::npos)
                            el.text = "FOV: " + std::to_string((int)*el.valueRef);
                        else if (el.text.find("SENSITIVITY") != std::string::npos)
                            el.text = "SENSITIVITY: " + std::to_string(*el.valueRef).substr(0, 4);
                        else if (el.text.find("AO") != std::string::npos)
                            el.text = "AO STRENGTH: " + std::to_string(*el.valueRef).substr(0, 3);
                    }
                    
                    if (onSettingsChanged) onSettingsChanged();
                } else if (el.onClick) {
                    pendingClick = el.onClick;
                    break; // Stop processing to avoid issues with vector modification
                }
            }
        } else {
            el.isHovered = false;
        }
    }

    if (pendingClick) {
        pendingClick();
    }
}

void UIManager::render() {
    if (!menuOpen) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    uiShader.setMat4("uProjection", projection);

    // Draw semi-transparent background
    drawRect(0, 0, (float)width, (float)height, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));

    for (const auto& el : elements) {
        glm::vec4 color = el.isHovered ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f) : glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
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
        float textW = el.text.length() * 8.0f * textScale; // Approx width
        float textX = el.x + (el.w - textW) / 2.0f;
        float textY = el.y + (el.h - 8.0f * textScale) / 2.0f;
        drawText(textX, textY, textScale, el.text, glm::vec4(1.0f));
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

// Very basic vector font implementation
void UIManager::drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color) {
    // We'll use GL_LINES for text
    // This is inefficient (many draw calls) but fine for a simple menu
    // Ideally we'd batch lines
    
    // For simplicity, let's just draw rects for now or implement a few chars
    // Actually, let's try to use the existing drawRect to draw "pixels" of text? No, too slow.
    // Let's just use a simple line drawer.
    
    // Since I can't easily add a line shader/buffer right now without more boilerplate,
    // I'll skip the complex text and just rely on the button colors/shapes for now?
    // NO, the user wants a menu. I MUST render text.
    
    // I'll use the same shader but with GL_LINES and a different VAO/VBO for lines?
    // Or just use thin rectangles for lines! Yes.
    
    float cursorX = x;
    for (char c : text) {
        // Draw char c at cursorX, y
        // 5x7 pixel font style, using rectangles
        
        // Helper to draw a "pixel"
        auto p = [&](float dx, float dy) {
            drawRect(cursorX + dx*scale, y + dy*scale, scale, scale, color);
        };
        
        // Very basic font map (A-Z, 0-9)
        // This is tedious to write out fully, so I'll implement a minimal set
        // or just use a "placeholder" text renderer that draws a box.
        
        // Actually, let's just implement a few key chars for the menu:
        // R, E, S, U, M, T, I, N, G, X, O, V, D, A, L, B, C, K, F, :, 0-9, .
        
        // To save time/tokens, I will implement a "segment" style font.
        // 7 segments + diagonals?
        
        // Let's try a different approach:
        // Just draw the text string as a unique hash color box for now? No that's bad.
        
        // Okay, I will implement a very crude "bitmap" font where I define the bits in code.
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
        
        // Numbers 0-9
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

        int idx = -1;
        bool isNum = false;
        
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a';
        else if (c >= '0' && c <= '9') { idx = c - '0'; isNum = true; }
        else if (c == ':') {
             p(2, 1); p(2, 3);
        } else if (c == '.') {
             p(2, 4);
        }
        
        if (idx >= 0) {
            const uint8_t* glyph = isNum ? nums[idx] : font[idx];
            for (int col = 0; col < 5; col++) {
                uint8_t colData = glyph[col];
                for (int row = 0; row < 7; row++) { // 7 rows high
                    if ((colData >> row) & 1) {
                        p(col, row); // Draw pixel
                    }
                }
            }
        }
        
        cursorX += 6 * scale; // Advance cursor
    }
}
