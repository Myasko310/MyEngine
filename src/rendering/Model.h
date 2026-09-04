#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>

#include "rendering/Mesh.h"
#include "rendering/Skeleton.h"
#include "rendering/AnimationClip.h"

namespace MyEngine
{
	// Per-mesh material data extracted from an Assimp scene.
	// Only the properties needed to auto-generate a Material asset are stored.
	struct MeshMaterialData
	{
		std::string name;              // aiMaterial name (may be empty)
		glm::vec3   albedo  = glm::vec3(1.0f);
		float       shininess = 32.0f;
		std::string diffuseTexturePath;  // first aiTextureType_DIFFUSE path, relative to model dir
		std::string normalTexturePath;   // first aiTextureType_NORMALS / HEIGHT path
	};

	class Model
	{
	public:
		Model() = default;

		bool LoadFromFile(const std::string& path);

		const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }

		// Per-mesh material data extracted from the source file.
		// Ordered to match GetMeshes() — element i belongs to mesh i.
		const std::vector<MeshMaterialData>& GetMeshMaterialData() const { return m_MeshMaterials; }

		// Present only if the source file contained bone weights (aiMesh::HasBones()).
		bool HasSkeleton() const { return m_Skeleton && m_Skeleton->GetBoneCount() > 0; }
		std::shared_ptr<Skeleton> GetSkeleton() const { return m_Skeleton; }

		const std::vector<AnimationClip>& GetAnimationClips() const { return m_AnimationClips; }

	private:
		std::vector<std::shared_ptr<Mesh>> m_Meshes;
		std::vector<MeshMaterialData>      m_MeshMaterials;
		std::shared_ptr<Skeleton>          m_Skeleton;
		std::vector<AnimationClip>         m_AnimationClips;
	};
}
