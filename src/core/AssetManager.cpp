#include "core/AssetManager.h"

#include "rendering/Model.h"
#include "rendering/Texture.h"
#include "ecs/Entity.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/BoundingSphereComponent.h"
#include "rendering/Shader.h"

namespace MyEngine
{
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> AssetManager::s_ModelCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> AssetManager::s_TextureCache;

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

	std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& path, bool generateMipmaps)
	{
		auto it = s_TextureCache.find(path);
		if (it != s_TextureCache.end())
			return it->second;

		auto texture = std::make_shared<Texture>(path, generateMipmaps);
		s_TextureCache[path] = texture;
		return texture;
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
}
