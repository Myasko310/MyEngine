#pragma once

#include "ecs/Scene.h"
#include "systems/ScriptSystem.h"
#include <string>
#include <memory>
#include <vector>

namespace MyEngine
{
	class Shader;

	namespace Serialization
	{
		bool SaveScene(
			const ::Scene& scene,
			const std::string& path,
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& globalScripts = {}
		);

		// In-memory variants — no disk I/O (used for Play/Stop snapshots).
		std::string SaveSceneToString(
			const ::Scene& scene,
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& globalScripts = {}
		);

		bool LoadSceneFromString(
			::Scene& scene,
			const std::string& json,
			const std::shared_ptr<MyEngine::Shader>& defaultShader = nullptr,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* outGlobalScripts = nullptr
		);
		// defaultShader is assigned to each loaded entity's MeshRendererComponent
		// since the shader itself is not persisted in the scene file.
		bool LoadScene(
			::Scene& scene,
			const std::string& path,
			const std::shared_ptr<MyEngine::Shader>& defaultShader = nullptr,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* outGlobalScripts = nullptr
		);

		// Prefab helpers: serialize one entity to a .prefab.json file, then
		// instantiate a copy of it back into a scene.
		bool SavePrefab(
			::Entity* entity,
			const std::string& path
		);

		::Entity* SpawnPrefab(
			::Scene& scene,
			const std::string& path,
			const std::shared_ptr<MyEngine::Shader>& defaultShader = nullptr
		);
	}
}
