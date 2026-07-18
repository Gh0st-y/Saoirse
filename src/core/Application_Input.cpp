// Keyboard/mouse input handling for the free-fly camera
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"


// --- Process Input ---
void Application::processInput(float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.processKeyboard(DOWN, deltaTime);
}


// --- Mouse Callback ---
void Application::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->processMouseInput(xpos, ypos);
}


// --- Process Mouse Input ---
void Application::processMouseInput(double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = static_cast<float>(xpos);
        lastMouseY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xOffset = invertMouseX
        ? (static_cast<float>(xpos) - lastMouseX)
        : (lastMouseX - static_cast<float>(xpos));
    float yOffset = invertMouseY
        ? (static_cast<float>(ypos) - lastMouseY)
        : (lastMouseY - static_cast<float>(ypos));

    lastMouseX = static_cast<float>(xpos);
    lastMouseY = static_cast<float>(ypos);

    camera.processMouseMovement(xOffset, yOffset);
}
