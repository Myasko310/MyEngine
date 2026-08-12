#pragma once

#include <vector>
#include <cstddef>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace MyEngine
{
    // Maximum number of bones that can influence a single vertex. Extra
    // influences beyond this (rare) are dropped, keeping the weights of the
    // strongest ones.
    constexpr int MAX_BONE_INFLUENCE = 4;

    struct Vertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Color{ 1.0f };
        glm::vec3 Normal{ 0.0f };
        glm::vec2 TexCoords{ 0.0f };

        // World/model-space tangent vector for tangent-space normal mapping
        // (see shaders/pbr.frag). Populated from Assimp's mTangents when
        // available (requires UVs); defaults to zero for meshes without
        // tangents, which the shader treats as "no normal map perturbation".
        glm::vec3 Tangent{ 0.0f };

        // Skinning data. Defaults to "no influence" (index -1, weight 0) so
        // static/non-skinned meshes built without setting these fields render
        // identically to before this was added.
        int BoneIDs[MAX_BONE_INFLUENCE] = { -1, -1, -1, -1 };
        float BoneWeights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        ~Mesh();

        const glm::vec3& GetBoundingCenter() const { return m_BoundingCenter; }
        float GetBoundingRadius() const { return m_BoundingRadius; }

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
        glm::vec3 m_BoundingCenter{0.0f};
        float m_BoundingRadius{0.0f};

    private:
        void SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        void Release();
    };
}