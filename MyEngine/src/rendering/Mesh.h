#pragma once

#include <vector>
#include <cstddef>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace MyEngine
{
    struct Vertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Color{ 1.0f };
        glm::vec3 Normal{ 0.0f };
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        void Draw() const;

        unsigned int GetVAO() const { return m_VAO; }
        unsigned int GetIndexCount() const { return m_IndexCount; }

    private:
        unsigned int m_VAO = 0;
        unsigned int m_VBO = 0;
        unsigned int m_EBO = 0;
        unsigned int m_IndexCount = 0;

    private:
        void SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        void Release();
    };
}