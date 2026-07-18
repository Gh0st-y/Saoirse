#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 startPosition = glm::vec3(2.0f, 2.0f, 2.0f));

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    void processKeyboard(int direction, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;

    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
    float fov = 45.0f;

private:
    void updateCameraVectors();
};

enum CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};