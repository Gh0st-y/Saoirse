#include "Camera.h"

Camera::Camera(glm::vec3 startPosition)
    : position(startPosition),
    worldUp(0.0f, 0.0f, 1.0f),   // Z-up, matching our existing scene setup from Section 6
    yaw(-135.0f),               // facing roughly back toward the origin from (2,2,2)
    pitch(-30.0f) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
    proj[1][1] *= -1; // Vulkan Y-flip, same fix as Section 6
    return proj;
}

void Camera::processKeyboard(int direction, float deltaTime) {
    float velocity = movementSpeed * deltaTime;

    if (direction == FORWARD)  position += front * velocity;
    if (direction == BACKWARD) position -= front * velocity;
    if (direction == LEFT)     position -= right * velocity;
    if (direction == RIGHT)    position += right * velocity;
    if (direction == UP)       position += worldUp * velocity;
    if (direction == DOWN)     position -= worldUp * velocity;
}

void Camera::processMouseMovement(float xOffset, float yOffset) {
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    yaw += xOffset;
    pitch += yOffset;

    // clamp pitch to avoid flipping upside down at the poles
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.z = sin(glm::radians(pitch));
    front = glm::normalize(newFront);

    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}