#pragma once

#include <algorithm>
#include <filesystem>

#include "components/CharacterCustomizationComponent.h"
#include "core/AssetManager.h"
#include "rendering/Texture.h"

namespace MyEngine
{
	inline std::vector<std::string> ListCharacterMaterials(const std::string& directory)
	{
		std::vector<std::string> paths;
		std::error_code ec;
		std::filesystem::recursive_directory_iterator it(directory,
			std::filesystem::directory_options::skip_permission_denied, ec), end;
		while (!ec && it != end)
		{
			const auto& entry = *it;
			const std::string name = entry.path().filename().string();
			const std::string suffix = ".material.json";
			if (entry.is_regular_file(ec) && name.size() >= suffix.size()
				&& name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
				paths.push_back(entry.path().generic_string());
			it.increment(ec);
		}
		std::sort(paths.begin(), paths.end());
		return paths;
	}

	inline void ResolveCharacterMaterialAssets(CharacterMaterialOverride& material)
	{
		if (!material.enabled)
			return;
		material.materialAsset.Resolve(material.materialPath, [](const std::string& path)
		{
			return AssetManager::LoadMaterial(path);
		});
		material.textureAsset.Resolve(material.albedoTexturePath, [](const std::string& path)
		{
			auto texture = AssetManager::LoadTexture(path);
			return texture && texture->GetID() != 0 && texture->GetWidth() > 0 && texture->GetHeight() > 0
				? texture : nullptr;
		});
	}
}
