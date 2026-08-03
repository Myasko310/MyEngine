#include "rendering/Mesh.h"

namespace MyEngine
{
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
        m_IndexCount = static_cast<unsigned int>(indices.size());
        SetupMesh(vertices, indices);
    }

    Mesh::~Mesh()
    {
        Release();
    }

    Mesh::Mesh(Mesh&& other) noexcept
    {
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_IndexCount = other.m_IndexCount;

        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_IndexCount = 0;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            Release();

            m_VAO = other.m_VAO;
            m_VBO = other.m_VBO;
            m_EBO = other.m_EBO;
            m_IndexCount = other.m_IndexCount;

            other.m_VAO = 0;
            other.m_VBO = 0;
            other.m_EBO = 0;
            other.m_IndexCount = 0;
        }

        return *this;
    }

    void Mesh::SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);

        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW
        );

        // layout(location = 0) in vec3 a_Position;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Position))
        );

        // layout(location = 1) in vec3 a_Color;
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Color))
        );

        // layout(location = 2) in vec3 a_Normal;
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Normal))
        );

        glBindVertexArray(0);
    }

    void Mesh::Draw() const
    {
        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void Mesh::Release()
    {
        if (m_EBO != 0)
        {
            glDeleteBuffers(1, &m_EBO);
            m_EBO = 0;
        }

        if (m_VBO != 0)
        {
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }

        if (m_VAO != 0)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }

        m_IndexCount = 0;
    }
}