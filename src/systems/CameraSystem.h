#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

class Scene;

namespace MyEngine
{
    class CameraSystem
    {
    public:
        void Update(Scene& scene, GLFWwindow* window, float deltaTime, float aspectRatio);

        // Apply interpolation to smooth camera between fixed-timestep updates
        // alpha should be in range [0, 1) representing position within the next physics frame
        void ApplyCameraInterpolation(Scene& scene, float alpha);

        const glm::mat4& GetViewMatrix() const;
        const glm::mat4& GetProjectionMatrix() const;

    private:
        // Store camera state from previous frame for interpolation
        struct CameraState
        {
            glm::vec3 position = glm::vec3(0.0f);
            float yaw = -90.0f;
            float pitch = 0.0f;
        };

        CameraState m_PreviousCameraState;

        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    };
}