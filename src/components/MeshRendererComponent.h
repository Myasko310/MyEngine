#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "rendering/Shader.h"
#include "rendering/Texture.h"

struct MeshRendererComponent
{
    std::shared_ptr<MyEngine::Shader> shader = nullptr;
    bool visible = true;
    glm::vec3 albedo = glm::vec3(1.0f);
    float shininess = 32.0f;
    std::shared_ptr<MyEngine::Texture> texture = nullptr;
    bool useTexture = false;
};