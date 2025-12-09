#pragma once

#include "../Render/Shader.h"
#include "../Core/Settings.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

struct UIElement {
    float x, y, w, h;
    std::string text;
    bool isHovered = false;
    std::function<void()> onClick;
    
    // For sliders
    bool isSlider = false;
    float* valueRef = nullptr;
    int* intValueRef = nullptr; // Added for integer support
    float minVal = 0.0f;
    float maxVal = 1.0f;
};

class UIManager {
public:
    UIManager();
    ~UIManager() = default;

    void initialize(int windowWidth, int windowHeight);
    void render();
    void update(float deltaTime, double mouseX, double mouseY, bool mousePressed);
    void handleResize(int width, int height);

    void setMenuState(bool isOpen);
    bool isMenuOpen() const { return menuOpen; }

    void setOnSettingsChanged(std::function<void()> callback) { onSettingsChanged = callback; }

private:
    bool menuOpen = false;
    int width, height;
    Shader uiShader;
    GLuint vao, vbo;
    
    std::vector<UIElement> elements;
    std::function<void()> onSettingsChanged;
    
    void setupMainMenu();
    void setupSettingsMenu();
    
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawText(float x, float y, float scale, const std::string& text, const glm::vec4& color);
    
    // Helper for vector font
    void drawLine(float x1, float y1, float x2, float y2, const glm::vec4& color);
};
