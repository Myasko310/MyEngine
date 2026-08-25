#include "core/AssetManager.h"

#include "rendering/Model.h"
#include "rendering/Texture.h"
#include "rendering/Material.h"
#include "rendering/Shader.h"
#include "audio/AudioClip.h"
#include "ecs/Entity.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/SkeletonComponent.h"
#include "components/AnimationComponent.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

namespace MyEngine
{
	static bool ComputeSkeletonBindBounds(
		const std::shared_ptr<Skeleton>& skeleton,
		glm::vec3& outCenter,
		float& outRadius,
		glm::vec3* outMin = nullptr,
		glm::vec3* outMax = nullptr)
	{
		if (!skeleton || skeleton->GetBoneCount() == 0)
			return false;

		const auto& bones = skeleton->GetBones();
		std::vector<glm::mat4> globalBind(bones.size(), glm::mat4(1.0f));

		glm::vec3 minV(std::numeric_limits<float>::max());
		glm::vec3 maxV(std::numeric_limits<float>::lowest());
		bool hasPoint = false;

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const auto& bone = bones[i];
			glm::mat4 parent = (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(globalBind.size()))
				? globalBind[bone.parentIndex]
				: glm::mat4(1.0f);
			globalBind[i] = parent * bone.localBindTransform;

			glm::vec3 p = glm::vec3(globalBind[i][3]);
			minV = glm::min(minV, p);
			maxV = glm::max(maxV, p);
			hasPoint = true;
		}

		if (!hasPoint)
			return false;

		outCenter = (minV + maxV) * 0.5f;
		outRadius = 0.0f;
		for (size_t i = 0; i < bones.size(); ++i)
		{
			glm::vec3 p = glm::vec3(globalBind[i][3]);
			outRadius = std::max(outRadius, glm::length(p - outCenter));
		}

		if (outMin) *outMin = minV;
		if (outMax) *outMax = maxV;

		// Small padding so hands/feet don't clip outside the bounds.
		outRadius = std::max(outRadius * 1.15f, 0.05f);
		return true;
	}

	static bool ComputeSkeletonCharacterCapsule(
		const std::shared_ptr<Skeleton>& skeleton,
		glm::vec3& outPointA,
		glm::vec3& outPointB,
		float& outRadius)
	{
		if (!skeleton || skeleton->GetBoneCount() == 0)
			return false;

		const auto& bones = skeleton->GetBones();
		std::vector<glm::mat4> globalBind(bones.size(), glm::mat4(1.0f));
		std::vector<glm::vec3> positions;
		positions.reserve(bones.size());

		auto quantile = [](std::vector<float>& values, float t) -> float
		{
			if (values.empty())
				return 0.0f;
			t = std::clamp(t, 0.0f, 1.0f);
			std::sort(values.begin(), values.end());
			float idx = t * static_cast<float>(values.size() - 1);
			size_t lo = static_cast<size_t>(idx);
			size_t hi = std::min(lo + 1, values.size() - 1);
			float f = idx - static_cast<float>(lo);
			return values[lo] * (1.0f - f) + values[hi] * f;
		};

		auto toLower = [](std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		};

		auto nameContainsAny = [](const std::string& name, const std::initializer_list<const char*> patterns)
		{
			for (const char* pattern : patterns)
			{
				if (name.find(pattern) != std::string::npos)
					return true;
			}
			return false;
		};

		std::vector<float> xs, ys, zs;
		std::vector<glm::vec3> torsoPoints;
		std::vector<glm::vec3> lowerBodyPoints;
		std::vector<float> namedFootHeights;
		std::vector<float> namedHeadHeights;
		xs.reserve(bones.size());
		ys.reserve(bones.size());
		zs.reserve(bones.size());

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const auto& bone = bones[i];
			glm::mat4 parent = (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(globalBind.size()))
				? globalBind[bone.parentIndex]
				: glm::mat4(1.0f);
			globalBind[i] = parent * bone.localBindTransform;
			glm::vec3 p = glm::vec3(globalBind[i][3]);
			positions.push_back(p);
			xs.push_back(p.x);
			ys.push_back(p.y);
			zs.push_back(p.z);

			std::string boneName = toLower(bone.name);
			if (nameContainsAny(boneName, { "hip", "pelvis", "spine", "chest", "torso", "neck", "thigh", "leg", "calf", "shin" }) &&
				!nameContainsAny(boneName, { "arm", "hand", "finger", "thumb", "shoulder", "toe" }))
			{
				torsoPoints.push_back(p);
			}
			if (nameContainsAny(boneName, { "hip", "pelvis", "thigh", "leg", "calf", "shin", "knee", "ankle", "foot" }) &&
				!nameContainsAny(boneName, { "arm", "hand", "finger", "thumb" }))
			{
				lowerBodyPoints.push_back(p);
			}
			if (nameContainsAny(boneName, { "foot", "ankle", "toe" }))
				namedFootHeights.push_back(p.y);
			if (nameContainsAny(boneName, { "head", "neck" }))
				namedHeadHeights.push_back(p.y);
		}

		if (positions.size() < 3)
			return false;

		float yMin = !namedFootHeights.empty() ? quantile(namedFootHeights, 0.10f) : quantile(ys, 0.03f);
		float yMax = !namedHeadHeights.empty() ? quantile(namedHeadHeights, 0.90f) : quantile(ys, 0.88f);
		if (yMax - yMin < 0.1f)
		{
			yMin = quantile(ys, 0.0f);
			yMax = quantile(ys, 1.0f);
		}

		const std::vector<glm::vec3>& centerSamples = !lowerBodyPoints.empty() ? lowerBodyPoints : (!torsoPoints.empty() ? torsoPoints : positions);
		std::vector<float> centerXs;
		std::vector<float> centerZs;
		centerXs.reserve(centerSamples.size());
		centerZs.reserve(centerSamples.size());
		for (const auto& p : centerSamples)
		{
			centerXs.push_back(p.x);
			centerZs.push_back(p.z);
		}
		float centerX = quantile(centerXs, 0.5f);
		float centerZ = quantile(centerZs, 0.5f);

		float height = std::max(yMax - yMin, 0.1f);
		float bodyBandMin = yMin + height * 0.12f;
		float bodyBandMax = yMin + height * 0.72f;
		std::vector<float> radial;
		radial.reserve(positions.size());
		for (const auto& p : positions)
		{
			if (p.y < bodyBandMin || p.y > bodyBandMax)
				continue;
			glm::vec2 d(p.x - centerX, p.z - centerZ);
			radial.push_back(glm::length(d));
		}
		if (radial.size() < 3)
		{
			for (const auto& p : (!torsoPoints.empty() ? torsoPoints : positions))
			{
				glm::vec2 d(p.x - centerX, p.z - centerZ);
				radial.push_back(glm::length(d));
			}
		}

		float rCore = quantile(radial, radial.size() >= 4 ? 0.45f : 0.55f);
		float maxBodyRadius = height * 0.22f;
		outRadius = std::clamp(rCore * 1.08f, 0.04f, maxBodyRadius);

		float lowerInset = std::min(outRadius * 0.35f, height * 0.08f);
		float upperInset = std::min(outRadius * 0.55f, height * 0.10f);
		float segmentMinY = yMin + lowerInset;
		float segmentMaxY = yMax - upperInset;
		if (segmentMaxY - segmentMinY < outRadius * 0.5f)
		{
			float centerY = (yMin + yMax) * 0.5f;
			float halfSegment = std::max(0.0f, 0.5f * height - outRadius);
			outPointA = glm::vec3(centerX, centerY - halfSegment, centerZ);
			outPointB = glm::vec3(centerX, centerY + halfSegment, centerZ);
		}
		else
		{
			outPointA = glm::vec3(centerX, segmentMinY, centerZ);
			outPointB = glm::vec3(centerX, segmentMaxY, centerZ);
		}
		return true;
	}

	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> AssetManager::s_ModelCache;
	std::unordered_map<std::string, SkinnedModelData> AssetManager::s_SkinnedModelCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> AssetManager::s_TextureCache;
	std::unordered_map<std::string, std::shared_ptr<Shader>> AssetManager::s_ShaderCache;
	std::unordered_map<std::string, std::shared_ptr<Material>> AssetManager::s_MaterialCache;
	std::unordered_map<std::string, std::shared_ptr<AudioClip>> AssetManager::s_AudioClipCache;

	std::vector<std::shared_ptr<Mesh>> AssetManager::LoadModel(const std::string& path)
	{
		auto it = s_ModelCache.find(path);
		if (it != s_ModelCache.end())
			return it->second;

		Model model;
		std::vector<std::shared_ptr<Mesh>> meshes;

		if (model.LoadFromFile(path))
		{
			meshes = model.GetMeshes();
			s_ModelCache[path] = meshes;
		}

		return meshes;
	}

	SkinnedModelData AssetManager::LoadSkinnedModel(const std::string& path)
	{
		auto it = s_SkinnedModelCache.find(path);
		if (it != s_SkinnedModelCache.end())
			return it->second;

		Model model;
		SkinnedModelData data;

		if (model.LoadFromFile(path))
		{
			data.meshes = model.GetMeshes();
			data.skeleton = model.GetSkeleton();
			data.clips = std::make_shared<std::vector<AnimationClip>>(model.GetAnimationClips());
			s_SkinnedModelCache[path] = data;
		}

		return data;
	}

	std::shared_ptr<std::vector<AnimationClip>> AssetManager::LoadAnimationClips(const std::string& path)
	{
		SkinnedModelData data = LoadSkinnedModel(path);
		if (!data.clips || data.clips->empty())
			return nullptr;
		return data.clips;
	}

	int AssetManager::CountMatchingAnimationTracks(const AnimationClip& clip, const std::shared_ptr<Skeleton>& skeleton)
	{
		if (!skeleton)
			return static_cast<int>(clip.tracks.size());

		int matches = 0;
		for (const auto& track : clip.tracks)
		{
			if (skeleton->GetBoneIndex(track.boneName) >= 0)
				++matches;
		}
		return matches;
	}

	bool AssetManager::IsAnimationClipCompatible(const AnimationClip& clip, const std::shared_ptr<Skeleton>& skeleton, float minimumTrackMatchRatio)
	{
		if (!skeleton)
			return !clip.tracks.empty();
		if (clip.tracks.empty())
			return false;

		int matchingTracks = CountMatchingAnimationTracks(clip, skeleton);
		if (matchingTracks <= 0)
			return false;

		float matchRatio = static_cast<float>(matchingTracks) / static_cast<float>(clip.tracks.size());
		return matchRatio >= std::clamp(minimumTrackMatchRatio, 0.0f, 1.0f);
	}

	std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& path, bool generateMipmaps)
	{
		if (path.empty())
			return nullptr;

		auto it = s_TextureCache.find(path);
		if (it != s_TextureCache.end())
			return it->second;

		std::error_code ec;
		if (!std::filesystem::exists(path, ec))
			return nullptr;

		auto texture = std::make_shared<Texture>(path, generateMipmaps);
		s_TextureCache[path] = texture;
		return texture;
	}

	std::shared_ptr<Shader> AssetManager::LoadShader(const std::string& vertexPath, const std::string& fragmentPath)
	{
		std::string key = vertexPath + "|" + fragmentPath;
		auto it = s_ShaderCache.find(key);
		if (it != s_ShaderCache.end())
			return it->second;

		auto shader = std::make_shared<Shader>(vertexPath, fragmentPath);
		s_ShaderCache[key] = shader;
		return shader;
	}

	std::shared_ptr<Material> AssetManager::LoadMaterial(const std::string& path)
	{
		auto it = s_MaterialCache.find(path);
		if (it != s_MaterialCache.end())
			return it->second;

		auto material = std::make_shared<Material>();
		if (!material->LoadFromFile(path))
			return nullptr;

		s_MaterialCache[path] = material;
		return material;
	}

	std::vector<std::shared_ptr<Material>> AssetManager::ImportModelMaterials(
		const std::string& modelPath,
		const std::string& outputDir)
	{
		// Load the model to read its embedded material data
		Model model;
		if (!model.LoadFromFile(modelPath))
			return {};

		const auto& meshMats  = model.GetMeshMaterialData();
		const size_t count    = meshMats.size();
		if (count == 0)
			return {};

		// Determine output directory
		std::string outDir = outputDir.empty() ? "assets/materials" : outputDir;
		{
			std::filesystem::path stem = std::filesystem::path(modelPath).stem();
			outDir += "/" + stem.string();
		}
		{
			std::error_code ec;
			std::filesystem::create_directories(outDir, ec);
		}

		std::string modelStem = std::filesystem::path(modelPath).stem().string();

		std::vector<std::shared_ptr<Material>> result;
		result.reserve(count);

		for (size_t i = 0; i < count; ++i)
		{
			const MeshMaterialData& md = meshMats[i];

			// Build a stable filename: <modelStem>_<index>_<matname>.material.json
			std::string safeName = md.name;
			for (char& c : safeName)
				if (c == '/' || c == '\\' || c == ':' || c == ' ')
					c = '_';
			if (safeName.empty())
				safeName = "mat" + std::to_string(i);

			std::string matPath = outDir + "/" + modelStem + "_" + safeName + ".material.json";

			// Re-use cached version if available (already imported before)
			auto cacheIt = s_MaterialCache.find(matPath);
			if (cacheIt != s_MaterialCache.end())
			{
				result.push_back(cacheIt->second);
				continue;
			}

			// Build material
			auto mat = std::make_shared<Material>(matPath);
			mat->shaderVertexPath   = "shaders/lit.vert";
			mat->shaderFragmentPath = "shaders/lit.frag";
			mat->shader             = LoadShader(mat->shaderVertexPath, mat->shaderFragmentPath);
			mat->albedo             = md.albedo;
			mat->shininess          = md.shininess;

			if (!md.diffuseTexturePath.empty())
			{
				try
				{
					mat->texture    = LoadTexture(md.diffuseTexturePath);
					mat->useTexture = (mat->texture != nullptr);
				}
				catch (...) {}
			}

			if (!md.normalTexturePath.empty())
			{
				try { mat->normalMap = LoadTexture(md.normalTexturePath); }
				catch (...) {}
			}

			// Persist to disk only if the file doesn't already exist
			{
				std::error_code ec;
				if (!std::filesystem::exists(matPath, ec))
					mat->SaveToFile(matPath);
			}

			s_MaterialCache[matPath] = mat;
			result.push_back(std::move(mat));
		}

		return result;
	}

	bool AssetManager::ComputeCharacterCapsuleFromSkeleton(
		const std::shared_ptr<Skeleton>& skeleton,
		glm::vec3& outPointA,
		glm::vec3& outPointB,
		float& outRadius)
	{
		return ComputeSkeletonCharacterCapsule(skeleton, outPointA, outPointB, outRadius);
	}

	std::shared_ptr<AudioClip> AssetManager::LoadAudioClip(const std::string& path)
	{
		auto it = s_AudioClipCache.find(path);
		if (it != s_AudioClipCache.end())
			return it->second;

		auto clip = std::make_shared<AudioClip>(path);
		if (!clip->IsValid())
			return nullptr;

		s_AudioClipCache[path] = clip;
		return clip;
	}

	void AssetManager::AttachMeshToEntity(const std::shared_ptr<::Entity>& entity,
								const std::shared_ptr<Mesh>& mesh,
								const std::string& assetPath,
								const std::shared_ptr<Shader>& shader)
	{
		if (!entity || !mesh)
			return;

		// Attach or update MeshComponent
		auto& mc = entity->AddComponent<MeshComponent>();
		mc.mesh = mesh;
		mc.assetPath = assetPath;

		// Attach or update MeshRendererComponent
		auto& mr = entity->AddComponent<MeshRendererComponent>();
		if (shader)
			mr.shader = shader;

		// Attach bounding sphere based on mesh bounds
		auto& bs = entity->AddComponent<BoundingSphereComponent>();
		bs.center = mesh->GetBoundingCenter();
		bs.radius = mesh->GetBoundingRadius();
	}

	void AssetManager::AttachSkinnedModelToEntity(const std::shared_ptr<::Entity>& entity,
								const SkinnedModelData& data,
								const std::shared_ptr<Shader>& shader,
								const std::string& assetPath)
	{
		if (!entity || data.meshes.empty())
			return;

		// Only the first mesh is attached: MeshComponent holds a single mesh,
		// matching the existing static-model attachment behavior. assetPath
		// is recorded (rather than left empty) so the scene serializer can
		// reload this as a skinned model instead of silently dropping it.
		AttachMeshToEntity(entity, data.meshes[0], assetPath, shader);

		if (data.skeleton && data.skeleton->GetBoneCount() > 0)
		{
			auto& sc = entity->AddComponent<SkeletonComponent>();
			sc.skeleton = data.skeleton;

			auto& ac = entity->AddComponent<AnimationComponent>();
			ac.clips = data.clips;
			ac.activeClipIndex = 0;
			ac.time = 0.0f;
			ac.playing = true;
			ac.looping = true;

			// Skinned meshes can have bind-pose vertex bounds that are much larger
			// than the visually deformed character. Prefer a skeleton-derived
			// bound for physics/culling so collider/shadow sizing better matches
			// the rendered character.
			glm::vec3 skinnedCenter(0.0f);
			float skinnedRadius = 0.0f;
			glm::vec3 skinnedMin(0.0f), skinnedMax(0.0f);
			if (ComputeSkeletonBindBounds(data.skeleton, skinnedCenter, skinnedRadius, &skinnedMin, &skinnedMax) && entity->HasComponent<BoundingSphereComponent>())
			{
				auto& bs = entity->GetComponent<BoundingSphereComponent>();

				// Skeleton-space bounds can be in a different scale/origin depending
				// on source rig conventions. Only adopt them when they're reasonably
				// close to the mesh-derived bounds; otherwise keep mesh bounds.
				const float meshRadius = bs.radius;
				const float minAllowedRadius = meshRadius * 0.15f;
				const float maxAllowedRadius = meshRadius * 2.5f;
				const bool radiusReasonable = skinnedRadius >= minAllowedRadius && skinnedRadius <= maxAllowedRadius;
				const bool centerReasonable = glm::length(skinnedCenter - bs.center) <= (meshRadius * 1.5f);

				if (radiusReasonable && centerReasonable)
				{
					bs.center = skinnedCenter;
					bs.radius = skinnedRadius;
				}

				// Physics should use a tighter body-shaped collider to avoid the
				// "invisible force field" feel around characters.
				auto& capsule = entity->AddComponent<CapsuleColliderComponent>();
				glm::vec3 fitA(0.0f), fitB(0.0f);
				float fitRadius = 0.0f;
				if (ComputeCharacterCapsuleFromSkeleton(data.skeleton, fitA, fitB, fitRadius))
				{
					capsule.pointA = fitA;
					capsule.pointB = fitB;
					capsule.radius = fitRadius;
				}
				else
				{
					glm::vec3 extents = glm::max(skinnedMax - skinnedMin, glm::vec3(0.001f));
					float height = std::max(extents.y, 0.1f);
					float radiusFromHorizontal = 0.5f * std::max(extents.x, extents.z) * 0.4f;
					float radiusFromHeight = height * 0.16f;
					capsule.radius = std::max(0.05f, std::min(radiusFromHorizontal, radiusFromHeight));

					float halfSegment = std::max(0.0f, 0.5f * height - capsule.radius);
					capsule.pointA = skinnedCenter + glm::vec3(0.0f, -halfSegment, 0.0f);
					capsule.pointB = skinnedCenter + glm::vec3(0.0f,  halfSegment, 0.0f);
				}
			}
		}
	}
}
