#include "rendering/Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <unordered_map>

namespace MyEngine
{
	static glm::mat4 ToGlmMat4(const aiMatrix4x4& m)
	{
		// Assimp matrices are row-major; glm is column-major, so this is
		// constructed transposed (glm::mat4 constructor takes columns).
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	}

	static glm::vec3 ToGlmVec3(const aiVector3D& v)
	{
		return glm::vec3(v.x, v.y, v.z);
	}

	static glm::quat ToGlmQuat(const aiQuaternion& q)
	{
		return glm::quat(q.w, q.x, q.y, q.z);
	}

	// Adds a weight/index influence to a vertex's least-significant free
	// bone slot, or replaces the weakest existing influence if all 4 slots
	// are already used (rare, but keeps the strongest influences).
	static void AddBoneInfluence(Vertex& vertex, int boneIndex, float weight)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
		{
			if (vertex.BoneIDs[i] < 0)
			{
				vertex.BoneIDs[i] = boneIndex;
				vertex.BoneWeights[i] = weight;
				return;
			}
		}

		// All slots full: replace the smallest weight if this one is larger.
		int weakestIndex = 0;
		for (int i = 1; i < MAX_BONE_INFLUENCE; ++i)
		{
			if (vertex.BoneWeights[i] < vertex.BoneWeights[weakestIndex])
				weakestIndex = i;
		}
		if (weight > vertex.BoneWeights[weakestIndex])
		{
			vertex.BoneIDs[weakestIndex] = boneIndex;
			vertex.BoneWeights[weakestIndex] = weight;
		}
	}

	// Recursively registers a bone hierarchy in the skeleton, mirroring the
	// assimp node tree. Only nodes that are actually referenced as bones by
	// some mesh (present in `boneOffsets`) are added; other nodes (e.g. mesh
	// or light nodes) are skipped, but the tree is still walked through them
	// so bones deeper in the hierarchy are still found and reparented onto
	// the nearest ancestor bone.
	static void CollectBones(
		const aiNode* node,
		int parentBoneIndex,
		const std::unordered_map<std::string, glm::mat4>& boneOffsets,
		Skeleton& skeleton)
	{
		if (!node)
			return;

		int thisBoneIndex = parentBoneIndex;

		std::string nodeName = node->mName.C_Str();
		auto offsetIt = boneOffsets.find(nodeName);
		if (offsetIt != boneOffsets.end())
		{
			Bone bone;
			bone.name = nodeName;
			bone.parentIndex = parentBoneIndex;
			bone.offsetMatrix = offsetIt->second;
			bone.localBindTransform = ToGlmMat4(node->mTransformation);
			thisBoneIndex = skeleton.AddBone(bone);
		}

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			CollectBones(node->mChildren[i], thisBoneIndex, boneOffsets, skeleton);
		}
	}

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
		m_Skeleton.reset();
		m_AnimationClips.clear();

		// Collect bone offset matrices across all meshes first, so the node
		// tree walk below (CollectBones) knows which nodes are bones.
		std::unordered_map<std::string, glm::mat4> boneOffsets;
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			aiMesh* aMesh = scene->mMeshes[i];
			for (unsigned int b = 0; b < aMesh->mNumBones; ++b)
			{
				aiBone* aBone = aMesh->mBones[b];
				boneOffsets[aBone->mName.C_Str()] = ToGlmMat4(aBone->mOffsetMatrix);
			}
		}

		if (!boneOffsets.empty())
		{
			m_Skeleton = std::make_shared<Skeleton>();
			CollectBones(scene->mRootNode, -1, boneOffsets, *m_Skeleton);
		}

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

				// Texture coordinates (first UV channel only). Needed for
				// both classic texturing and PBR material maps.
				if (aMesh->HasTextureCoords(0))
				{
					vert.TexCoords = glm::vec2(
						aMesh->mTextureCoords[0][v].x,
						aMesh->mTextureCoords[0][v].y
					);
				}

				// Tangent (world/model-space) for tangent-space normal
				// mapping in shaders/pbr.frag. aiProcess_CalcTangentSpace
				// computes this for us, but only when UVs are present.
				if (aMesh->HasTangentsAndBitangents())
				{
					vert.Tangent = glm::vec3(
						aMesh->mTangents[v].x,
						aMesh->mTangents[v].y,
						aMesh->mTangents[v].z
					);
				}

				vertices.push_back(vert);
			}

			// Bone weights: for each bone influencing this mesh, look up its
			// skeleton index and stamp the weight onto every vertex it affects.
			if (m_Skeleton)
			{
				for (unsigned int b = 0; b < aMesh->mNumBones; ++b)
				{
					aiBone* aBone = aMesh->mBones[b];
					int boneIndex = m_Skeleton->GetBoneIndex(aBone->mName.C_Str());
					if (boneIndex < 0)
						continue;

					for (unsigned int w = 0; w < aBone->mNumWeights; ++w)
					{
						unsigned int vertexId = aBone->mWeights[w].mVertexId;
						float weight = aBone->mWeights[w].mWeight;
						if (vertexId < vertices.size())
							AddBoneInfluence(vertices[vertexId], boneIndex, weight);
					}
				}
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

		// Process animations: one AnimationClip per aiAnimation, one
		// BoneAnimationTrack per aiNodeAnim (channel).
		for (unsigned int a = 0; a < scene->mNumAnimations; ++a)
		{
			aiAnimation* aAnim = scene->mAnimations[a];

			AnimationClip clip;
			clip.name = aAnim->mName.length > 0 ? aAnim->mName.C_Str() : ("Animation" + std::to_string(a));
			clip.durationTicks = static_cast<float>(aAnim->mDuration);
			clip.ticksPerSecond = aAnim->mTicksPerSecond > 0.0001 ? static_cast<float>(aAnim->mTicksPerSecond) : 25.0f;

			for (unsigned int c = 0; c < aAnim->mNumChannels; ++c)
			{
				aiNodeAnim* channel = aAnim->mChannels[c];

				BoneAnimationTrack track;
				track.boneName = channel->mNodeName.C_Str();

				track.positionKeys.reserve(channel->mNumPositionKeys);
				for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
				{
					PositionKey key;
					key.time = static_cast<float>(channel->mPositionKeys[k].mTime);
					key.value = ToGlmVec3(channel->mPositionKeys[k].mValue);
					track.positionKeys.push_back(key);
				}

				track.rotationKeys.reserve(channel->mNumRotationKeys);
				for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
				{
					RotationKey key;
					key.time = static_cast<float>(channel->mRotationKeys[k].mTime);
					key.value = ToGlmQuat(channel->mRotationKeys[k].mValue);
					track.rotationKeys.push_back(key);
				}

				track.scaleKeys.reserve(channel->mNumScalingKeys);
				for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
				{
					ScaleKey key;
					key.time = static_cast<float>(channel->mScalingKeys[k].mTime);
					key.value = ToGlmVec3(channel->mScalingKeys[k].mValue);
					track.scaleKeys.push_back(key);
				}

				clip.tracks.push_back(std::move(track));
			}

			m_AnimationClips.push_back(std::move(clip));
		}

		return true;
	}
}
