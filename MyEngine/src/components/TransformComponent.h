#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MyEngine {

    struct TransformComponent : public Component {

        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f); // Euler angles in degrees
        glm::vec3 scale = glm::vec3(1.0f);

        glm::mat4 GetModelMatrix() const
        {
            glm::mat4 model = glm::mat4(1.0f);

            model = glm::translate(model, position);

            model = glm::rotate(model,
                glm::radians(rotation.x), glm::vec3(1, 0, 0));
            model = glm::rotate(model,
                glm::radians(rotation.y), glm::vec3(0, 1, 0));
            model = glm::rotate(model,
                glm::radians(rotation.z), glm::vec3(0, 0, 1));

            model = glm::scale(model, scale);

            return model;
        }
    };

} // namespace MyEngine