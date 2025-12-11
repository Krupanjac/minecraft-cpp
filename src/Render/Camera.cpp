#include "Camera.h"
#include "../Util/Config.h"

Camera::Camera(const glm::vec3& position)
    : position(position),
      worldUp(0.0f, 1.0f, 0.0f),
      yaw(-90.0f),
      pitch(0.0f),
      movementSpeed(CAMERA_SPEED),
      mouseSensitivity(MOUSE_SENSITIVITY),
      fov(FOV),
      velocity(0.0f),
      isFlying(true),
      onGround(false) {
    updateCameraVectors();
}

void Camera::jump() {
    if (onGround && !isFlying) {
        velocity.y = 7.0f; // Jump force
        onGround = false;
    }
}

void Camera::update(float /*deltaTime*/) {
    // Can be used for camera shake, smooth movement, etc.
}

void Camera::processInput(bool forward, bool backward, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, float deltaTime) {
    if (isFlying) {
        float vel = movementSpeed * deltaTime;
        if (forward) position += front * vel;
        if (backward) position -= front * vel;
        if (moveLeft) position -= this->right * vel;
        if (moveRight) position += this->right * vel;
        if (moveUp) position += worldUp * vel;
        if (moveDown) position -= worldUp * vel;
    } else {
        // Walking
        glm::vec3 frontFlat = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
        glm::vec3 rightFlat = glm::normalize(glm::vec3(this->right.x, 0.0f, this->right.z));
        
        glm::vec3 wishDir(0.0f);
        if (forward) wishDir += frontFlat;
        if (backward) wishDir -= frontFlat;
        if (moveLeft) wishDir -= rightFlat;
        if (moveRight) wishDir += rightFlat;
        
        if (glm::length(wishDir) > 0.0f) {
            wishDir = glm::normalize(wishDir);
            velocity.x = wishDir.x * movementSpeed * 0.5f; // Walk speed
            velocity.z = wishDir.z * movementSpeed * 0.5f;
        } else {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }
        
        if (moveUp) jump();
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
    // Wrap yaw to avoid floating point precision issues with large values
    if (yaw > 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    
    // Constrain pitch
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
    
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, NEAR_PLANE, FAR_PLANE);
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}
