#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(const glm::vec3& position = glm::vec3(0.0f, 64.0f, 0.0f));
    ~Camera() = default;

    void update(float deltaTime);
    void processInput(bool forward, bool backward, bool left, bool right, bool up, bool down, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset);
    
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
    
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getFront() const { return front; }
    const glm::vec3& getUp() const { return up; }
    const glm::vec3& getRight() const { return right; }
    
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setYaw(float y) { yaw = y; updateCameraVectors(); }
    void setPitch(float p) { pitch = p; updateCameraVectors(); }
    void setSpeed(float speed) { movementSpeed = speed; }
    void setSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }
    void setFov(float f) { fov = f; }

    // Physics
    void toggleFlightMode() { isFlying = !isFlying; velocity = glm::vec3(0.0f); }
    bool getFlightMode() const { return isFlying; }
    void jump();
    
    glm::vec3 velocity;
    bool isFlying;
    bool onGround;

private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    float yaw;
    float pitch;
    
    float movementSpeed;
    float mouseSensitivity;
    float fov;
    
    void updateCameraVectors();
};
