#pragma once

#include <memory>

#include "rendering/Mesh.h"

namespace MyEngine
{
    class MeshPrimitives
    {
    public:
        static std::shared_ptr<Mesh> CreateCube();
        static std::shared_ptr<Mesh> CreateSphere(unsigned int segments = 32, unsigned int rings = 16);
        static std::shared_ptr<Mesh> CreatePlane(float halfExtent = 5.0f);
    };
}