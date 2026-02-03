#pragma once

class UIManager;
class Window;

class DebugInputSystem {
public:
    DebugInputSystem(UIManager& uiManager);

    void update(bool skipPlayerControls, Window& window);

private:
    UIManager& uiManager;

    bool f1Pressed = false;
    bool f2Pressed = false;
    bool f3Pressed = false;
    bool f4Pressed = false;
    bool f6Pressed = false;
    bool f7Pressed = false;
    bool f8Pressed = false;
};
