#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct TransformComponent
{
    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

    // ID of the parent entity for hierarchy transforms (0 = no parent).
    // World matrices are resolved via TransformHierarchy::GetWorldMatrix.
    uint32_t parentID = 0;

    // Local matrix relative to the parent (or world if parentID == 0).
    glm::mat4 GetMatrix() const
    {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

        glm::quat rotationQuat = glm::quat(rotation);
        glm::mat4 rotationMatrix = glm::toMat4(rotationQuat);

        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        return translationMatrix * rotationMatrix * scaleMatrix;
    }
};