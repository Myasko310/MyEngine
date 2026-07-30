#pragma once

#include "components/MeshComponent.h"

namespace MyEngine {
    namespace MeshPrimitives {

        // ── Triangle ─────────────────────────────────────────────
        inline MeshComponent CreateTriangle()
        {
            MeshComponent mesh;

            mesh.vertices = {
                { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(0,0), glm::vec3(1,0,0) },
                { glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(1,0), glm::vec3(0,1,0) },
                { glm::vec3(0.0f,  0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(0.5f,1), glm::vec3(0,0,1) }
            };

            mesh.indices = { 0, 1, 2 };

            return mesh;
        }

        // ── Quad ─────────────────────────────────────────────────
        inline MeshComponent CreateQuad()
        {
            MeshComponent mesh;

            mesh.vertices = {
                { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(0,0), glm::vec3(1,1,1) },
                { glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(1,0), glm::vec3(1,1,1) },
                { glm::vec3(0.5f,  0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(1,1), glm::vec3(1,1,1) },
                { glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(0,0,1), glm::vec2(0,1), glm::vec3(1,1,1) }
            };

            mesh.indices = { 0, 1, 2,  2, 3, 0 };

            return mesh;
        }

        // ── Cube ─────────────────────────────────────────────────
        inline MeshComponent CreateCube()
        {
            MeshComponent mesh;

            mesh.vertices = {
                // Front face
                { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(0,0), glm::vec3(1,0,0) },
                { glm::vec3(0.5f,-0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(1,0), glm::vec3(0,1,0) },
                { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(1,1), glm::vec3(0,0,1) },
                { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(0,1), glm::vec3(1,1,0) },

                // Back face
                { glm::vec3(0.5f,-0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(0,0), glm::vec3(1,0,1) },
                { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(1,0), glm::vec3(0,1,1) },
                { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(1,1), glm::vec3(1,1,1) },
                { glm::vec3(0.5f, 0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(0,1), glm::vec3(0,0,0) },

                // Left face
                { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(-1,0,0), glm::vec2(0,0), glm::vec3(1,0,0) },
                { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(-1,0,0), glm::vec2(1,0), glm::vec3(0,1,0) },
                { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(-1,0,0), glm::vec2(1,1), glm::vec3(0,0,1) },
                { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(-1,0,0), glm::vec2(0,1), glm::vec3(1,1,0) },

                // Right face
                { glm::vec3(0.5f,-0.5f, 0.5f), glm::vec3(1,0,0), glm::vec2(0,0), glm::vec3(1,0,1) },
                { glm::vec3(0.5f,-0.5f,-0.5f), glm::vec3(1,0,0), glm::vec2(1,0), glm::vec3(0,1,1) },
                { glm::vec3(0.5f, 0.5f,-0.5f), glm::vec3(1,0,0), glm::vec2(1,1), glm::vec3(1,1,1) },
                { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1,0,0), glm::vec2(0,1), glm::vec3(0,0,0) },

                // Top face
                { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0,1,0), glm::vec2(0,0), glm::vec3(1,0,0) },
                { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0,1,0), glm::vec2(1,0), glm::vec3(0,1,0) },
                { glm::vec3(0.5f, 0.5f,-0.5f), glm::vec3(0,1,0), glm::vec2(1,1), glm::vec3(0,0,1) },
                { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(0,1,0), glm::vec2(0,1), glm::vec3(1,1,0) },

                // Bottom face
                { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(0,-1,0), glm::vec2(0,0), glm::vec3(1,0,1) },
                { glm::vec3(0.5f,-0.5f,-0.5f), glm::vec3(0,-1,0), glm::vec2(1,0), glm::vec3(0,1,1) },
                { glm::vec3(0.5f,-0.5f, 0.5f), glm::vec3(0,-1,0), glm::vec2(1,1), glm::vec3(1,1,1) },
                { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(0,-1,0), glm::vec2(0,1), glm::vec3(0,0,0) },
            };

            mesh.indices = {
                 0, 1, 2,  2, 3, 0,   // Front
                 4, 5, 6,  6, 7, 4,   // Back
                 8, 9,10,  10,11, 8,  // Left
                12,13,14,  14,15,12,  // Right
                16,17,18,  18,19,16,  // Top
                20,21,22,  22,23,20   // Bottom
            };

            return mesh;
        }

    } // MeshPrimitives
} // MyEngine