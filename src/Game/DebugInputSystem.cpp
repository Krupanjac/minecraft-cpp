#include "DebugInputSystem.h"

#include "../Core/Settings.h"
#include "../Core/Window.h"
#include "../UI/UIManager.h"

DebugInputSystem::DebugInputSystem(UIManager& uiManagerRef)
    : uiManager(uiManagerRef) {
}

void DebugInputSystem::update(bool skipPlayerControls, Window& window) {
    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F1)) {
        if (!f1Pressed) {
            uiManager.toggleDebug();
            f1Pressed = true;
        }
    } else {
        f1Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F2)) {
        if (!f2Pressed) {
            uiManager.isDayNightPaused = !uiManager.isDayNightPaused;
            f2Pressed = true;
        }
    } else {
        f2Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F3)) {
        if (!f3Pressed) {
            Settings::instance().enableShadows = !Settings::instance().enableShadows;
            f3Pressed = true;
        }
    } else {
        f3Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F4)) {
        if (!f4Pressed) {
            Settings::instance().debugShowTAA = !Settings::instance().debugShowTAA;
            f4Pressed = true;
        }
    } else {
        f4Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F8)) {
        if (!f8Pressed) {
            Settings::instance().debugNoTexture = !Settings::instance().debugNoTexture;
            f8Pressed = true;
        }
    } else {
        f8Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F6)) {
        if (!f6Pressed) {
            Settings::instance().debugWireframe = !Settings::instance().debugWireframe;
            f6Pressed = true;
        }
    } else {
        f6Pressed = false;
    }

    if (!skipPlayerControls && window.isKeyPressed(GLFW_KEY_F7)) {
        if (!f7Pressed) {
            Settings::instance().debugShowNormals = !Settings::instance().debugShowNormals;
            f7Pressed = true;
        }
    } else {
        f7Pressed = false;
    }
}
