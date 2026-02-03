#pragma once

#include <glm/glm.hpp>

class UIManager;
class Renderer;
class Camera;
class Window;

namespace Network {
class NetworkManager;
}

class TimeOfDaySystem {
public:
    static constexpr float kDayDuration = 2400.0f;

    TimeOfDaySystem(UIManager& uiManager,
                    Renderer& renderer,
                    Network::NetworkManager& networkManager,
                    Camera& camera);

    void update(float deltaTime, bool skipPlayerControls, Window& window);

private:
    UIManager& uiManager;
    Renderer& renderer;
    Network::NetworkManager& networkManager;
    Camera& camera;

    float timeSyncTimer = 0.0f;
};
