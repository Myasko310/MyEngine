#pragma once

#include "ecs/Component.h"
#include "rendering/Shader.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace MyEngine {

    struct MeshRendererComponent : public Component {
        std::shared_ptr<Shader> shader;

        glm::vec3 color = glm::vec3(1.0f);
        bool      wireframe = false;
        bool      visible = true;

        std::string modelPath = ""; // for model loading
    };

} // namespace MyEngine