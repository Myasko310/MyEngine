#pragma once

#include "components/MeshComponent.h"
#include <string>
#include <vector>

namespace MyEngine {

    class ModelLoader {
    public:
        // Returns a list of meshes loaded from a file
        static std::vector<MeshComponent> Load(const std::string& path);

    private:
        static void ProcessNode(
            const void* nodePtr,
            const void* scenePtr,
            std::vector<MeshComponent>& meshes
        );

        static MeshComponent ProcessMesh(
            const void* meshPtr,
            const void* scenePtr
        );
    };

} // namespace MyEngine