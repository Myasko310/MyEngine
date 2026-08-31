#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "systems/ScriptSystem.h"
#include "serialization/SceneSerializer.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/LightComponent.h"
#include "components/ScriptComponent.h"
#include "components/SkeletonComponent.h"
#include "components/CollisionEventsComponent.h"
#include "core/InputActions.h"

#include <GLFW/glfw3.h>

#include <algorithm>
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
		child->AddComponent<SkeletonComponent>();
		child->AddComponent<CollisionEventsComponent>();

		auto lightEntity = scene.CreateEntity("SerializerLight");
		auto& lightTransform = lightEntity->AddComponent<TransformComponent>();
		lightTransform.position = { -2.0f, 1.0f, 0.5f };
		auto& light = lightEntity->AddComponent<MyEngine::LightComponent>();
		light.type = MyEngine::LightComponent::Type::Point;
		light.castShadows = true;
		light.pointShadowSizeOverride = 768;
		light.pointShadowPCFSamplesOverride = 11;
		light.pointShadowPCFRadiusOverride = 0.031f;

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

		if (loaded.GetEntities().size() != 3)
		{
			std::cerr << "SerializerSmokeTest: expected loaded scene to replace old entities" << std::endl;
			return false;
		}

		Entity* loadedParent = nullptr;
		Entity* loadedChild = nullptr;
		Entity* loadedLight = nullptr;
		for (const auto& e : loaded.GetEntities())
		{
			if (!e)
				continue;

			if (e->GetName() == "SerializerParent")
				loadedParent = e.get();
			else if (e->GetName() == "SerializerChild")
				loadedChild = e.get();
			else if (e->GetName() == "SerializerLight")
				loadedLight = e.get();
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

		if (!loadedChild->HasComponent<SkeletonComponent>())
		{
			std::cerr << "SerializerSmokeTest: SkeletonComponent did not survive roundtrip" << std::endl;
			return false;
		}

		if (!loadedChild->HasComponent<CollisionEventsComponent>())
		{
			std::cerr << "SerializerSmokeTest: CollisionEventsComponent did not survive roundtrip" << std::endl;
			return false;
		}

		if (!loadedLight || !loadedLight->HasComponent<MyEngine::LightComponent>())
		{
			std::cerr << "SerializerSmokeTest: SerializerLight missing after roundtrip" << std::endl;
			return false;
		}

		auto& loadedLightComp = loadedLight->GetComponent<MyEngine::LightComponent>();
		if (loadedLightComp.pointShadowSizeOverride != 768 ||
			loadedLightComp.pointShadowPCFSamplesOverride != 11 ||
			!NearlyEqual(loadedLightComp.pointShadowPCFRadiusOverride, 0.031f))
		{
			std::cerr << "SerializerSmokeTest: light shadow override fields were not preserved" << std::endl;
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

	bool InputProfilesSmokeTest()
	{
		MyEngine::InputActions::RegisterDefaults();
		if (!MyEngine::InputActions::CreateProfile("Alt", true))
		{
			std::cerr << "InputProfilesSmokeTest: failed to create Alt profile" << std::endl;
			return false;
		}

		if (!MyEngine::InputActions::SetActiveProfile("Alt"))
		{
			std::cerr << "InputProfilesSmokeTest: failed to activate Alt profile" << std::endl;
			return false;
		}

		MyEngine::InputActions::ActionBinding altJump;
		altJump.keys = { GLFW_KEY_E };
		altJump.gamepadAxes = { GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER };
		altJump.axisThreshold = 0.65f;
		altJump.invertAxis = true;
		MyEngine::InputActions::BindAction("Jump", altJump);

		MyEngine::InputActions::AxisBinding altMove;
		altMove.keyPairs = { { GLFW_KEY_D, GLFW_KEY_A } };
		altMove.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_X };
		altMove.deadzone = 0.23f;
		altMove.sensitivity = 1.8f;
		MyEngine::InputActions::BindAxis("MoveRight", altMove);

		if (!MyEngine::InputActions::SetActiveProfile("Default"))
		{
			std::cerr << "InputProfilesSmokeTest: failed to switch back to Default" << std::endl;
			return false;
		}

		MyEngine::InputActions::ActionBinding defaultJump;
		if (!MyEngine::InputActions::TryGetActionBinding("Jump", defaultJump) || defaultJump.keys.empty() || defaultJump.keys[0] == GLFW_KEY_E)
		{
			std::cerr << "InputProfilesSmokeTest: profile switch did not isolate bindings" << std::endl;
			return false;
		}

		if (!MyEngine::InputActions::SetActiveProfile("Alt"))
		{
			std::cerr << "InputProfilesSmokeTest: failed to re-activate Alt" << std::endl;
			return false;
		}

		MyEngine::InputActions::ActionBinding loadedAltJump;
		if (!MyEngine::InputActions::TryGetActionBinding("Jump", loadedAltJump) || loadedAltJump.keys.empty() || loadedAltJump.keys[0] != GLFW_KEY_E)
		{
			std::cerr << "InputProfilesSmokeTest: Alt Jump binding mismatch" << std::endl;
			return false;
		}
		if (loadedAltJump.gamepadAxes.empty() || loadedAltJump.gamepadAxes[0] != GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER || !NearlyEqual(loadedAltJump.axisThreshold, 0.65f) || !loadedAltJump.invertAxis)
		{
			std::cerr << "InputProfilesSmokeTest: action metadata mismatch" << std::endl;
			return false;
		}

		MyEngine::InputActions::AxisBinding loadedAltMove;
		if (!MyEngine::InputActions::TryGetAxisBinding("MoveRight", loadedAltMove) || !NearlyEqual(loadedAltMove.deadzone, 0.23f) || !NearlyEqual(loadedAltMove.sensitivity, 1.8f))
		{
			std::cerr << "InputProfilesSmokeTest: axis metadata mismatch" << std::endl;
			return false;
		}

		const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "myengine_input_bindings_smoke.json";
		if (!MyEngine::InputActions::SaveBindings(tempPath.string()))
		{
			std::cerr << "InputProfilesSmokeTest: SaveBindings failed" << std::endl;
			return false;
		}

		MyEngine::InputActions::RegisterDefaults();
		if (!MyEngine::InputActions::LoadBindings(tempPath.string()))
		{
			std::cerr << "InputProfilesSmokeTest: LoadBindings failed" << std::endl;
			return false;
		}

		auto profiles = MyEngine::InputActions::GetProfileNames();
		if (std::find(profiles.begin(), profiles.end(), "Alt") == profiles.end())
		{
			std::cerr << "InputProfilesSmokeTest: Alt profile missing after load" << std::endl;
			return false;
		}

		if (MyEngine::InputActions::GetActiveProfileName() != "Alt")
		{
			std::cerr << "InputProfilesSmokeTest: active profile not restored" << std::endl;
			return false;
		}

		std::error_code ec;
		std::filesystem::remove(tempPath, ec);
		return true;
	}

	bool InputConflictResolutionSmokeTest()
	{
		MyEngine::InputActions::RegisterDefaults();

		MyEngine::InputActions::ActionBinding a0;
		a0.keys = { GLFW_KEY_Q };
		MyEngine::InputActions::BindAction("AAction", a0);
		MyEngine::InputActions::ActionBinding a1;
		a1.keys = { GLFW_KEY_Q };
		MyEngine::InputActions::BindAction("BAction", a1);

		MyEngine::InputActions::AxisBinding ax0;
		ax0.keyPairs = { { GLFW_KEY_W, GLFW_KEY_S } };
		ax0.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_X };
		MyEngine::InputActions::BindAxis("AAxis", ax0);
		MyEngine::InputActions::AxisBinding ax1;
		ax1.keyPairs = { { GLFW_KEY_W, GLFW_KEY_D } };
		ax1.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_X };
		MyEngine::InputActions::BindAxis("BAxis", ax1);

		MyEngine::InputActions::ResolveConflictsKeepFirst();

		MyEngine::InputActions::ActionBinding bAfterFirst;
		if (!MyEngine::InputActions::TryGetActionBinding("BAction", bAfterFirst) || !bAfterFirst.keys.empty())
		{
			std::cerr << "InputConflictResolutionSmokeTest: keep-first action conflict resolution failed" << std::endl;
			return false;
		}

		MyEngine::InputActions::AxisBinding bAxisAfterFirst;
		if (!MyEngine::InputActions::TryGetAxisBinding("BAxis", bAxisAfterFirst) || !bAxisAfterFirst.gamepadAxes.empty())
		{
			std::cerr << "InputConflictResolutionSmokeTest: keep-first axis conflict resolution failed" << std::endl;
			return false;
		}

		MyEngine::InputActions::BindAction("AAction", a0);
		MyEngine::InputActions::BindAction("BAction", a1);
		MyEngine::InputActions::BindAxis("AAxis", ax0);
		MyEngine::InputActions::BindAxis("BAxis", ax1);

		MyEngine::InputActions::ResolveConflictsKeepLast();

		MyEngine::InputActions::ActionBinding aAfterLast;
		if (!MyEngine::InputActions::TryGetActionBinding("AAction", aAfterLast) || !aAfterLast.keys.empty())
		{
			std::cerr << "InputConflictResolutionSmokeTest: keep-last action conflict resolution failed" << std::endl;
			return false;
		}

		MyEngine::InputActions::AxisBinding aAxisAfterLast;
		if (!MyEngine::InputActions::TryGetAxisBinding("AAxis", aAxisAfterLast) || !aAxisAfterLast.gamepadAxes.empty())
		{
			std::cerr << "InputConflictResolutionSmokeTest: keep-last axis conflict resolution failed" << std::endl;
			return false;
		}

		return true;
	}
}

int main()
{
	const bool serializerOk = SerializerSmokeTest();
	const bool luaApiOk = LuaApiSmokeTest();
	const bool inputProfilesOk = InputProfilesSmokeTest();
	const bool inputConflictsOk = InputConflictResolutionSmokeTest();

	if (!serializerOk || !luaApiOk || !inputProfilesOk || !inputConflictsOk)
		return 1;

	std::cout << "Smoke tests passed" << std::endl;
	return 0;
}
