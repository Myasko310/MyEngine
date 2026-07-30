#include "rendering/ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

namespace MyEngine {

    std::vector<MeshComponent> ModelLoader::Load(const std::string& path)
    {
        std::vector<MeshComponent> meshes;

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenNormals
        );

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            std::cerr << "[ModelLoader] Assimp error: "
                << importer.GetErrorString() << "\n";
            return meshes;
        }

        ProcessNode(scene->mRootNode, scene, meshes);

        return meshes;
    }

    // ─────────────────────────────────────────────────────────────
    void ModelLoader::ProcessNode(
        const void* nodePtr,
        const void* scenePtr,
        std::vector<MeshComponent>& meshes
    )
    {
        const aiNode* node = static_cast<const aiNode*>(nodePtr);
        const aiScene* scene = static_cast<const aiScene*>(scenePtr);

        if (!node || !scene)
            return;

        // Process all meshes in this node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(ProcessMesh(mesh, scene));
        }

        // Process children recursively
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, meshes);
        }
    }

    // ─────────────────────────────────────────────────────────────
    MeshComponent ModelLoader::ProcessMesh(
        const void* meshPtr,
        const void* /*scenePtr*/
    )
    {
        const aiMesh* m = static_cast<const aiMesh*>(meshPtr);

        MeshComponent mesh;

        if (!m)
            return mesh;

        // ── Vertices ─────────────────────────────────────────────
        for (unsigned int i = 0; i < m->mNumVertices; i++)
        {
            Vertex v;

            v.position = glm::vec3(
                m->mVertices[i].x,
                m->mVertices[i].y,
                m->mVertices[i].z
            );

            if (m->HasNormals())
            {
                v.normal = glm::vec3(
                    m->mNormals[i].x,
                    m->mNormals[i].y,
                    m->mNormals[i].z
                );
            }
            else
            {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (m->mTextureCoords[0])
            {
                v.texCoords = glm::vec2(
                    m->mTextureCoords[0][i].x,
                    m->mTextureCoords[0][i].y
                );
            }
            else
            {
                v.texCoords = glm::vec2(0.0f, 0.0f);
            }

            // Default white vertex color
            v.color = glm::vec3(1.0f, 1.0f, 1.0f);

            mesh.vertices.push_back(v);
        }

        // ── Indices ──────────────────────────────────────────────
        for (unsigned int i = 0; i < m->mNumFaces; i++)
        {
            const aiFace& face = m->mFaces[i];

            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                mesh.indices.push_back(face.mIndices[j]);
            }
        }

        return mesh;
    }

} // namespace MyEngine