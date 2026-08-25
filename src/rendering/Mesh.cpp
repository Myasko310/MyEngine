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
        m_IndexCount     = other.m_IndexCount;
        m_BoundingCenter = other.m_BoundingCenter;
        m_BoundingRadius = other.m_BoundingRadius;
        m_Vertices       = std::move(other.m_Vertices);
        m_Indices        = std::move(other.m_Indices);

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
            m_IndexCount     = other.m_IndexCount;
            m_BoundingCenter = other.m_BoundingCenter;
            m_BoundingRadius = other.m_BoundingRadius;
            m_Vertices       = std::move(other.m_Vertices);
            m_Indices        = std::move(other.m_Indices);

            other.m_VAO = 0;
            other.m_VBO = 0;
            other.m_EBO = 0;
            other.m_IndexCount = 0;
        }

        return *this;
    }

    void Mesh::SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
        // Retain CPU copies for collision mesh building and editor tools.
        m_Vertices = vertices;
        m_Indices  = indices;

        // Compute bounding sphere from vertex positions (AABB-based)
        if (!vertices.empty())
        {
            glm::vec3 minV = vertices[0].Position;
            glm::vec3 maxV = vertices[0].Position;

            for (size_t i = 1; i < vertices.size(); ++i)
            {
                const glm::vec3& p = vertices[i].Position;
                minV = glm::min(minV, p);
                maxV = glm::max(maxV, p);
            }

            m_BoundingCenter = (minV + maxV) * 0.5f;
            m_BoundingRadius = glm::length(maxV - m_BoundingCenter);
        }
        else
        {
            m_BoundingCenter = glm::vec3(0.0f);
            m_BoundingRadius = 0.0f;
        }

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

        // layout(location = 3) in vec2 a_TexCoords;
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(
            3,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, TexCoords))
        );

        // layout(location = 6) in vec3 a_Tangent;
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(
            6,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Tangent))
        );

        // layout(location = 4) in ivec4 a_BoneIDs;
        // Bone indices must use the integer attribute pointer variant (not the
        // float one) so they are read as ints in the shader rather than being
        // implicitly converted, which would corrupt indices above 24 bits.
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(
            4,
            MAX_BONE_INFLUENCE,
            GL_INT,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, BoneIDs))
        );

        // layout(location = 5) in vec4 a_BoneWeights;
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(
            5,
            MAX_BONE_INFLUENCE,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, BoneWeights))
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
        m_BoundingCenter = glm::vec3(0.0f);
        m_BoundingRadius = 0.0f;
    }
}