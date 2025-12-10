#pragma once

#include <chrono>

class Time {
public:
    static Time& instance() {
        static Time time;
        return time;
    }

    void update() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        if (deltaTime <= 0.0f) deltaTime = 0.000001f; // Prevent zero delta time
        lastTime = currentTime;
        totalTime += deltaTime;
        frameCount++;
    }

    float getDeltaTime() const { return deltaTime; }
    float getTotalTime() const { return totalTime; }
    int getFrameCount() const { return frameCount; }
    float getFPS() const { return deltaTime > 0 ? 1.0f / deltaTime : 0.0f; }

    void reset() {
        lastTime = std::chrono::high_resolution_clock::now();
        totalTime = 0.0f;
        deltaTime = 0.0f;
        frameCount = 0;
    }

private:
    Time() {
        reset();
    }

    std::chrono::high_resolution_clock::time_point lastTime;
    float deltaTime = 0.0f;
    float totalTime = 0.0f;
    int frameCount = 0;
};
