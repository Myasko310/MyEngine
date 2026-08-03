#include "rendering/Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

namespace MyEngine
{
	bool Model::LoadFromFile(const std::string& path)
	{
		Assimp::Importer importer;

		const aiScene* scene = importer.ReadFile(
			path,
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_JoinIdenticalVertices
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cerr << "[Model] Failed to load: " << path << " - " << importer.GetErrorString() << std::endl;
			return false;
		}

		m_Meshes.clear();

		// Process meshes
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			aiMesh* aMesh = scene->mMeshes[i];

			std::vector<Vertex> vertices;
			std::vector<unsigned int> indices;

			vertices.reserve(aMesh->mNumVertices);

			for (unsigned int v = 0; v < aMesh->mNumVertices; ++v)
			{
				Vertex vert;
				if (aMesh->HasPositions())
				{
					vert.Position = glm::vec3(
						aMesh->mVertices[v].x,
						aMesh->mVertices[v].y,
						aMesh->mVertices[v].z
					);
				}

				if (aMesh->HasNormals())
				{
					vert.Normal = glm::vec3(
						aMesh->mNormals[v].x,
						aMesh->mNormals[v].y,
						aMesh->mNormals[v].z
					);
				}
				else
				{
					vert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
				}

				if (aMesh->HasVertexColors(0))
				{
					vert.Color = glm::vec3(
						aMesh->mColors[0][v].r,
						aMesh->mColors[0][v].g,
						aMesh->mColors[0][v].b
					);
				}
				else
				{
					vert.Color = glm::vec3(1.0f);
				}

				vertices.push_back(vert);
			}

			for (unsigned int f = 0; f < aMesh->mNumFaces; ++f)
			{
				aiFace& face = aMesh->mFaces[f];
				for (unsigned int idx = 0; idx < face.mNumIndices; ++idx)
				{
					indices.push_back(face.mIndices[idx]);
				}
			}

			auto mesh = std::make_shared<Mesh>(vertices, indices);
			m_Meshes.push_back(mesh);
		}

		return true;
	}
}
