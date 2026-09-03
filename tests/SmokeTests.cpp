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
#include "components/PrefabInstanceComponent.h"
#include "components/AnimationComponent.h"
#include "components/AudioSourceComponent.h"
#include "audio/AudioEngine.h"
#include "rendering/Material.h"
#include "core/Input.h"
#include "core/InputActions.h"
#include "core/AssetPipeline.h"
#include "core/AnimationEventBus.h"
#include "systems/AnimationSystem.h"
#include "renderer/RenderBackend.h"
#include "renderer/RenderCommandList.h"
#include "renderer/DirectX12RenderCommandExecutor.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
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
		auto& childAudio = child->AddComponent<AudioSourceComponent>();
		childAudio.clipPath = "assets/audio/test.wav";
		childAudio.busName = "SFX";
		childAudio.eventName = "Explosion";

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

		if (!loadedChild->HasComponent<AudioSourceComponent>())
		{
			std::cerr << "SerializerSmokeTest: AudioSourceComponent did not survive roundtrip" << std::endl;
			return false;
		}

		auto& loadedAudio = loadedChild->GetComponent<AudioSourceComponent>();
		if (loadedAudio.busName != "SFX" || loadedAudio.eventName != "Explosion")
		{
			std::cerr << "SerializerSmokeTest: audio bus/event fields were not preserved" << std::endl;
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

	bool LuaAudioControlsSmokeTest()
	{
		Scene scene;
		const std::filesystem::path scriptPath = std::filesystem::temp_directory_path() / "myengine_lua_audio_controls_smoke.lua";
		{
			std::ofstream script(scriptPath.string(), std::ios::binary);
			script
				<< "didRun = didRun or false\n"
				<< "function OnGlobalUpdate(dt)\n"
				<< "  if didRun then return end\n"
				<< "  engine.set_audio_bus_volume('SFX', 0.35)\n"
				<< "  local current = engine.get_audio_bus_volume('SFX')\n"
				<< "  if math.abs(current - 0.35) > 0.001 then error('Unexpected SFX bus volume') end\n"
				<< "  engine.trigger_audio_event('Explosion')\n"
				<< "  didRun = true\n"
				<< "end\n";
		}

		MyEngine::ScriptSystem scriptSystem;
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> globals;
		globals.push_back({ scriptPath.string(), true, true, false });
		scriptSystem.SetGlobalScripts(globals);
		scriptSystem.OnUpdate(scene, 0.016f);

		if (!NearlyEqual(MyEngine::AudioEngine::GetBusVolume("SFX"), 0.35f, 0.001f))
		{
			std::cerr << "LuaAudioControlsSmokeTest: SFX bus volume was not set via Lua" << std::endl;
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
		if (!MyEngine::InputActions::SetDefaultProfile("Alt"))
		{
			std::cerr << "InputProfilesSmokeTest: failed to set default profile" << std::endl;
			return false;
		}

		MyEngine::InputActions::AxisCalibration calibration;
		calibration.deadzone = 0.12f;
		calibration.exponent = 1.6f;
		calibration.saturation = 0.85f;
		calibration.invert = true;
		if (!MyEngine::InputActions::SetGamepadAxisCalibration(GLFW_GAMEPAD_AXIS_RIGHT_X, calibration))
		{
			std::cerr << "InputProfilesSmokeTest: failed to set calibration" << std::endl;
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
		if (MyEngine::InputActions::GetDefaultProfileName() != "Alt")
		{
			std::cerr << "InputProfilesSmokeTest: default profile not restored" << std::endl;
			return false;
		}

		MyEngine::InputActions::AxisCalibration loadedCalibration;
		if (!MyEngine::InputActions::TryGetGamepadAxisCalibration(GLFW_GAMEPAD_AXIS_RIGHT_X, loadedCalibration))
		{
			std::cerr << "InputProfilesSmokeTest: missing loaded calibration" << std::endl;
			return false;
		}
		if (!NearlyEqual(loadedCalibration.deadzone, 0.12f) || !NearlyEqual(loadedCalibration.exponent, 1.6f) || !NearlyEqual(loadedCalibration.saturation, 0.85f) || !loadedCalibration.invert)
		{
			std::cerr << "InputProfilesSmokeTest: calibration values mismatch" << std::endl;
			return false;
		}

		if (!MyEngine::InputActions::RenameProfile("Alt", "AltRenamed"))
		{
			std::cerr << "InputProfilesSmokeTest: rename profile failed" << std::endl;
			return false;
		}
		if (!MyEngine::InputActions::DuplicateProfile("AltRenamed", "AltCopy"))
		{
			std::cerr << "InputProfilesSmokeTest: duplicate profile failed" << std::endl;
			return false;
		}
		auto renamedProfiles = MyEngine::InputActions::GetProfileNames();
		if (std::find(renamedProfiles.begin(), renamedProfiles.end(), "AltRenamed") == renamedProfiles.end() ||
			std::find(renamedProfiles.begin(), renamedProfiles.end(), "AltCopy") == renamedProfiles.end())
		{
			std::cerr << "InputProfilesSmokeTest: rename/duplicate result missing" << std::endl;
			return false;
		}
		if (!MyEngine::InputActions::DeleteProfile("AltCopy"))
		{
			std::cerr << "InputProfilesSmokeTest: delete duplicated profile failed" << std::endl;
			return false;
		}
		if (!MyEngine::InputActions::IsBindingsDirty())
		{
			std::cerr << "InputProfilesSmokeTest: dirty-state was not set after profile edits" << std::endl;
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

	bool PrefabRoundTripSmokeTest()
	{
		Scene sourceScene;
		auto root = sourceScene.CreateEntity("PrefabRoot");
		auto& rootTransform = root->AddComponent<TransformComponent>();
		rootTransform.position = { 3.0f, 2.0f, 1.0f };

		auto child = sourceScene.CreateEntity("PrefabChild");
		auto& childTransform = child->AddComponent<TransformComponent>();
		childTransform.parentID = root->GetID();
		child->AddComponent<MyEngine::RigidbodyComponent>().mass = 7.5f;

		const std::filesystem::path prefabPath = std::filesystem::temp_directory_path() / "myengine_prefab_smoke.prefab.json";
		if (!MyEngine::Serialization::SavePrefab(sourceScene, root.get(), prefabPath.string()))
		{
			std::cerr << "PrefabRoundTripSmokeTest: SavePrefab failed" << std::endl;
			return false;
		}

		Scene destinationScene;
		Entity* spawnedRoot = MyEngine::Serialization::SpawnPrefab(destinationScene, prefabPath.string(), nullptr);
		if (!spawnedRoot)
		{
			std::cerr << "PrefabRoundTripSmokeTest: SpawnPrefab failed" << std::endl;
			return false;
		}
		if (!spawnedRoot->HasComponent<PrefabInstanceComponent>())
		{
			std::cerr << "PrefabRoundTripSmokeTest: root missing PrefabInstanceComponent" << std::endl;
			return false;
		}

		const auto& rootPrefabInfo = spawnedRoot->GetComponent<PrefabInstanceComponent>();
		if (rootPrefabInfo.sourcePrefabPath.empty() ||
			std::filesystem::path(rootPrefabInfo.sourcePrefabPath).filename().string() != prefabPath.filename().string())
		{
			std::cerr << "PrefabRoundTripSmokeTest: source prefab path mismatch" << std::endl;
			return false;
		}

		Entity* spawnedChild = nullptr;
		for (const auto& e : destinationScene.GetEntities())
		{
			if (!e || e.get() == spawnedRoot)
				continue;
			if (e->GetName() == "PrefabChild")
				spawnedChild = e.get();
		}
		if (!spawnedChild || !spawnedChild->HasComponent<TransformComponent>())
		{
			std::cerr << "PrefabRoundTripSmokeTest: child missing after spawn" << std::endl;
			return false;
		}
		if (spawnedChild->GetComponent<TransformComponent>().parentID != 0)
		{
			std::cerr << "PrefabRoundTripSmokeTest: expected prefab child to remain unparented at root level" << std::endl;
			return false;
		}

		std::error_code ec;
		std::filesystem::remove(prefabPath, ec);
		return true;
	}

	bool AssetDependencySmokeTest()
	{
		namespace fs = std::filesystem;
		const fs::path tempRoot = fs::temp_directory_path() / "myengine_assetdep_smoke";
		std::error_code ec;
		fs::create_directories(tempRoot, ec);
		if (ec)
		{
			std::cerr << "AssetDependencySmokeTest: unable to create temp directory" << std::endl;
			return false;
		}

		const fs::path texturePath = tempRoot / "smoke_texture.png";
		const fs::path materialPath = tempRoot / "smoke_material.material.json";
		const fs::path cachePath = tempRoot / "assetdeps.cache";
		{
			std::ofstream tex(texturePath.string(), std::ios::binary);
			tex << "png";
			std::ofstream mat(materialPath.string(), std::ios::binary);
			mat << "{\"albedoMap\":\"smoke_texture.png\"}";
		}

		auto& pipeline = MyEngine::AssetPipeline::Get();
		pipeline.ScanAssets(tempRoot.generic_string());
		auto dependents = pipeline.GetDependentsForAsset(texturePath.generic_string());
		if (std::find(dependents.begin(), dependents.end(), materialPath.generic_string()) == dependents.end())
		{
			std::cerr << "AssetDependencySmokeTest: expected dependent metadata was not found" << std::endl;
			fs::remove_all(tempRoot, ec);
			return false;
		}

		if (!pipeline.SaveDependencyCache(cachePath.generic_string()))
		{
			std::cerr << "AssetDependencySmokeTest: failed to save dependency cache" << std::endl;
			fs::remove_all(tempRoot, ec);
			return false;
		}
		if (!pipeline.LoadDependencyCache(cachePath.generic_string()))
		{
			std::cerr << "AssetDependencySmokeTest: failed to load dependency cache" << std::endl;
			fs::remove_all(tempRoot, ec);
			return false;
		}
		if (!pipeline.InvalidateAssetMetadata(texturePath.generic_string()))
		{
			std::cerr << "AssetDependencySmokeTest: failed to invalidate metadata" << std::endl;
			fs::remove_all(tempRoot, ec);
			return false;
		}

		int queued = pipeline.QueueReimportDependents(texturePath.generic_string());
		if (queued < 1)
		{
			std::cerr << "AssetDependencySmokeTest: expected at least one dependent reimport queued" << std::endl;
			fs::remove_all(tempRoot, ec);
			return false;
		}

		fs::remove_all(tempRoot, ec);
		return true;
	}

	bool ReplaySimulationConfigSmokeTest()
	{
		MyEngine::Input::BeginInputRecording(4242u);
		MyEngine::Input::FinalizeReplayFrame(nullptr, 0, nullptr, 0, 0.016f, 4, 123456u);
		MyEngine::Input::StopInputRecording();
		MyEngine::Input::BeginInputPlayback();
		MyEngine::Input::Update();

		float fixedTimestep = 0.0f;
		int maxSubsteps = 0;
		unsigned int particleSeed = 0u;
		if (!MyEngine::Input::TryGetPlaybackSimulationConfig(&fixedTimestep, &maxSubsteps, &particleSeed))
		{
			std::cerr << "ReplaySimulationConfigSmokeTest: playback simulation config unavailable" << std::endl;
			MyEngine::Input::StopInputPlayback();
			return false;
		}

		MyEngine::Input::StopInputPlayback();
		if (!NearlyEqual(fixedTimestep, 0.016f) || maxSubsteps != 4 || particleSeed != 123456u)
		{
			std::cerr << "ReplaySimulationConfigSmokeTest: simulation config mismatch" << std::endl;
			return false;
		}

		return true;
	}

	bool PrefabVariantMetadataSmokeTest()
	{
		Scene scene;
		auto root = scene.CreateEntity("VariantRoot");
		if (!root)
			return false;

		auto& rootPrefab = root->AddComponent<PrefabInstanceComponent>();
		rootPrefab.sourcePrefabPath = "assets/prefabs/base.prefab.json";
		rootPrefab.sourceEntityID = 42;
		rootPrefab.isVariantInstance = true;
		rootPrefab.variantBasePrefabPath = "assets/prefabs/base.prefab.json";
		rootPrefab.variantBaseEntityID = 42;
		rootPrefab.overrideName = true;
		rootPrefab.overrideTag = false;
		rootPrefab.overrideLayer = true;

		std::string json = MyEngine::Serialization::SaveSceneToString(scene, {});
		if (json.find("\"isVariantInstance\": true") == std::string::npos ||
			json.find("\"variantBasePrefabPath\"") == std::string::npos)
		{
			std::cerr << "PrefabVariantMetadataSmokeTest: missing serialized variant metadata" << std::endl;
			return false;
		}

		Scene loaded;
		if (!MyEngine::Serialization::LoadSceneFromString(loaded, json, nullptr, nullptr))
		{
			std::cerr << "PrefabVariantMetadataSmokeTest: LoadSceneFromString failed" << std::endl;
			return false;
		}

		auto loadedRoot = loaded.GetEntityByID(root->GetID());
		if (!loadedRoot || !loadedRoot->HasComponent<PrefabInstanceComponent>())
		{
			std::cerr << "PrefabVariantMetadataSmokeTest: missing prefab component after roundtrip" << std::endl;
			return false;
		}

		const auto& loadedPrefab = loadedRoot->GetComponent<PrefabInstanceComponent>();
		if (!loadedPrefab.isVariantInstance ||
			loadedPrefab.variantBasePrefabPath != "assets/prefabs/base.prefab.json" ||
			loadedPrefab.variantBaseEntityID != 42 ||
			!loadedPrefab.overrideName || loadedPrefab.overrideTag || !loadedPrefab.overrideLayer)
		{
			std::cerr << "PrefabVariantMetadataSmokeTest: variant metadata mismatch after roundtrip" << std::endl;
			return false;
		}

		return true;
	}

	bool AnimationEventSerializationSmokeTest()
	{
		Scene scene;
		auto animEntity = scene.CreateEntity("AnimEvents");
		if (!animEntity)
			return false;
		auto& anim = animEntity->AddComponent<AnimationComponent>();
		AnimationComponent::AnimationEvent ev;
		ev.timeSeconds = 1.25f;
		ev.name = "Footstep";
		ev.enabled = true;
		ev.triggerAudio = true;
		ev.audioClipPath = "assets/audio/footstep.wav";
		ev.audioVolume = 0.75f;
		ev.audioPitch = 1.1f;
		ev.triggerParticleBurst = true;
		ev.particleBurstCount = 12;
		ev.triggerScriptCallback = true;
		ev.scriptCallbackName = "OnFootstep";
		anim.events.push_back(ev);

		std::string json = MyEngine::Serialization::SaveSceneToString(scene, {});
		if (json.find("\"events\"") == std::string::npos || json.find("Footstep") == std::string::npos)
		{
			std::cerr << "AnimationEventSerializationSmokeTest: events not serialized" << std::endl;
			return false;
		}

		Scene loaded;
		if (!MyEngine::Serialization::LoadSceneFromString(loaded, json, nullptr, nullptr))
		{
			std::cerr << "AnimationEventSerializationSmokeTest: LoadSceneFromString failed" << std::endl;
			return false;
		}

		auto loadedEntity = loaded.GetEntityByID(animEntity->GetID());
		if (!loadedEntity || !loadedEntity->HasComponent<AnimationComponent>())
		{
			std::cerr << "AnimationEventSerializationSmokeTest: missing animation component after roundtrip" << std::endl;
			return false;
		}

		const auto& loadedAnim = loadedEntity->GetComponent<AnimationComponent>();
		if (loadedAnim.events.size() != 1 || loadedAnim.events[0].name != "Footstep" || !NearlyEqual(loadedAnim.events[0].timeSeconds, 1.25f))
		{
			std::cerr << "AnimationEventSerializationSmokeTest: event data mismatch after roundtrip" << std::endl;
			return false;
		}
		const auto& loadedEvent = loadedAnim.events[0];
		if (!loadedEvent.triggerAudio ||
			loadedEvent.audioClipPath != "assets/audio/footstep.wav" ||
			!NearlyEqual(loadedEvent.audioVolume, 0.75f) ||
			!NearlyEqual(loadedEvent.audioPitch, 1.1f) ||
			!loadedEvent.triggerParticleBurst ||
			loadedEvent.particleBurstCount != 12 ||
			!loadedEvent.triggerScriptCallback ||
			loadedEvent.scriptCallbackName != "OnFootstep")
		{
			std::cerr << "AnimationEventSerializationSmokeTest: action payload mismatch after roundtrip" << std::endl;
			return false;
		}

		return true;
	}

	bool AnimationEventBusDispatchSmokeTest()
	{
		Scene scene;
		auto animEntity = scene.CreateEntity("AnimBusEntity");
		if (!animEntity)
			return false;
		auto& anim = animEntity->AddComponent<AnimationComponent>();
		auto& skel = animEntity->AddComponent<SkeletonComponent>();

		auto skeleton = std::make_shared<MyEngine::Skeleton>();
		MyEngine::Bone rootBone;
		rootBone.name = "Root";
		rootBone.parentIndex = -1;
		rootBone.offsetMatrix = glm::mat4(1.0f);
		rootBone.localBindTransform = glm::mat4(1.0f);
		skeleton->AddBone(rootBone);
		skel.skeleton = skeleton;

		auto clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		MyEngine::AnimationClip clip;
		clip.name = "Idle";
		clip.durationTicks = 1.0f;
		clip.ticksPerSecond = 1.0f;
		clips->push_back(clip);
		anim.clips = clips;
		anim.playing = true;
		anim.looping = true;
		anim.time = 0.0f;
		anim.playbackSpeed = 1.0f;

		AnimationComponent::AnimationEvent ev;
		ev.timeSeconds = 0.05f;
		ev.name = "Footstep";
		ev.enabled = true;
		ev.triggerAudio = true;
		ev.audioClipPath = "assets/audio/footstep.wav";
		ev.audioVolume = 0.5f;
		ev.audioPitch = 0.9f;
		ev.triggerParticleBurst = true;
		ev.particleBurstCount = 6;
		ev.triggerScriptCallback = true;
		ev.scriptCallbackName = "OnFootstep";
		anim.events.push_back(ev);

		int received = 0;
		int token = MyEngine::AnimationEventBus::Subscribe([&](const MyEngine::AnimationEventMessage& msg)
		{
			if (msg.eventName == "Footstep" && msg.entityID == animEntity->GetID())
				++received;
		});

		MyEngine::AnimationSystem animationSystem;
		animationSystem.Update(scene, 0.1f);
		MyEngine::AnimationEventBus::Unsubscribe(token);

		if (received < 1)
		{
			std::cerr << "AnimationEventBusDispatchSmokeTest: event bus did not dispatch animation event" << std::endl;
			return false;
		}

		auto actions = MyEngine::AnimationEventBus::ConsumeQueuedActions();
		if (actions.empty())
		{
			std::cerr << "AnimationEventBusDispatchSmokeTest: action queue did not receive animation event payload" << std::endl;
			return false;
		}
		const auto& action = actions[0];
		if (action.entityID != animEntity->GetID() ||
			action.eventName != "Footstep" ||
			!action.triggerAudio ||
			action.audioClipPath != "assets/audio/footstep.wav" ||
			!NearlyEqual(action.audioVolume, 0.5f) ||
			!NearlyEqual(action.audioPitch, 0.9f) ||
			!action.triggerParticleBurst ||
			action.particleBurstCount != 6 ||
			!action.triggerScriptCallback ||
			action.scriptCallbackName != "OnFootstep")
		{
			std::cerr << "AnimationEventBusDispatchSmokeTest: action payload mismatch" << std::endl;
			return false;
		}

		return true;
	}

	bool RenderBackendSelectionSmokeTest()
	{
		bool valid = false;
		if (MyEngine::RenderBackendSelector::Parse("opengl", valid) != MyEngine::RenderBackendType::OpenGL || !valid)
		{
			std::cerr << "RenderBackendSelectionSmokeTest: failed to parse opengl backend" << std::endl;
			return false;
		}

		valid = false;
		if (MyEngine::RenderBackendSelector::Parse("dx12", valid) != MyEngine::RenderBackendType::DirectX12 || !valid)
		{
			std::cerr << "RenderBackendSelectionSmokeTest: failed to parse dx12 backend" << std::endl;
			return false;
		}

		const auto glCaps = MyEngine::RenderBackendSelector::GetCapabilities(MyEngine::RenderBackendType::OpenGL);
		if (!glCaps.supportsOpenGLPipeline || glCaps.supportsShaderModel6)
		{
			std::cerr << "RenderBackendSelectionSmokeTest: unexpected OpenGL capability flags" << std::endl;
			return false;
		}

		const auto dx12Caps = MyEngine::RenderBackendSelector::GetCapabilities(MyEngine::RenderBackendType::DirectX12);
		if (dx12Caps.supportsOpenGLPipeline || !dx12Caps.supportsNativeDebugCapture)
		{
			std::cerr << "RenderBackendSelectionSmokeTest: unexpected DirectX12 capability flags" << std::endl;
			return false;
		}

		char arg0[] = "myengine";
		char arg1[] = "--renderer=dx12";
		char* argv[] = { arg0, arg1 };
		const auto selection = MyEngine::RenderBackendSelector::Select(2, argv);
		if (selection.requested != MyEngine::RenderBackendType::DirectX12 || !selection.explicitSelection)
		{
			std::cerr << "RenderBackendSelectionSmokeTest: argv selection failed" << std::endl;
			return false;
		}

		return true;
	}

	bool RenderCommandPlumbingSmokeTest()
	{
		MyEngine::RenderCommandList commands;
		commands.BeginFrame();
		MyEngine::RenderClearCommand clear;
		clear.clearColor = true;
		clear.clearDepth = true;
		clear.clearStencil = true;
		clear.color[0] = 0.1f;
		clear.color[1] = 0.2f;
		clear.color[2] = 0.3f;
		clear.color[3] = 1.0f;
		commands.Clear(clear);
		MyEngine::RenderDrawIndexedCommand draw;
		draw.pipelineHandle = 42;
		draw.meshHandle = 77;
		draw.indexCount = 3;
		draw.startInstance = 2;
		commands.DrawIndexed(draw);

		MyEngine::RenderClearCommand invalidClear;
		invalidClear.clearColor = false;
		invalidClear.clearDepth = false;
		invalidClear.clearStencil = false;
		commands.Clear(invalidClear);

		MyEngine::RenderDrawIndexedCommand invalidDraw;
		invalidDraw.pipelineHandle = 0;
		invalidDraw.meshHandle = 0;
		invalidDraw.indexCount = 0;
		commands.DrawIndexed(invalidDraw);
		commands.EndFrame();

		const auto& cmdList = commands.GetCommands();
		if (cmdList.size() < 3)
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: expected command sequence entries" << std::endl;
			return false;
		}
		const auto* drawCmd = std::get_if<MyEngine::RenderDrawIndexedCommand>(&cmdList[2]);
		if (!drawCmd || drawCmd->startInstance != 2 || drawCmd->pipelineHandle != 42 || drawCmd->meshHandle != 77)
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: draw command payload mismatch" << std::endl;
			return false;
		}

		const auto* invalidDrawCmd = std::get_if<MyEngine::RenderDrawIndexedCommand>(&cmdList[4]);
		if (!invalidDrawCmd || invalidDrawCmd->indexCount != 0)
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: invalid draw command setup mismatch" << std::endl;
			return false;
		}

		MyEngine::DirectX12RenderCommandExecutor dx12Executor;
		dx12Executor.Execute(commands);
		if (dx12Executor.GetClearCommandCount() != 1)
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: expected 1 clear command" << std::endl;
			return false;
		}
		if (dx12Executor.GetDrawIndexedCommandCount() != 1)
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: expected 1 draw indexed command" << std::endl;
			return false;
		}

		commands.Reset();
		if (!commands.Empty())
		{
			std::cerr << "RenderCommandPlumbingSmokeTest: command list reset failed" << std::endl;
			return false;
		}

		return true;
	}

	bool MaterialInheritanceSmokeTest()
	{
		MyEngine::Material base;
		base.albedo = { 0.2f, 0.4f, 0.6f };
		base.metallic = 0.9f;

		MyEngine::Material instance;
		instance.useBaseMaterial = true;
		instance.baseMaterial = std::make_shared<MyEngine::Material>(base);
		instance.overrideSurface = false;
		instance.overridePBR = false;
		instance.overrideTextures = false;
		instance.overrideShader = false;
		instance.overrideRenderFlags = false;

		const glm::vec3 resolvedAlbedo = instance.GetResolvedAlbedo();
		if (!NearlyEqual(resolvedAlbedo.x, 0.2f) || !NearlyEqual(resolvedAlbedo.y, 0.4f) || !NearlyEqual(resolvedAlbedo.z, 0.6f))
		{
			std::cerr << "MaterialInheritanceSmokeTest: resolved albedo mismatch" << std::endl;
			return false;
		}
		if (!NearlyEqual(instance.GetResolvedMetallic(), 0.9f))
		{
			std::cerr << "MaterialInheritanceSmokeTest: resolved metallic mismatch" << std::endl;
			return false;
		}

		return true;
	}
}

int main()
{
	const bool serializerOk = SerializerSmokeTest();
	const bool luaApiOk = LuaApiSmokeTest();
	const bool luaAudioControlsOk = LuaAudioControlsSmokeTest();
	const bool inputProfilesOk = InputProfilesSmokeTest();
	const bool inputConflictsOk = InputConflictResolutionSmokeTest();
	const bool prefabOk = PrefabRoundTripSmokeTest();
	const bool assetDepsOk = AssetDependencySmokeTest();
	const bool replaySimOk = ReplaySimulationConfigSmokeTest();
	const bool prefabVariantMetaOk = PrefabVariantMetadataSmokeTest();
	const bool animationEventsOk = AnimationEventSerializationSmokeTest();
	const bool animationEventBusOk = AnimationEventBusDispatchSmokeTest();
	const bool renderBackendSelectionOk = RenderBackendSelectionSmokeTest();
	const bool renderCommandPlumbingOk = RenderCommandPlumbingSmokeTest();
	const bool materialInheritanceOk = MaterialInheritanceSmokeTest();

	if (!serializerOk || !luaApiOk || !luaAudioControlsOk || !inputProfilesOk || !inputConflictsOk || !prefabOk || !assetDepsOk || !replaySimOk || !prefabVariantMetaOk || !animationEventsOk || !animationEventBusOk || !renderBackendSelectionOk || !renderCommandPlumbingOk || !materialInheritanceOk)
		return 1;

	std::cout << "Smoke tests passed" << std::endl;
	return 0;
}
