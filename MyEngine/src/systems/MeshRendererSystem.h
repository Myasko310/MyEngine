#pragma once

#include <glm/glm.hpp>

#include "ecs/Scene.h"

class MeshRendererSystem
{
public:
    void Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection);
};