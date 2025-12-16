#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(const glm::vec3& position = glm::vec3(0.0f, 64.0f, 0.0f));
    ~Camera() = default;

    void update(float deltaTime);
    void processInput(bool forward, bool backward, bool left, bool right, bool up, bool down, bool sprint, bool sneak, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset);
    
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
    
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getFront() const { return front; }
    const glm::vec3& getUp() const { return up; }
    const glm::vec3& getRight() const { return right; }
    
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setYaw(float y) { yaw = y; updateCameraVectors(); }
    float getYaw() const { return yaw; }
    void setPitch(float p) { pitch = p; updateCameraVectors(); }
    float getPitch() const { return pitch; }
    void setSpeed(float speed) { movementSpeed = speed; }
    void setSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }
    void setFov(float f) { fov = f; }

    // Physics
    void toggleFlightMode() { isFlying = !isFlying; velocity = glm::vec3(0.0f); }
    bool getFlightMode() const { return isFlying; }
    void jump();
    
    // Third Person
    void toggleThirdPerson() { thirdPerson = !thirdPerson; }
    bool isThirdPerson() const { return thirdPerson; }
    void setThirdPersonDistance(float dist) { thirdPersonDistance = dist; }
    
    glm::vec3 velocity;
    bool isFlying;
    bool onGround;
    bool isSprinting = false;
    bool isSneaking = false;
    
    // View Bobbing
    float bobbingTimer = 0.0f;
    float defaultY = 0.0f; // Stores the local Y offset for the camera relative to player position
    
    // 3rd Person State
    bool thirdPerson = false;
    float thirdPersonDistance = 4.0f;


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
    float baseFov;
    
    // Physics constants
    // Physics constants
    const float ACCELERATION = 60.0f; // Snappy ground movement
    const float AIR_ACCELERATION = 100.0f; // Very high air acceleration for CS-style strafing (limited by max speed projection)
    const float FRICTION = 14.0f; // Quick stopping
    const float AIR_FRICTION = 0.0f; // No air friction (drag) in CS/Quake usually, control comes from player input
    const float MAX_SPEED = 5.0f;  // ~4.3m/s (MC Walk)
    const float SPRINT_SPEED = 7.0f; // ~5.6m/s (MC Sprint)
    const float SNEAK_SPEED = 1.3f; // ~1.3m/s (MC Sneak)

    
    void updateCameraVectors();
};
