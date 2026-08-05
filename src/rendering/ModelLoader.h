#pragma once

#include <memory>
#include <string>

#include "rendering/Mesh.h"

namespace MyEngine
{
    class ModelLoader
    {
    public:
        static std::shared_ptr<Mesh> LoadOBJ(const std::string& filepath);
    };
}