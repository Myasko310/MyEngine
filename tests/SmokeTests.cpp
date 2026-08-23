#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "systems/ScriptSystem.h"
#include "serialization/SceneSerializer.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/LightComponent.h"
#include "components/ScriptComponent.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	bool NearlyEqual(float a, float b, float eps = 0.0001f)
	{
		return std::fabs(a - b) <= eps;
	}

	bool SerializerSmokeTest()
	{
		Scene scene;
		auto parent = scene.CreateEntity("SerializerParent");
		auto& parentTransform = parent->AddComponent<TransformComponent>();
		parentTransform.position = { 1.0f, 2.0f, 3.0f };

		auto child = scene.CreateEntity("SerializerChild");
		auto& childTransform = child->AddComponent<TransformComponent>();
		childTransform.parentID = parent->GetID();

		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> globalScripts;
		globalScripts.push_back({"assets/scripts/a.lua", true, true, false});
		globalScripts.push_back({"assets/scripts/b.lua", false, false, false});

		const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "myengine_serializer_smoke.scene";
		if (!MyEngine::Serialization::SaveScene(scene, tempPath.string(), globalScripts))
		{
			std::cerr << "SerializerSmokeTest: SaveScene failed" << std::endl;
			return false;
		}

		std::ifstream ifs(tempPath.string());
		std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		if (text.find("\"sceneVersion\"") == std::string::npos ||
			text.find("\"globalScripts\"") == std::string::npos ||
			text.find("\"order\"") == std::string::npos)
		{
			std::cerr << "SerializerSmokeTest: expected metadata fields missing" << std::endl;
			return false;
		}

		Scene loaded;
		loaded.CreateEntity("ShouldBeReplaced");

		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> loadedGlobals;
		if (!MyEngine::Serialization::LoadScene(loaded, tempPath.string(), nullptr, &loadedGlobals))
		{
			std::cerr << "SerializerSmokeTest: LoadScene failed" << std::endl;
			return false;
		}

		if (loadedGlobals.size() != 2)
		{
			std::cerr << "SerializerSmokeTest: expected 2 global scripts" << std::endl;
			return false;
		}

		if (loadedGlobals[0].scriptPath != "assets/scripts/a.lua" ||
			loadedGlobals[1].scriptPath != "assets/scripts/b.lua")
		{
			std::cerr << "SerializerSmokeTest: global script order/path mismatch" << std::endl;
			return false;
		}

		if (loaded.GetEntities().size() != 2)
		{
			std::cerr << "SerializerSmokeTest: expected loaded scene to replace old entities" << std::endl;
			return false;
		}

		Entity* loadedParent = nullptr;
		Entity* loadedChild = nullptr;
		for (const auto& e : loaded.GetEntities())
		{
			if (!e)
				continue;

			if (e->GetName() == "SerializerParent")
				loadedParent = e.get();
			else if (e->GetName() == "SerializerChild")
				loadedChild = e.get();
		}

		if (!loadedParent || !loadedChild || !loadedChild->HasComponent<TransformComponent>())
		{
			std::cerr << "SerializerSmokeTest: loaded entities missing" << std::endl;
			return false;
		}

		if (loadedChild->GetComponent<TransformComponent>().parentID != loadedParent->GetID())
		{
			std::cerr << "SerializerSmokeTest: parent relationship was not preserved" << std::endl;
			return false;
		}

		std::error_code ec;
		std::filesystem::remove(tempPath, ec);
		return true;
	}

	bool LuaApiSmokeTest()
	{
		Scene scene;
		auto player = scene.CreateEntity("Player");
		player->AddComponent<TransformComponent>();
		auto& rb = player->AddComponent<MyEngine::RigidbodyComponent>();

		const std::filesystem::path scriptPath = std::filesystem::temp_directory_path() / "myengine_lua_api_smoke.lua";
		{
			std::ofstream script(scriptPath.string(), std::ios::binary);
			script
				<< "didRun = didRun or false\n"
				<< "function OnGlobalUpdate(dt)\n"
				<< "  if didRun then return end\n"
				<< "  local playerId = engine.find_entity_by_name('Player')\n"
				<< "  engine.set_velocity(playerId, 4.0, 5.0, 6.0)\n"
				<< "  local spawned = engine.create_entity('LuaSpawned')\n"
				<< "  engine.add_component(spawned, 'light')\n"
				<< "  engine.remove_component(spawned, 'light')\n"
				<< "  didRun = true\n"
				<< "end\n";
		}

		MyEngine::ScriptSystem scriptSystem;
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> globals;
		globals.push_back({ scriptPath.string(), true, true, false });
		scriptSystem.SetGlobalScripts(globals);
		scriptSystem.OnUpdate(scene, 0.016f);

		if (!NearlyEqual(rb.velocity.x, 4.0f) || !NearlyEqual(rb.velocity.y, 5.0f) || !NearlyEqual(rb.velocity.z, 6.0f))
		{
			std::cerr << "LuaApiSmokeTest: velocity was not set by global script" << std::endl;
			return false;
		}

		Entity* spawnedEntity = nullptr;
		for (const auto& e : scene.GetEntities())
		{
			if (e && e->GetName() == "LuaSpawned")
			{
				spawnedEntity = e.get();
				break;
			}
		}

		if (!spawnedEntity)
		{
			std::cerr << "LuaApiSmokeTest: spawned entity not found" << std::endl;
			return false;
		}

		if (spawnedEntity->HasComponent<MyEngine::LightComponent>())
		{
			std::cerr << "LuaApiSmokeTest: light component should have been removed" << std::endl;
			return false;
		}

		std::error_code ec;
		std::filesystem::remove(scriptPath, ec);
		return true;
	}
}

int main()
{
	const bool serializerOk = SerializerSmokeTest();
	const bool luaApiOk = LuaApiSmokeTest();

	if (!serializerOk || !luaApiOk)
		return 1;

	std::cout << "Smoke tests passed" << std::endl;
	return 0;
}
