#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool shouldClose() const;
    void close() { glfwSetWindowShouldClose(window, true); }
    void pollEvents();
    void swapBuffers();
    
    GLFWwindow* getNative() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    void setKeyCallback(std::function<void(int, int, int, int)> callback);
    void setCharCallback(std::function<void(unsigned int)> callback);
    void setCursorPosCallback(std::function<void(double, double)> callback);
    void setMouseButtonCallback(std::function<void(int, int, int)> callback);
    void setFramebufferSizeCallback(std::function<void(int, int)> callback);
    
    void setCursorMode(int mode);
    void setVSync(bool enabled);
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;

private:
    GLFWwindow* window;
    int width;
    int height;
    
    static void errorCallback(int error, const char* description);

    struct WindowData {
        std::function<void(int, int, int, int)> keyCallback;
        std::function<void(unsigned int)> charCallback;
        std::function<void(double, double)> cursorPosCallback;
        std::function<void(int, int, int)> mouseButtonCallback;
        std::function<void(int, int)> framebufferSizeCallback;
    };
    WindowData windowData;
};
