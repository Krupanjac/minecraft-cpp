#include "Camera.h"
#include "../Util/Config.h"

Camera::Camera(const glm::vec3& position)
    : position(position),
      worldUp(0.0f, 1.0f, 0.0f),
      yaw(-90.0f),
      pitch(0.0f),
      movementSpeed(CAMERA_SPEED),
      mouseSensitivity(MOUSE_SENSITIVITY),
      fov(FOV) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    // Can be used for camera shake, smooth movement, etc.
}

void Camera::processInput(bool forward, bool backward, bool left, bool right, bool up, bool down, float deltaTime) {
    float velocity = movementSpeed * deltaTime;
    
    if (forward)
        position += front * velocity;
    if (backward)
        position -= front * velocity;
    if (left)
        position -= this->right * velocity;
    if (right)
        position += this->right * velocity;
    if (up)
        position += worldUp * velocity;
    if (down)
        position -= worldUp * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
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
