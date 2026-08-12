#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace MyEngine
{
	// A single bone in a skeleton hierarchy. `parentIndex` mirrors the
	// assimp node tree that produced it (-1 for the root bone). `offsetMatrix`
	// transforms a vertex from mesh (bind pose) space into this bone's local
	// space, as extracted from aiBone::mOffsetMatrix.
	struct Bone
	{
		std::string name;
		int parentIndex = -1;
		glm::mat4 offsetMatrix{ 1.0f };

		// Local bind-pose transform relative to the parent bone, used as a
		// fallback for any animation track that doesn't touch this bone.
		glm::mat4 localBindTransform{ 1.0f };
	};

	// Holds the bone hierarchy for a skinned model. Built once at load time
	// from assimp's node/bone data and shared (via SkeletonComponent) by all
	// entities that use the same skinned model.
	class Skeleton
	{
	public:
		int AddBone(const Bone& bone)
		{
			int index = static_cast<int>(m_Bones.size());
			m_Bones.push_back(bone);
			m_NameToIndex[bone.name] = index;
			return index;
		}

		int GetBoneIndex(const std::string& name) const
		{
			auto it = m_NameToIndex.find(name);
			return it != m_NameToIndex.end() ? it->second : -1;
		}

		const std::vector<Bone>& GetBones() const { return m_Bones; }
		std::vector<Bone>& GetBones() { return m_Bones; }

		size_t GetBoneCount() const { return m_Bones.size(); }

	private:
		std::vector<Bone> m_Bones;
		std::unordered_map<std::string, int> m_NameToIndex;
	};
}
