#include "MeshPrimitives.h"

namespace MyEngine {
    namespace MeshPrimitives {

        MeshComponent CreateCube(glm::vec3 color) {
            MeshComponent mesh;
            mesh.name = "Cube";

            mesh.vertices = {
                // Front face
                {{ -0.5f, -0.5f,  0.5f }, { 0,0,1 }, { 0,0 }, color },
                {{  0.5f, -0.5f,  0.5f }, { 0,0,1 }, { 1,0 }, color },
                {{  0.5f,  0.5f,  0.5f }, { 0,0,1 }, { 1,1 }, color },
                {{ -0.5f,  0.5f,  0.5f }, { 0,0,1 }, { 0,1 }, color },

                // Back face
                {{  0.5f, -0.5f, -0.5f }, { 0,0,-1 }, { 0,0 }, color },
                {{ -0.5f, -0.5f, -0.5f }, { 0,0,-1 }, { 1,0 }, color },
                {{ -0.5f,  0.5f, -0.5f }, { 0,0,-1 }, { 1,1 }, color },
                {{  0.5f,  0.5f, -0.5f }, { 0,0,-1 }, { 0,1 }, color },

                // Left face
                {{ -0.5f, -0.5f, -0.5f }, { -1,0,0 }, { 0,0 }, color },
                {{ -0.5f, -0.5f,  0.5f }, { -1,0,0 }, { 1,0 }, color },
                {{ -0.5f,  0.5f,  0.5f }, { -1,0,0 }, { 1,1 }, color },
                {{ -0.5f,  0.5f, -0.5f }, { -1,0,0 }, { 0,1 }, color },

                // Right face
                {{  0.5f, -0.5f,  0.5f }, { 1,0,0 }, { 0,0 }, color },
                {{  0.5f, -0.5f, -0.5f }, { 1,0,0 }, { 1,0 }, color },
                {{  0.5f,  0.5f, -0.5f }, { 1,0,0 }, { 1,1 }, color },
                {{  0.5f,  0.5f,  0.5f }, { 1,0,0 }, { 0,1 }, color },

                // Top face
                {{ -0.5f,  0.5f,  0.5f }, { 0,1,0 }, { 0,0 }, color },
                {{  0.5f,  0.5f,  0.5f }, { 0,1,0 }, { 1,0 }, color },
                {{  0.5f,  0.5f, -0.5f }, { 0,1,0 }, { 1,1 }, color },
                {{ -0.5f,  0.5f, -0.5f }, { 0,1,0 }, { 0,1 }, color },

                // Bottom face
                {{ -0.5f, -0.5f, -0.5f }, { 0,-1,0 }, { 0,0 }, color },
                {{  0.5f, -0.5f, -0.5f }, { 0,-1,0 }, { 1,0 }, color },
                {{  0.5f, -0.5f,  0.5f }, { 0,-1,0 }, { 1,1 }, color },
                {{ -0.5f, -0.5f,  0.5f }, { 0,-1,0 }, { 0,1 }, color },
            };

            mesh.indices = {
                 0,  1,  2,   2,  3,  0,   // Front
                 4,  5,  6,   6,  7,  4,   // Back
                 8,  9, 10,  10, 11,  8,   // Left
                12, 13, 14,  14, 15, 12,   // Right
                16, 17, 18,  18, 19, 16,   // Top
                20, 21, 22,  22, 23, 20,   // Bottom
            };

            return mesh;
        }

        MeshComponent CreatePlane(glm::vec3 color) {
            MeshComponent mesh;
            mesh.name = "Plane";

            mesh.vertices = {
                {{ -5.0f, 0.0f,  5.0f }, { 0,1,0 }, { 0,0 }, color },
                {{  5.0f, 0.0f,  5.0f }, { 0,1,0 }, { 1,0 }, color },
                {{  5.0f, 0.0f, -5.0f }, { 0,1,0 }, { 1,1 }, color },
                {{ -5.0f, 0.0f, -5.0f }, { 0,1,0 }, { 0,1 }, color },
            };

            mesh.indices = { 0, 1, 2,  2, 3, 0 };

            return mesh;
        }

    }
} // namespace MyEngine::MeshPrimitives