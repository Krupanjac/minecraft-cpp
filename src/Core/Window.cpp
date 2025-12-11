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
    glfwSetWindowUserPointer(window, &windowData);
    
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
    windowData.keyCallback = callback;
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
        if (data && data->keyCallback) data->keyCallback(key, scancode, action, mods);
    });
}

void Window::setCharCallback(std::function<void(unsigned int)> callback) {
    windowData.charCallback = callback;
    glfwSetCharCallback(window, [](GLFWwindow* win, unsigned int codepoint) {
        auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
        if (data && data->charCallback) data->charCallback(codepoint);
    });
}

void Window::setCursorPosCallback(std::function<void(double, double)> callback) {
    windowData.cursorPosCallback = callback;
    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
        if (data && data->cursorPosCallback) data->cursorPosCallback(x, y);
    });
}

void Window::setMouseButtonCallback(std::function<void(int, int, int)> callback) {
    windowData.mouseButtonCallback = callback;
    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
        if (data && data->mouseButtonCallback) data->mouseButtonCallback(button, action, mods);
    });
}

void Window::setFramebufferSizeCallback(std::function<void(int, int)> callback) {
    windowData.framebufferSizeCallback = callback;
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        // Update internal size
        Window* self = static_cast<Window*>(glfwGetWindowUserPointer(win)); // This is actually WindowData*, wait.
        // We can't easily get 'this' from here unless we store it in WindowData or use a different user pointer strategy.
        // But we can just rely on the callback to update the app.
        
        auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
        if (data && data->framebufferSizeCallback) data->framebufferSizeCallback(w, h);
    });
}

void Window::setCursorMode(int mode) {
    glfwSetInputMode(window, GLFW_CURSOR, mode);
}

void Window::setVSync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::setFullscreen(int mode) {
    // mode: 0 = Windowed, 1 = Fullscreen, 2 = Borderless
    
    // Check current state to avoid redundant updates
    bool isMonitor = (glfwGetWindowMonitor(window) != nullptr);
    bool isDecorated = (glfwGetWindowAttrib(window, GLFW_DECORATED) == GLFW_TRUE);
    
    int currentMode = 0;
    if (isMonitor) currentMode = 1;
    else if (!isDecorated) currentMode = 2;
    
    if (mode == currentMode) return;

    LOG_INFO("Setting Window Mode: " + std::to_string(mode));
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);
    
    if (mode == 1) { // Fullscreen
        // Store windowed state if coming from windowed
        if (currentMode == 0) {
            glfwGetWindowPos(window, &windowedX, &windowedY);
            glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
        }
        glfwSetWindowMonitor(window, monitor, 0, 0, vidMode->width, vidMode->height, vidMode->refreshRate);
    } 
    else if (mode == 2) { // Borderless Windowed
        if (currentMode == 0) {
            glfwGetWindowPos(window, &windowedX, &windowedY);
            glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
        }
        // Borderless is just a windowed mode with no decoration and size of monitor
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(window, nullptr, 0, 0, vidMode->width, vidMode->height, vidMode->refreshRate);
    }
    else { // Windowed
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
    }
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
