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
    void pollEvents();
    void swapBuffers();
    
    GLFWwindow* getNative() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    void setKeyCallback(std::function<void(int, int, int, int)> callback);
    void setCursorPosCallback(std::function<void(double, double)> callback);
    void setMouseButtonCallback(std::function<void(int, int, int)> callback);
    void setFramebufferSizeCallback(std::function<void(int, int)> callback);
    
    void setCursorMode(int mode);
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;

private:
    GLFWwindow* window;
    int width;
    int height;
    
    static void errorCallback(int error, const char* description);
};
