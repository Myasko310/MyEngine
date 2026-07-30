#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MyEngine {

    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    struct CameraComponent : public Component {

        // Projection settings
        ProjectionType projectionType = ProjectionType::Perspective;
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float aspectRatio = 800.0f / 600.0f;

        // Orthographic settings
        float orthoSize = 5.0f;

        // Camera vectors (updated by CameraSystem)
        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

        // Cached matrices
        glm::mat4 viewMatrix = glm::mat4(1.0f);
        glm::mat4 projectionMatrix = glm::mat4(1.0f);

        bool isPrimary = true;

        glm::mat4 GetViewMatrix(const glm::vec3& position) const
        {
            return glm::lookAt(position, position + front, up);
        }

        glm::mat4 GetProjectionMatrix() const
        {
            if (projectionType == ProjectionType::Perspective)
            {
                return glm::perspective(
                    glm::radians(fov),
                    aspectRatio,
                    nearPlane,
                    farPlane
                );
            }
            else
            {
                float halfSize = orthoSize * 0.5f;
                return glm::ortho(
                    -halfSize * aspectRatio,
                    halfSize * aspectRatio,
                    -halfSize,
                    halfSize,
                    nearPlane,
                    farPlane
                );
            }
        }
    };

} // namespace MyEngine