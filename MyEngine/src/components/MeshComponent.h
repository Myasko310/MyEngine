#pragma once

#include <memory>

#include "rendering/Mesh.h"

struct MeshComponent
{
    std::shared_ptr<MyEngine::Mesh> mesh = nullptr;
};