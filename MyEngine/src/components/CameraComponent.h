#pragma once

#include <glm/glm.hpp>

namespace MyEngine
{
    struct CameraComponent
    {
        bool isPrimary = false;

        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        float yaw = -90.0f;
        float pitch = 0.0f;

        float mouseSensitivity = 0.1f;
        float mouseSmoothing = 30.0f;

        float moveSpeed = 5.0f;
        float sprintMultiplier = 3.0f;
        float slowMultiplier = 0.25f;

        bool flyMode = true;
        bool enableInput = true;
        bool enableMouseLook = true;

        glm::vec3 velocity = glm::vec3(0.0f);
        glm::vec2 smoothedMouseDelta = glm::vec2(0.0f);

        bool firstMouse = true;
        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
    };
}