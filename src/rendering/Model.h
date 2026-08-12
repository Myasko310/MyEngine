#pragma once

#include <string>
#include <vector>
#include <memory>

#include "rendering/Mesh.h"
#include "rendering/Skeleton.h"
#include "rendering/AnimationClip.h"

namespace MyEngine
{
	class Model
	{
	public:
		Model() = default;

		bool LoadFromFile(const std::string& path);

		const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }

		// Present only if the source file contained bone weights (aiMesh::HasBones()).
		bool HasSkeleton() const { return m_Skeleton && m_Skeleton->GetBoneCount() > 0; }
		std::shared_ptr<Skeleton> GetSkeleton() const { return m_Skeleton; }

		const std::vector<AnimationClip>& GetAnimationClips() const { return m_AnimationClips; }

	private:
		std::vector<std::shared_ptr<Mesh>> m_Meshes;
		std::shared_ptr<Skeleton> m_Skeleton;
		std::vector<AnimationClip> m_AnimationClips;
	};
}
