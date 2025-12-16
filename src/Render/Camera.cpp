#include "Camera.h"
#include "../Util/Config.h"
#include <iostream>

Camera::Camera(const glm::vec3& position)
    : position(position),
      worldUp(0.0f, 1.0f, 0.0f),
      yaw(-90.0f),
      pitch(0.0f),
      movementSpeed(CAMERA_SPEED),
      mouseSensitivity(MOUSE_SENSITIVITY),
      fov(FOV),
      baseFov(FOV),
      velocity(0.0f),
      isFlying(false),
      onGround(false),
      isSprinting(false),
      isSneaking(false) {
    updateCameraVectors();
}

void Camera::jump() {
    if (onGround && !isFlying) {
        velocity.y = 9.0f; // Adjusted momentum for ~1.2 blocks height
        onGround = false;
    }
}

void Camera::update(float deltaTime) {
    // 1. Friction
    if (!isFlying) {
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        if (speed > 0.1f) {
            float control = (speed < FRICTION) ? FRICTION : speed; // Friction applies more at low speeds to stop
            float drop = control * (onGround ? FRICTION : AIR_FRICTION) * deltaTime;
            
            float newSpeed = speed - drop;
            if (newSpeed < 0) newSpeed = 0;
            if (newSpeed != speed) {
                newSpeed /= speed;
                velocity.x *= newSpeed;
                velocity.z *= newSpeed;
            }
        } else {
             velocity.x = 0;
             velocity.z = 0;
        }
    } else {
        // Fly mode friction
        velocity *= 0.90f; // Simple drag
    }

    // 2. View Bobbing
    if (onGround && !isFlying && glm::length(glm::vec2(velocity.x, velocity.z)) > 0.1f) {
         float bobSpeed = isSprinting ? 18.0f : 12.0f;
         bobbingTimer += deltaTime * bobSpeed;
    } else {
         // Reset smoothly to 0
         if (bobbingTimer > 0.0f) {
             bobbingTimer = 0.0f; 
         }
    }

    // 3. Dynamic FOV
    float targetFov = baseFov;
    if (isSprinting && !isFlying) targetFov += 10.0f;
    if (isFlying && isSprinting) targetFov += 15.0f;
    
    fov += (targetFov - fov) * deltaTime * 10.0f; // Smooth transition
}

void Camera::processInput(bool forward, bool backward, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, bool sprint, bool sneak, float deltaTime) {
    isSprinting = sprint;
    isSneaking = sneak;

    if (isFlying) {
        float flySpeed = sprint ? SPRINT_SPEED * 2.0f : MAX_SPEED * 1.5f;
        glm::vec3 wishDir(0.0f);
        if (forward) wishDir += front;
        if (backward) wishDir -= front;
        if (moveLeft) wishDir -= right;
        if (moveRight) wishDir += right;
        if (moveUp) wishDir += worldUp;
        if (moveDown) wishDir -= worldUp;

        if (glm::length(wishDir) > 0) {
            wishDir = glm::normalize(wishDir);
            position += wishDir * flySpeed * deltaTime;
        }
        return; 
    }

    // Walking / Running
    float currentMaxSpeed = MAX_SPEED;
    if (isSprinting) currentMaxSpeed = SPRINT_SPEED;
    if (isSneaking) currentMaxSpeed = SNEAK_SPEED;

    glm::vec3 frontFlat = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 rightFlat = glm::normalize(glm::vec3(right.x, 0.0f, right.z));
    
    glm::vec3 wishDir(0.0f);
    if (forward) wishDir += frontFlat;
    if (backward) wishDir -= frontFlat;
    if (moveLeft) wishDir -= rightFlat;
    if (moveRight) wishDir += rightFlat;
    
    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
    }
    
    // Apply Acceleration
    float currentSpeedInWishDir = glm::dot(glm::vec2(velocity.x, velocity.z), glm::vec2(wishDir.x, wishDir.z));
    float addSpeed = currentMaxSpeed - currentSpeedInWishDir;
    
    if (addSpeed > 0) {
        float accel = onGround ? ACCELERATION : AIR_ACCELERATION;
        float accelSpeed = accel * deltaTime * currentMaxSpeed;
        
        if (accelSpeed > addSpeed) accelSpeed = addSpeed;
        
        velocity.x += accelSpeed * wishDir.x;
        velocity.z += accelSpeed * wishDir.z;
    }
    
    if (moveUp) jump();
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
    if (yaw > 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    // Apply bobbing to view matrix only (visual)
    glm::vec3 viewPos = position;
    
    if (!isFlying) {
         float bobY = sin(bobbingTimer) * 0.15f; 
         // MC also does some X bobbing
         // float bobX = cos(bobbingTimer) * 0.05f; 
         viewPos.y += bobY;
         // We could also rotate view slightly for head tilt
    }
    
    return glm::lookAt(viewPos, viewPos + front, up);
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
