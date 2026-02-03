#define NOMINMAX
#include "TimeOfDaySystem.h"

#include "../Core/Window.h"
#include "../Network/NetworkManager.h"
#include "../Render/Camera.h"
#include "../Render/Renderer.h"
#include "../UI/UIManager.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

TimeOfDaySystem::TimeOfDaySystem(UIManager& uiManagerRef,
                                 Renderer& rendererRef,
                                 Network::NetworkManager& networkManagerRef,
                                 Camera& cameraRef)
    : uiManager(uiManagerRef),
      renderer(rendererRef),
      networkManager(networkManagerRef),
      camera(cameraRef) {
}

void TimeOfDaySystem::update(float deltaTime, bool skipPlayerControls, Window& window) {
    bool canControlTime = !networkManager.isOnline() || networkManager.isHost();

    if (!uiManager.isDayNightPaused) {
        uiManager.timeOfDay += deltaTime * 10.0f;
    }

    bool timeChanged = false;
    if (canControlTime && !skipPlayerControls) {
        if (window.isKeyPressed(GLFW_KEY_RIGHT)) {
            uiManager.timeOfDay += deltaTime * 100.0f;
            timeChanged = true;
        }
        if (window.isKeyPressed(GLFW_KEY_LEFT)) {
            uiManager.timeOfDay -= deltaTime * 100.0f;
            timeChanged = true;
        }
    }

    if (uiManager.timeOfDay >= kDayDuration) uiManager.timeOfDay -= kDayDuration;
    if (uiManager.timeOfDay < 0.0f) uiManager.timeOfDay += kDayDuration;

    timeSyncTimer += deltaTime;
    if (networkManager.isHost() && (timeChanged || timeSyncTimer >= 2.0f)) {
        networkManager.sendTimeSync(uiManager.timeOfDay, uiManager.isDayNightPaused);
        timeSyncTimer = 0.0f;
    }

    float angle = (uiManager.timeOfDay / kDayDuration) * glm::two_pi<float>();

    float sunX = std::cos(angle);
    float sunY = std::sin(angle);
    float sunZ = 0.2f;

    glm::vec3 sunDir = glm::normalize(glm::vec3(sunX, sunY, sunZ));

    if (sunY < -0.1f) {
        glm::vec3 moonDir = -sunDir;
        renderer.setLightDirection(moonDir);
    } else {
        renderer.setLightDirection(sunDir);
    }

    glm::vec3 dayColor(0.53f, 0.81f, 0.92f);
    glm::vec3 nightColor(0.05f, 0.05f, 0.1f);
    glm::vec3 sunsetColor(0.8f, 0.4f, 0.2f);

    glm::vec3 currentSkyColor;

    if (sunY > 0.2f) {
        currentSkyColor = dayColor;
    } else if (sunY < -0.2f) {
        currentSkyColor = nightColor;
    } else {
        float t = (sunY + 0.2f) / 0.4f;
        if (sunX > 0) {
            currentSkyColor = glm::mix(nightColor, dayColor, t);
            float glow = 1.0f - std::abs(t - 0.5f) * 2.0f;
            currentSkyColor = glm::mix(currentSkyColor, sunsetColor, glow * 0.5f);
        } else {
            currentSkyColor = glm::mix(nightColor, dayColor, t);
            float glow = 1.0f - std::abs(t - 0.5f) * 2.0f;
            currentSkyColor = glm::mix(currentSkyColor, sunsetColor, glow * 0.5f);
        }
    }

    if (camera.getPosition().y < 40.0f) {
        float depthFactor = std::clamp((40.0f - camera.getPosition().y) / 20.0f, 0.0f, 1.0f);
        currentSkyColor = glm::mix(currentSkyColor, glm::vec3(0.0f), depthFactor);
    }

    renderer.setSkyColor(currentSkyColor);
    renderer.setSunHeight(sunY);
    renderer.setTimeOfDay(uiManager.timeOfDay);
}
