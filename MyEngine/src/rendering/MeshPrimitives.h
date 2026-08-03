#pragma once

#include <memory>

#include "rendering/Mesh.h"

namespace MyEngine
{
    class MeshPrimitives
    {
    public:
        static std::shared_ptr<Mesh> CreateCube();
    };
}