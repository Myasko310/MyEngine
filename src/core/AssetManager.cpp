#include "core/AssetManager.h"

#include "rendering/Model.h"
#include "rendering/Texture.h"
#include "ecs/Entity.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/SkeletonComponent.h"
#include "components/AnimationComponent.h"
#include "rendering/Shader.h"
#include "audio/AudioClip.h"

namespace MyEngine
{
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> AssetManager::s_ModelCache;
	std::unordered_map<std::string, SkinnedModelData> AssetManager::s_SkinnedModelCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> AssetManager::s_TextureCache;
	std::unordered_map<std::string, std::shared_ptr<Shader>> AssetManager::s_ShaderCache;
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

	std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& path, bool generateMipmaps)
	{
		auto it = s_TextureCache.find(path);
		if (it != s_TextureCache.end())
			return it->second;

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
		}
	}
}
