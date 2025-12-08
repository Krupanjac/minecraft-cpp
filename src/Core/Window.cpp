#include "Window.h"
#include "Logger.h"
#include <stdexcept>

Window::Window(int width, int height, const std::string& title)
    : width(width), height(height) {
    
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    glfwSwapInterval(1);  // VSync

    LOG_INFO("Window created: " + std::to_string(width) + "x" + std::to_string(height));
    LOG_INFO("OpenGL Version: " + std::string((const char*)glGetString(GL_VERSION)));
    LOG_INFO("GPU: " + std::string((const char*)glGetString(GL_RENDERER)));
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(window);
}

void Window::setKeyCallback(std::function<void(int, int, int, int)> callback) {
    // Store callback in user pointer and set GLFW callback
    auto callbackPtr = new std::function<void(int, int, int, int)>(callback);
    glfwSetWindowUserPointer(window, callbackPtr);
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto cb = static_cast<std::function<void(int, int, int, int)>*>(glfwGetWindowUserPointer(win));
        if (cb) (*cb)(key, scancode, action, mods);
    });
}

void Window::setCursorPosCallback(std::function<void(double, double)> callback) {
    auto callbackPtr = new std::function<void(double, double)>(callback);
    glfwSetWindowUserPointer(window, callbackPtr);
    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        auto cb = static_cast<std::function<void(double, double)>*>(glfwGetWindowUserPointer(win));
        if (cb) (*cb)(x, y);
    });
}

void Window::setMouseButtonCallback(std::function<void(int, int, int)> callback) {
    auto callbackPtr = new std::function<void(int, int, int)>(callback);
    glfwSetWindowUserPointer(window, callbackPtr);
    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        auto cb = static_cast<std::function<void(int, int, int)>*>(glfwGetWindowUserPointer(win));
        if (cb) (*cb)(button, action, mods);
    });
}

void Window::setFramebufferSizeCallback(std::function<void(int, int)> callback) {
    auto callbackPtr = new std::function<void(int, int)>(callback);
    glfwSetWindowUserPointer(window, callbackPtr);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        auto cb = static_cast<std::function<void(int, int)>*>(glfwGetWindowUserPointer(win));
        if (cb) (*cb)(w, h);
    });
}

void Window::setCursorMode(int mode) {
    glfwSetInputMode(window, GLFW_CURSOR, mode);
}

bool Window::isKeyPressed(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool Window::isMouseButtonPressed(int button) const {
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

void Window::errorCallback(int error, const char* description) {
    LOG_ERROR("GLFW Error " + std::to_string(error) + ": " + description);
}
