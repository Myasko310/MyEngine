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

        const glm::mat4& GetViewMatrix() const;
        const glm::mat4& GetProjectionMatrix() const;

    private:
        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    };
}