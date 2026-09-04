#include "systems/ScriptSystem.h"

#include "audio/AudioEngine.h"
#include "components/CollisionEventsComponent.h"
#include "components/LightComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/ScriptComponent.h"
#include "components/TransformComponent.h"
#include "components/AnimationComponent.h"
#include "core/Input.h"
#include "core/InputActions.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"
#include "systems/AudioSystem.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace MyEngine
{
	namespace
	{
		constexpr const char* kSystemRegistryKey = "MyEngine.ScriptSystem";
		constexpr const char* kEntityRegistryKey = "MyEngine.ScriptEntityID";

		Entity* FindEntityByID(Scene& scene, uint32_t entityID)
		{
			for (const auto& entity : scene.GetEntities())
			{
				if (entity && entity->GetID() == entityID)
					return entity.get();
			}

			return nullptr;
		}

		bool ResolveTargetEntity(lua_State* L, Scene& scene, uint32_t boundEntityID, int valueArgumentCount, uint32_t& targetEntityID, int& valueStartIndex)
		{
			targetEntityID = boundEntityID;
			valueStartIndex = 1;

			const int argCount = lua_gettop(L);
				if (argCount == valueArgumentCount + 1 && lua_isinteger(L, 1) && boundEntityID == 0)
				{
					lua_Integer requestedID = lua_tointeger(L, 1);
					if (requestedID > 0)
					{
						targetEntityID = static_cast<uint32_t>(requestedID);
						valueStartIndex = 2;
					}
				}

			if (argCount < valueArgumentCount)
				return false;

			if (targetEntityID == 0)
				return false;

			return FindEntityByID(scene, targetEntityID) != nullptr;
		}

		std::string NormalizeComponentName(std::string value)
		{
			for (char& c : value)
			{
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}

			return value;
		}

		bool InstallLuaHelpers(lua_State* L, bool entityContext)
		{
			const char* helperSource = entityContext ? R"LUA(
engine = engine or {}
engine.log = engine_log
engine.is_key_down = engine_is_key_down
engine.is_key_pressed = engine_is_key_pressed
engine.is_key_released = engine_is_key_released
engine.is_mouse_button_down = engine_is_mouse_button_down
engine.is_mouse_button_pressed = engine_is_mouse_button_pressed
engine.is_mouse_button_released = engine_is_mouse_button_released
engine.get_mouse_delta = engine_get_mouse_delta
engine.get_mouse_wheel = engine_get_mouse_wheel
engine.get_mouse_position = engine_get_mouse_position
engine.is_mouse_captured = engine_is_mouse_captured
engine.is_action = engine_is_action
engine.is_action_pressed = engine_is_action_pressed
engine.is_action_released = engine_is_action_released
engine.get_axis = engine_get_axis
engine.is_gamepad_connected = engine_is_gamepad_connected
engine.find_entity_by_name = engine_find_entity_by_name
engine.entity_exists = engine_entity_exists
engine.get_entity_name = engine_get_entity_name
engine.create_entity = engine_create_entity
engine.destroy_entity = engine_destroy_entity
engine.add_component = engine_add_component
engine.remove_component = engine_remove_component
engine.get_position_of = engine_get_position_of
engine.set_position_of = engine_set_position_of
engine.translate_of = engine_translate_of
engine.has_rigidbody = engine_has_rigidbody
engine.get_velocity = engine_get_velocity
engine.set_velocity = engine_set_velocity
engine.get_gravity_scale = engine_get_gravity_scale
engine.set_gravity_scale = engine_set_gravity_scale
engine.get_kinematic = engine_get_kinematic
engine.set_kinematic = engine_set_kinematic
engine.has_animation = engine_has_animation
engine.get_animation_clip_count = engine_get_animation_clip_count
engine.get_animation_clip_name = engine_get_animation_clip_name
engine.get_active_animation_clip = engine_get_active_animation_clip
engine.play_animation_clip = engine_play_animation_clip
engine.has_light = engine_has_light
engine.get_light_color = engine_get_light_color
engine.set_light_color = engine_set_light_color
engine.get_light_intensity = engine_get_light_intensity
engine.set_light_intensity = engine_set_light_intensity
engine.get_light_range = engine_get_light_range
engine.set_light_range = engine_set_light_range
engine.get_light_cast_shadows = engine_get_light_cast_shadows
engine.set_light_cast_shadows = engine_set_light_cast_shadows
engine.trigger_audio_event = engine_trigger_audio_event
engine.set_audio_bus_volume = engine_set_audio_bus_volume
engine.get_audio_bus_volume = engine_get_audio_bus_volume

self = self or {}
self.get_position = engine_get_position
self.set_position = engine_set_position
self.translate = engine_translate
self.get_rotation = engine_get_rotation
self.set_rotation = engine_set_rotation
self.get_scale = engine_get_scale
self.set_scale = engine_set_scale
self.has_rigidbody = engine_has_rigidbody
self.get_velocity = engine_get_velocity
self.set_velocity = engine_set_velocity
self.get_gravity_scale = engine_get_gravity_scale
self.set_gravity_scale = engine_set_gravity_scale
self.get_kinematic = engine_get_kinematic
self.set_kinematic = engine_set_kinematic
self.has_animation = engine_has_animation
self.get_animation_clip_count = engine_get_animation_clip_count
self.get_animation_clip_name = engine_get_animation_clip_name
self.get_active_animation_clip = engine_get_active_animation_clip
self.play_animation_clip = engine_play_animation_clip
self.has_light = engine_has_light
self.get_light_color = engine_get_light_color
self.set_light_color = engine_set_light_color
self.get_light_intensity = engine_get_light_intensity
self.set_light_intensity = engine_set_light_intensity
self.get_light_range = engine_get_light_range
self.set_light_range = engine_set_light_range
self.get_light_cast_shadows = engine_get_light_cast_shadows
self.set_light_cast_shadows = engine_set_light_cast_shadows
self.get_id = function(id) return id end
self.log = engine_log
)LUA"
			:
R"LUA(
engine = engine or {}
engine.log = engine_log
engine.is_key_down = engine_is_key_down
engine.is_key_pressed = engine_is_key_pressed
engine.is_key_released = engine_is_key_released
engine.is_mouse_button_down = engine_is_mouse_button_down
engine.is_mouse_button_pressed = engine_is_mouse_button_pressed
engine.is_mouse_button_released = engine_is_mouse_button_released
engine.get_mouse_delta = engine_get_mouse_delta
engine.get_mouse_wheel = engine_get_mouse_wheel
engine.get_mouse_position = engine_get_mouse_position
engine.is_mouse_captured = engine_is_mouse_captured
engine.is_action = engine_is_action
engine.is_action_pressed = engine_is_action_pressed
engine.is_action_released = engine_is_action_released
engine.get_axis = engine_get_axis
engine.is_gamepad_connected = engine_is_gamepad_connected
engine.find_entity_by_name = engine_find_entity_by_name
engine.entity_exists = engine_entity_exists
engine.get_entity_name = engine_get_entity_name
engine.create_entity = engine_create_entity
engine.destroy_entity = engine_destroy_entity
engine.add_component = engine_add_component
engine.remove_component = engine_remove_component
engine.get_position_of = engine_get_position_of
engine.set_position_of = engine_set_position_of
engine.translate_of = engine_translate_of
engine.has_rigidbody = engine_has_rigidbody
engine.get_velocity = engine_get_velocity
engine.set_velocity = engine_set_velocity
engine.get_gravity_scale = engine_get_gravity_scale
engine.set_gravity_scale = engine_set_gravity_scale
engine.get_kinematic = engine_get_kinematic
engine.set_kinematic = engine_set_kinematic
engine.has_animation = engine_has_animation
engine.get_animation_clip_count = engine_get_animation_clip_count
engine.get_animation_clip_name = engine_get_animation_clip_name
engine.get_active_animation_clip = engine_get_active_animation_clip
engine.play_animation_clip = engine_play_animation_clip
engine.has_light = engine_has_light
engine.get_light_color = engine_get_light_color
engine.set_light_color = engine_set_light_color
engine.get_light_intensity = engine_get_light_intensity
engine.set_light_intensity = engine_set_light_intensity
engine.get_light_range = engine_get_light_range
engine.set_light_range = engine_set_light_range
engine.get_light_cast_shadows = engine_get_light_cast_shadows
engine.set_light_cast_shadows = engine_set_light_cast_shadows
engine.trigger_audio_event = engine_trigger_audio_event
engine.set_audio_bus_volume = engine_set_audio_bus_volume
engine.get_audio_bus_volume = engine_get_audio_bus_volume

scene = scene or {}
scene.find_entity_by_name = engine_find_entity_by_name
scene.entity_exists = engine_entity_exists
scene.get_entity_name = engine_get_entity_name
scene.create_entity = engine_create_entity
scene.destroy_entity = engine_destroy_entity
scene.add_component = engine_add_component
scene.remove_component = engine_remove_component
scene.get_position = engine_get_position_of
scene.set_position = engine_set_position_of
scene.translate = engine_translate_of
scene.has_animation = engine_has_animation
scene.get_animation_clip_count = engine_get_animation_clip_count
scene.get_animation_clip_name = engine_get_animation_clip_name
scene.get_active_animation_clip = engine_get_active_animation_clip
scene.play_animation_clip = engine_play_animation_clip
scene.log = engine_log
)LUA";

			if (luaL_dostring(L, helperSource) != LUA_OK)
			{
				std::cerr << "[ScriptSystem] Failed to install Lua helpers: " << lua_tostring(L, -1) << std::endl;
				lua_pop(L, 1);
				return false;
			}

			return true;
		}
	}

	ScriptSystem::~ScriptSystem()
	{
		for (auto& [entityID, state] : m_States)
		{
			if (state.luaState)
			{
				lua_close(state.luaState);
				state.luaState = nullptr;
			}
		}

		for (auto& state : m_GlobalStates)
		{
			if (state.luaState)
			{
				lua_close(state.luaState);
				state.luaState = nullptr;
			}
		}
	}

	void ScriptSystem::OnUpdate(Scene& scene, float deltaTime)
	{
		m_CurrentScene = &scene;

		std::vector<uint32_t> liveEntities;
		liveEntities.reserve(scene.GetEntities().size());

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<ScriptComponent>())
				continue;

			liveEntities.push_back(entity->GetID());
			SyncEntity(scene, *entity, deltaTime);
		}

		for (auto it = m_States.begin(); it != m_States.end(); )
		{
			bool stillAlive = std::find(liveEntities.begin(), liveEntities.end(), it->first) != liveEntities.end();
			if (!stillAlive)
			{
				if (it->second.luaState)
					lua_close(it->second.luaState);
				it = m_States.erase(it);
				continue;
			}

			++it;
		}

		SyncGlobalScripts(scene, deltaTime);
		m_CurrentScene = nullptr;
	}

	void ScriptSystem::SetGlobalScripts(const std::vector<GlobalScriptConfig>& scripts)
	{
		if (scripts.size() < m_GlobalStates.size())
		{
			for (size_t i = scripts.size(); i < m_GlobalStates.size(); ++i)
			{
				if (m_GlobalStates[i].luaState)
					lua_close(m_GlobalStates[i].luaState);
			}
		}

		m_GlobalScripts = scripts;
		m_GlobalStates.resize(m_GlobalScripts.size());
	}

	void ScriptSystem::SyncScriptCollisionCallbacks(Entity& entity)
	{
		CollisionEventsComponent* events = nullptr;
		if (entity.HasComponent<CollisionEventsComponent>())
		{
			events = &entity.GetComponent<CollisionEventsComponent>();
		}
		else
		{
			events = &entity.AddComponent<CollisionEventsComponent>();
		}

		const uint32_t entityID = entity.GetID();
		events->onScriptCollisionEnter = [this, entityID](const std::shared_ptr<Entity>& other)
		{
			DispatchScriptCollisionEvent(entityID, other, "OnCollisionEnter");
		};
		events->onScriptCollisionExit = [this, entityID](const std::shared_ptr<Entity>& other)
		{
			DispatchScriptCollisionEvent(entityID, other, "OnCollisionExit");
		};
		events->onScriptTriggerEnter = [this, entityID](const std::shared_ptr<Entity>& other)
		{
			DispatchScriptCollisionEvent(entityID, other, "OnTriggerEnter");
		};
		events->onScriptTriggerExit = [this, entityID](const std::shared_ptr<Entity>& other)
		{
			DispatchScriptCollisionEvent(entityID, other, "OnTriggerExit");
		};
	}

	void ScriptSystem::ClearScriptCollisionCallbacks(Entity& entity)
	{
		if (!entity.HasComponent<CollisionEventsComponent>())
			return;

		auto& events = entity.GetComponent<CollisionEventsComponent>();
		events.onScriptCollisionEnter = nullptr;
		events.onScriptCollisionExit = nullptr;
		events.onScriptTriggerEnter = nullptr;
		events.onScriptTriggerExit = nullptr;
	}

	void ScriptSystem::DispatchScriptCollisionEvent(uint32_t entityID, const std::shared_ptr<Entity>& otherEntity, const char* functionName)
	{
		auto it = m_States.find(entityID);
		if (it == m_States.end() || !it->second.luaState)
			return;

		lua_State* L = it->second.luaState;
		lua_getglobal(L, functionName);
		if (lua_isfunction(L, -1))
		{
			lua_pushinteger(L, static_cast<lua_Integer>(entityID));
			lua_pushinteger(L, static_cast<lua_Integer>(otherEntity ? otherEntity->GetID() : 0));
			CallScriptFunction(L, functionName, 2, 0);
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	void ScriptSystem::DispatchGlobalEvent(const char* functionName, uint32_t entityID)
	{
		for (size_t i = 0; i < m_GlobalStates.size(); ++i)
		{
			lua_State* L = m_GlobalStates[i].luaState;
			if (!L || !m_GlobalStates[i].started)
				continue;

			lua_getglobal(L, functionName);
			if (lua_isfunction(L, -1))
			{
				lua_pushinteger(L, static_cast<lua_Integer>(entityID));
				CallScriptFunction(L, functionName, 1, 0);
			}
			else
			{
				lua_pop(L, 1);
			}
		}
	}

	void ScriptSystem::DispatchGlobalEventWithString(const char* functionName, uint32_t entityID, const char* strArg)
	{
		for (size_t i = 0; i < m_GlobalStates.size(); ++i)
		{
			lua_State* L = m_GlobalStates[i].luaState;
			if (!L || !m_GlobalStates[i].started)
				continue;

			lua_getglobal(L, functionName);
			if (lua_isfunction(L, -1))
			{
				lua_pushinteger(L, static_cast<lua_Integer>(entityID));
				lua_pushstring(L, strArg ? strArg : "");
				CallScriptFunction(L, functionName, 2, 0);
			}
			else
			{
				lua_pop(L, 1);
			}
		}
	}

	void ScriptSystem::DispatchAnimationEvent(uint32_t entityID, const char* eventName, const char* callbackName)
	{
		auto it = m_States.find(entityID);
		if (it != m_States.end() && it->second.luaState)
		{
			lua_State* L = it->second.luaState;
			lua_getglobal(L, callbackName && callbackName[0] != '\0' ? callbackName : "OnAnimationEvent");
			if (lua_isfunction(L, -1))
			{
				lua_pushinteger(L, static_cast<lua_Integer>(entityID));
				lua_pushstring(L, eventName ? eventName : "");
				CallScriptFunction(L, callbackName && callbackName[0] != '\0' ? callbackName : "OnAnimationEvent", 2, 0);
			}
			else
			{
				lua_pop(L, 1);
			}
		}

		DispatchGlobalEventWithString("OnAnimationEvent", entityID, eventName ? eventName : "");
	}

	void ScriptSystem::SyncEntity(Scene& scene, Entity& entity, float deltaTime)
	{
		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		uint32_t entityID = entity.GetID();

		auto existing = m_States.find(entityID);
		if (scriptComponent.scriptPath.empty())
		{
			ClearScriptCollisionCallbacks(entity);
			if (existing != m_States.end() && existing->second.luaState && existing->second.wasEnabled)
			{
				CallEntityLifecycle(existing->second.luaState, "OnDisable", entityID);
			}
			UnloadScript(entityID);
			return;
		}

		auto& state = m_States[entityID];
		if (state.scriptPath != scriptComponent.scriptPath || scriptComponent.requestReload)
		{
			if (state.luaState)
				lua_close(state.luaState);
			state = ScriptState{};
			state.scriptPath = scriptComponent.scriptPath;
			scriptComponent.requestReload = false;
		}

		if (!scriptComponent.enabled)
		{
			ClearScriptCollisionCallbacks(entity);
			if (state.luaState && state.wasEnabled && !state.disablePending)
			{
				CallEntityLifecycle(state.luaState, "OnDisable", entityID);
				state.disablePending = true;
			}
			state.wasEnabled = false;
			return;
		}

		if (!state.luaState)
		{
			if (!LoadScript(scene, entity, state, scriptComponent))
			{
				UnloadScript(entityID);
				return;
			}
		}

		SyncScriptCollisionCallbacks(entity);
		lua_State* L = state.luaState;

		if (!state.started && scriptComponent.autoStart)
		{
			lua_getglobal(L, "OnStart");
			if (lua_isfunction(L, -1))
			{
				lua_pushinteger(L, static_cast<lua_Integer>(entityID));
				CallScriptFunction(L, "OnStart", 1, 0);
			}
			else
			{
				lua_pop(L, 1);
			}

			state.started = true;
		}

		if (!state.wasEnabled)
		{
			CallEntityLifecycle(L, "OnEnable", entityID);
			state.wasEnabled = true;
			state.disablePending = false;
		}

		lua_getglobal(L, "OnUpdate");
		if (lua_isfunction(L, -1))
		{
			lua_pushinteger(L, static_cast<lua_Integer>(entityID));
			lua_pushnumber(L, static_cast<lua_Number>(deltaTime));
			CallScriptFunction(L, "OnUpdate", 2, 0);
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	void ScriptSystem::SyncGlobalScripts(Scene& scene, float deltaTime)
	{
		for (size_t i = 0; i < m_GlobalScripts.size(); ++i)
		{
			auto& config = m_GlobalScripts[i];
			auto& state = m_GlobalStates[i];

			if (config.scriptPath.empty())
			{
				if (state.luaState && state.wasEnabled)
					CallGlobalLifecycle(state.luaState, "OnGlobalDisable");
				if (state.luaState)
					lua_close(state.luaState);
				state = ScriptState{};
				continue;
			}

			if (state.scriptPath != config.scriptPath || config.requestReload)
			{
				if (state.luaState && state.wasEnabled)
					CallGlobalLifecycle(state.luaState, "OnGlobalDisable");
				if (state.luaState)
					lua_close(state.luaState);
				state = ScriptState{};
				state.scriptPath = config.scriptPath;
				config.requestReload = false;
			}

			if (!state.luaState)
			{
				if (!LoadGlobalScript(scene, state, config))
				{
					state = ScriptState{};
					continue;
				}
			}

			if (!config.enabled)
			{
				if (state.luaState && state.wasEnabled && !state.disablePending)
				{
					CallGlobalLifecycle(state.luaState, "OnGlobalDisable");
					state.disablePending = true;
				}
				state.wasEnabled = false;
				continue;
			}

			lua_State* L = state.luaState;
			if (!state.started && config.autoStart)
			{
				lua_getglobal(L, "OnGlobalStart");
				if (lua_isfunction(L, -1))
				{
					CallScriptFunction(L, "OnGlobalStart", 0, 0);
				}
				else
				{
					lua_pop(L, 1);
				}

				state.started = true;
			}

			if (!state.wasEnabled)
			{
				CallGlobalLifecycle(L, "OnGlobalEnable");
				state.wasEnabled = true;
				state.disablePending = false;
			}

			lua_getglobal(L, "OnGlobalUpdate");
			if (lua_isfunction(L, -1))
			{
				lua_pushnumber(L, static_cast<lua_Number>(deltaTime));
				CallScriptFunction(L, "OnGlobalUpdate", 1, 0);
			}
			else
			{
				lua_pop(L, 1);
			}
		}
	}

	void ScriptSystem::UnloadScript(uint32_t entityID)
	{
		auto it = m_States.find(entityID);
		if (it == m_States.end())
			return;

		if (it->second.luaState)
			lua_close(it->second.luaState);
		m_States.erase(it);
	}

	bool ScriptSystem::LoadScript(Scene& scene, Entity& entity, ScriptState& state, const ::ScriptComponent& scriptComponent)
	{
		if (!m_CurrentScene)
			m_CurrentScene = &scene;

		state.luaState = luaL_newstate();
		if (!state.luaState)
		{
			std::cerr << "[ScriptSystem] Failed to create Lua state for entity " << entity.GetID() << std::endl;
			return false;
		}

		luaL_openlibs(state.luaState);
		RegisterBindings(state.luaState, entity.GetID());

		if (luaL_dofile(state.luaState, scriptComponent.scriptPath.c_str()) != LUA_OK)
		{
			std::cerr << "[ScriptSystem] Failed to load script '" << scriptComponent.scriptPath
					  << "' for entity " << entity.GetID() << ": " << lua_tostring(state.luaState, -1) << std::endl;
			lua_pop(state.luaState, 1);
			lua_close(state.luaState);
			state.luaState = nullptr;
			return false;
		}

		state.scriptPath = scriptComponent.scriptPath;
		state.started = false;
		state.wasEnabled = false;
		state.disablePending = false;
		return true;
	}

	bool ScriptSystem::LoadGlobalScript(Scene& scene, ScriptState& state, const GlobalScriptConfig& config)
	{
		if (!m_CurrentScene)
			m_CurrentScene = &scene;

		state.luaState = luaL_newstate();
		if (!state.luaState)
		{
			std::cerr << "[ScriptSystem] Failed to create Lua state for global script '" << config.scriptPath << "'" << std::endl;
			return false;
		}

		luaL_openlibs(state.luaState);
		RegisterGlobalBindings(state.luaState);

		if (luaL_dofile(state.luaState, config.scriptPath.c_str()) != LUA_OK)
		{
			std::cerr << "[ScriptSystem] Failed to load global script '" << config.scriptPath
				<< "': " << lua_tostring(state.luaState, -1) << std::endl;
			lua_pop(state.luaState, 1);
			lua_close(state.luaState);
			state.luaState = nullptr;
			return false;
		}

		state.scriptPath = config.scriptPath;
		state.started = false;
		state.wasEnabled = false;
		state.disablePending = false;
		return true;
	}

	void ScriptSystem::RegisterBindings(lua_State* L, uint32_t entityID)
	{
		lua_pushlightuserdata(L, this);
		lua_setfield(L, LUA_REGISTRYINDEX, kSystemRegistryKey);

		lua_pushinteger(L, static_cast<lua_Integer>(entityID));
		lua_setfield(L, LUA_REGISTRYINDEX, kEntityRegistryKey);

		lua_pushcfunction(L, &ScriptSystem::LuaLog);
		lua_setglobal(L, "engine_log");

		lua_pushcfunction(L, &ScriptSystem::LuaGetPosition);
		lua_setglobal(L, "engine_get_position");

		lua_pushcfunction(L, &ScriptSystem::LuaSetPosition);
		lua_setglobal(L, "engine_set_position");

		lua_pushcfunction(L, &ScriptSystem::LuaTranslate);
		lua_setglobal(L, "engine_translate");

		lua_pushcfunction(L, &ScriptSystem::LuaGetRotation);
		lua_setglobal(L, "engine_get_rotation");

		lua_pushcfunction(L, &ScriptSystem::LuaSetRotation);
		lua_setglobal(L, "engine_set_rotation");

		lua_pushcfunction(L, &ScriptSystem::LuaGetScale);
		lua_setglobal(L, "engine_get_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaSetScale);
		lua_setglobal(L, "engine_set_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaFindEntityByName);
		lua_setglobal(L, "engine_find_entity_by_name");

		lua_pushcfunction(L, &ScriptSystem::LuaEntityExists);
		lua_setglobal(L, "engine_entity_exists");

		lua_pushcfunction(L, &ScriptSystem::LuaGetEntityName);
		lua_setglobal(L, "engine_get_entity_name");

		lua_pushcfunction(L, &ScriptSystem::LuaCreateEntity);
		lua_setglobal(L, "engine_create_entity");

		lua_pushcfunction(L, &ScriptSystem::LuaDestroyEntity);
		lua_setglobal(L, "engine_destroy_entity");

		lua_pushcfunction(L, &ScriptSystem::LuaAddComponent);
		lua_setglobal(L, "engine_add_component");

		lua_pushcfunction(L, &ScriptSystem::LuaRemoveComponent);
		lua_setglobal(L, "engine_remove_component");

		lua_pushcfunction(L, &ScriptSystem::LuaGetPositionOf);
		lua_setglobal(L, "engine_get_position_of");

		lua_pushcfunction(L, &ScriptSystem::LuaSetPositionOf);
		lua_setglobal(L, "engine_set_position_of");

		lua_pushcfunction(L, &ScriptSystem::LuaTranslateOf);
		lua_setglobal(L, "engine_translate_of");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyDown);
		lua_setglobal(L, "engine_is_key_down");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyPressed);
		lua_setglobal(L, "engine_is_key_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyReleased);
		lua_setglobal(L, "engine_is_key_released");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonDown);
		lua_setglobal(L, "engine_is_mouse_button_down");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonPressed);
		lua_setglobal(L, "engine_is_mouse_button_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonReleased);
		lua_setglobal(L, "engine_is_mouse_button_released");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMouseDelta);
		lua_setglobal(L, "engine_get_mouse_delta");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMouseWheel);
		lua_setglobal(L, "engine_get_mouse_wheel");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMousePosition);
		lua_setglobal(L, "engine_get_mouse_position");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseCaptured);
		lua_setglobal(L, "engine_is_mouse_captured");

		lua_pushcfunction(L, &ScriptSystem::LuaHasRigidbody);
		lua_setglobal(L, "engine_has_rigidbody");

		lua_pushcfunction(L, &ScriptSystem::LuaGetVelocity);
		lua_setglobal(L, "engine_get_velocity");

		lua_pushcfunction(L, &ScriptSystem::LuaSetVelocity);
		lua_setglobal(L, "engine_set_velocity");

		lua_pushcfunction(L, &ScriptSystem::LuaGetGravityScale);
		lua_setglobal(L, "engine_get_gravity_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaSetGravityScale);
		lua_setglobal(L, "engine_set_gravity_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaGetKinematic);
		lua_setglobal(L, "engine_get_kinematic");

		lua_pushcfunction(L, &ScriptSystem::LuaSetKinematic);
		lua_setglobal(L, "engine_set_kinematic");

		lua_pushcfunction(L, &ScriptSystem::LuaHasAnimation);
		lua_setglobal(L, "engine_has_animation");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAnimationClipCount);
		lua_setglobal(L, "engine_get_animation_clip_count");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAnimationClipName);
		lua_setglobal(L, "engine_get_animation_clip_name");

		lua_pushcfunction(L, &ScriptSystem::LuaGetActiveAnimationClip);
		lua_setglobal(L, "engine_get_active_animation_clip");

		lua_pushcfunction(L, &ScriptSystem::LuaPlayAnimationClip);
		lua_setglobal(L, "engine_play_animation_clip");

		lua_pushcfunction(L, &ScriptSystem::LuaHasLight);
		lua_setglobal(L, "engine_has_light");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightColor);
		lua_setglobal(L, "engine_get_light_color");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightColor);
		lua_setglobal(L, "engine_set_light_color");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightIntensity);
		lua_setglobal(L, "engine_get_light_intensity");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightIntensity);
		lua_setglobal(L, "engine_set_light_intensity");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightRange);
		lua_setglobal(L, "engine_get_light_range");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightRange);
		lua_setglobal(L, "engine_set_light_range");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightCastShadows);
		lua_setglobal(L, "engine_get_light_cast_shadows");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightCastShadows);
		lua_setglobal(L, "engine_set_light_cast_shadows");

		lua_pushcfunction(L, &ScriptSystem::LuaTriggerAudioEvent);
		lua_setglobal(L, "engine_trigger_audio_event");

		lua_pushcfunction(L, &ScriptSystem::LuaSetAudioBusVolume);
		lua_setglobal(L, "engine_set_audio_bus_volume");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAudioBusVolume);
		lua_setglobal(L, "engine_get_audio_bus_volume");

		lua_pushcfunction(L, &ScriptSystem::LuaIsAction);
		lua_setglobal(L, "engine_is_action");

		lua_pushcfunction(L, &ScriptSystem::LuaIsActionPressed);
		lua_setglobal(L, "engine_is_action_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsActionReleased);
		lua_setglobal(L, "engine_is_action_released");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAxis);
		lua_setglobal(L, "engine_get_axis");

		lua_pushcfunction(L, &ScriptSystem::LuaIsGamepadConnected);
		lua_setglobal(L, "engine_is_gamepad_connected");

		InstallLuaHelpers(L, true);
	}

	void ScriptSystem::RegisterGlobalBindings(lua_State* L)
	{
		lua_pushlightuserdata(L, this);
		lua_setfield(L, LUA_REGISTRYINDEX, kSystemRegistryKey);

		lua_pushinteger(L, static_cast<lua_Integer>(0));
		lua_setfield(L, LUA_REGISTRYINDEX, kEntityRegistryKey);

		lua_pushcfunction(L, &ScriptSystem::LuaLog);
		lua_setglobal(L, "engine_log");

		lua_pushcfunction(L, &ScriptSystem::LuaGetPosition);
		lua_setglobal(L, "engine_get_position");

		lua_pushcfunction(L, &ScriptSystem::LuaSetPosition);
		lua_setglobal(L, "engine_set_position");

		lua_pushcfunction(L, &ScriptSystem::LuaTranslate);
		lua_setglobal(L, "engine_translate");

		lua_pushcfunction(L, &ScriptSystem::LuaGetRotation);
		lua_setglobal(L, "engine_get_rotation");

		lua_pushcfunction(L, &ScriptSystem::LuaSetRotation);
		lua_setglobal(L, "engine_set_rotation");

		lua_pushcfunction(L, &ScriptSystem::LuaGetScale);
		lua_setglobal(L, "engine_get_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaSetScale);
		lua_setglobal(L, "engine_set_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaFindEntityByName);
		lua_setglobal(L, "engine_find_entity_by_name");

		lua_pushcfunction(L, &ScriptSystem::LuaEntityExists);
		lua_setglobal(L, "engine_entity_exists");

		lua_pushcfunction(L, &ScriptSystem::LuaGetEntityName);
		lua_setglobal(L, "engine_get_entity_name");

		lua_pushcfunction(L, &ScriptSystem::LuaCreateEntity);
		lua_setglobal(L, "engine_create_entity");

		lua_pushcfunction(L, &ScriptSystem::LuaDestroyEntity);
		lua_setglobal(L, "engine_destroy_entity");

		lua_pushcfunction(L, &ScriptSystem::LuaAddComponent);
		lua_setglobal(L, "engine_add_component");

		lua_pushcfunction(L, &ScriptSystem::LuaRemoveComponent);
		lua_setglobal(L, "engine_remove_component");

		lua_pushcfunction(L, &ScriptSystem::LuaGetPositionOf);
		lua_setglobal(L, "engine_get_position_of");

		lua_pushcfunction(L, &ScriptSystem::LuaSetPositionOf);
		lua_setglobal(L, "engine_set_position_of");

		lua_pushcfunction(L, &ScriptSystem::LuaTranslateOf);
		lua_setglobal(L, "engine_translate_of");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyDown);
		lua_setglobal(L, "engine_is_key_down");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyPressed);
		lua_setglobal(L, "engine_is_key_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsKeyReleased);
		lua_setglobal(L, "engine_is_key_released");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonDown);
		lua_setglobal(L, "engine_is_mouse_button_down");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonPressed);
		lua_setglobal(L, "engine_is_mouse_button_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseButtonReleased);
		lua_setglobal(L, "engine_is_mouse_button_released");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMouseDelta);
		lua_setglobal(L, "engine_get_mouse_delta");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMouseWheel);
		lua_setglobal(L, "engine_get_mouse_wheel");

		lua_pushcfunction(L, &ScriptSystem::LuaGetMousePosition);
		lua_setglobal(L, "engine_get_mouse_position");

		lua_pushcfunction(L, &ScriptSystem::LuaIsMouseCaptured);
		lua_setglobal(L, "engine_is_mouse_captured");

		lua_pushcfunction(L, &ScriptSystem::LuaHasRigidbody);
		lua_setglobal(L, "engine_has_rigidbody");

		lua_pushcfunction(L, &ScriptSystem::LuaGetVelocity);
		lua_setglobal(L, "engine_get_velocity");

		lua_pushcfunction(L, &ScriptSystem::LuaSetVelocity);
		lua_setglobal(L, "engine_set_velocity");

		lua_pushcfunction(L, &ScriptSystem::LuaGetGravityScale);
		lua_setglobal(L, "engine_get_gravity_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaSetGravityScale);
		lua_setglobal(L, "engine_set_gravity_scale");

		lua_pushcfunction(L, &ScriptSystem::LuaGetKinematic);
		lua_setglobal(L, "engine_get_kinematic");

		lua_pushcfunction(L, &ScriptSystem::LuaSetKinematic);
		lua_setglobal(L, "engine_set_kinematic");

		lua_pushcfunction(L, &ScriptSystem::LuaHasAnimation);
		lua_setglobal(L, "engine_has_animation");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAnimationClipCount);
		lua_setglobal(L, "engine_get_animation_clip_count");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAnimationClipName);
		lua_setglobal(L, "engine_get_animation_clip_name");

		lua_pushcfunction(L, &ScriptSystem::LuaGetActiveAnimationClip);
		lua_setglobal(L, "engine_get_active_animation_clip");

		lua_pushcfunction(L, &ScriptSystem::LuaPlayAnimationClip);
		lua_setglobal(L, "engine_play_animation_clip");

		lua_pushcfunction(L, &ScriptSystem::LuaHasLight);
		lua_setglobal(L, "engine_has_light");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightColor);
		lua_setglobal(L, "engine_get_light_color");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightColor);
		lua_setglobal(L, "engine_set_light_color");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightIntensity);
		lua_setglobal(L, "engine_get_light_intensity");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightIntensity);
		lua_setglobal(L, "engine_set_light_intensity");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightRange);
		lua_setglobal(L, "engine_get_light_range");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightRange);
		lua_setglobal(L, "engine_set_light_range");

		lua_pushcfunction(L, &ScriptSystem::LuaGetLightCastShadows);
		lua_setglobal(L, "engine_get_light_cast_shadows");

		lua_pushcfunction(L, &ScriptSystem::LuaSetLightCastShadows);
		lua_setglobal(L, "engine_set_light_cast_shadows");

		lua_pushcfunction(L, &ScriptSystem::LuaTriggerAudioEvent);
		lua_setglobal(L, "engine_trigger_audio_event");

		lua_pushcfunction(L, &ScriptSystem::LuaSetAudioBusVolume);
		lua_setglobal(L, "engine_set_audio_bus_volume");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAudioBusVolume);
		lua_setglobal(L, "engine_get_audio_bus_volume");

		lua_pushcfunction(L, &ScriptSystem::LuaIsAction);
		lua_setglobal(L, "engine_is_action");

		lua_pushcfunction(L, &ScriptSystem::LuaIsActionPressed);
		lua_setglobal(L, "engine_is_action_pressed");

		lua_pushcfunction(L, &ScriptSystem::LuaIsActionReleased);
		lua_setglobal(L, "engine_is_action_released");

		lua_pushcfunction(L, &ScriptSystem::LuaGetAxis);
		lua_setglobal(L, "engine_get_axis");

		lua_pushcfunction(L, &ScriptSystem::LuaIsGamepadConnected);
		lua_setglobal(L, "engine_is_gamepad_connected");

		InstallLuaHelpers(L, false);
	}

	void ScriptSystem::CallScriptFunction(lua_State* L, const char* functionName, int argCount, int resultCount)
	{
		if (lua_pcall(L, argCount, resultCount, 0) != LUA_OK)
		{
			std::cerr << "[ScriptSystem] Lua error in " << functionName << ": " << lua_tostring(L, -1) << std::endl;
			lua_pop(L, 1);
		}
	}

	void ScriptSystem::CallEntityLifecycle(lua_State* L, const char* functionName, uint32_t entityID)
	{
		lua_getglobal(L, functionName);
		if (lua_isfunction(L, -1))
		{
			lua_pushinteger(L, static_cast<lua_Integer>(entityID));
			CallScriptFunction(L, functionName, 1, 0);
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	void ScriptSystem::CallGlobalLifecycle(lua_State* L, const char* functionName)
	{
		lua_getglobal(L, functionName);
		if (lua_isfunction(L, -1))
		{
			CallScriptFunction(L, functionName, 0, 0);
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	ScriptSystem* ScriptSystem::GetSystem(lua_State* L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, kSystemRegistryKey);
		auto* system = static_cast<ScriptSystem*>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return system;
	}

	uint32_t ScriptSystem::GetBoundEntityID(lua_State* L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, kEntityRegistryKey);
		uint32_t entityID = static_cast<uint32_t>(lua_tointeger(L, -1));
		lua_pop(L, 1);
		return entityID;
	}

	Entity* ScriptSystem::FindEntity(Scene& scene, uint32_t entityID)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (entity && entity->GetID() == entityID)
				return entity.get();
		}
		return nullptr;
	}

	int ScriptSystem::LuaLog(lua_State* L)
	{
		const char* message = luaL_checkstring(L, 1);
		std::cout << "[Lua] " << message << std::endl;
		return 0;
	}

	int ScriptSystem::LuaGetPosition(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_position: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.get_position: entity %u has no Transform component", entityID);

		const auto& transform = entity->GetComponent<TransformComponent>();
		lua_pushnumber(L, transform.position.x);
		lua_pushnumber(L, transform.position.y);
		lua_pushnumber(L, transform.position.z);
		return 3;
	}

	int ScriptSystem::LuaSetPosition(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_position: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.set_position: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.position.x = static_cast<float>(luaL_checknumber(L, 1));
		transform.position.y = static_cast<float>(luaL_checknumber(L, 2));
		transform.position.z = static_cast<float>(luaL_checknumber(L, 3));
		return 0;
	}

	int ScriptSystem::LuaTranslate(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.translate: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.translate: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.position.x += static_cast<float>(luaL_checknumber(L, 1));
		transform.position.y += static_cast<float>(luaL_checknumber(L, 2));
		transform.position.z += static_cast<float>(luaL_checknumber(L, 3));
		return 0;
	}

	int ScriptSystem::LuaGetRotation(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_rotation: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.get_rotation: entity %u has no Transform component", entityID);

		const auto& transform = entity->GetComponent<TransformComponent>();
		lua_pushnumber(L, transform.rotation.x);
		lua_pushnumber(L, transform.rotation.y);
		lua_pushnumber(L, transform.rotation.z);
		return 3;
	}

	int ScriptSystem::LuaSetRotation(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_rotation: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.set_rotation: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.rotation.x = static_cast<float>(luaL_checknumber(L, 1));
		transform.rotation.y = static_cast<float>(luaL_checknumber(L, 2));
		transform.rotation.z = static_cast<float>(luaL_checknumber(L, 3));
		return 0;
	}

	int ScriptSystem::LuaGetScale(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_scale: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.get_scale: entity %u has no Transform component", entityID);

		const auto& transform = entity->GetComponent<TransformComponent>();
		lua_pushnumber(L, transform.scale.x);
		lua_pushnumber(L, transform.scale.y);
		lua_pushnumber(L, transform.scale.z);
		return 3;
	}

	int ScriptSystem::LuaSetScale(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_scale: script runtime context is unavailable");

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.set_scale: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.scale.x = static_cast<float>(luaL_checknumber(L, 1));
		transform.scale.y = static_cast<float>(luaL_checknumber(L, 2));
		transform.scale.z = static_cast<float>(luaL_checknumber(L, 3));
		return 0;
	}

	int ScriptSystem::LuaFindEntityByName(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushnil(L);
			return 1;
		}

		const char* name = luaL_checkstring(L, 1);
		for (const auto& entity : system->m_CurrentScene->GetEntities())
		{
			if (entity && entity->GetName() == name)
			{
				lua_pushinteger(L, static_cast<lua_Integer>(entity->GetID()));
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	int ScriptSystem::LuaEntityExists(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		lua_pushboolean(L, entity ? 1 : 0);
		return 1;
	}

	int ScriptSystem::LuaGetEntityName(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushnil(L);
			return 1;
		}

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity)
		{
			lua_pushnil(L);
			return 1;
		}

		lua_pushstring(L, entity->GetName().c_str());
		return 1;
	}

	int ScriptSystem::LuaCreateEntity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.create_entity: script runtime context is unavailable");

		std::string name;
		if (lua_gettop(L) >= 1)
		{
			if (!lua_isstring(L, 1))
				return luaL_error(L, "engine.create_entity: expected optional string name");
			name = lua_tostring(L, 1);
		}

		auto entity = system->m_CurrentScene->CreateEntity(name);
		if (!entity)
			return luaL_error(L, "engine.create_entity: failed to create entity");

		if (!entity->HasComponent<TransformComponent>())
		{
			entity->AddComponent<TransformComponent>();
		}

		const lua_Integer newID = static_cast<lua_Integer>(entity->GetID());
		system->DispatchGlobalEvent("OnEntityCreated", static_cast<uint32_t>(newID));

		lua_pushinteger(L, newID);
		return 1;
	}

	int ScriptSystem::LuaDestroyEntity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.destroy_entity: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity)
			return luaL_error(L, "engine.destroy_entity: entity %u does not exist", entityID);

		system->DispatchGlobalEvent("OnEntityDestroyed", entityID);
		system->m_CurrentScene->DestroyEntity(entityID);
		system->UnloadScript(entityID);
		return 0;
	}

	int ScriptSystem::LuaAddComponent(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.add_component: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		const char* componentNameRaw = luaL_checkstring(L, 2);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity)
			return luaL_error(L, "engine.add_component: entity %u does not exist", entityID);

		const std::string componentName = NormalizeComponentName(componentNameRaw ? componentNameRaw : "");
		if (componentName == "transform")
		{
			if (!entity->HasComponent<TransformComponent>())
				entity->AddComponent<TransformComponent>();
		}
		else if (componentName == "rigidbody")
		{
			if (!entity->HasComponent<RigidbodyComponent>())
				entity->AddComponent<RigidbodyComponent>();
		}
		else if (componentName == "light")
		{
			if (!entity->HasComponent<LightComponent>())
				entity->AddComponent<LightComponent>();
		}
		else if (componentName == "script")
		{
			auto& script = entity->HasComponent<ScriptComponent>()
				? entity->GetComponent<ScriptComponent>()
				: entity->AddComponent<ScriptComponent>();

			if (lua_gettop(L) >= 3)
			{
				if (!lua_isstring(L, 3))
					return luaL_error(L, "engine.add_component(script): expected string scriptPath as arg #3");
				script.scriptPath = lua_tostring(L, 3);
			}
			if (lua_gettop(L) >= 4)
			{
				if (!lua_isboolean(L, 4))
					return luaL_error(L, "engine.add_component(script): expected boolean enabled as arg #4");
				script.enabled = lua_toboolean(L, 4) != 0;
			}
			if (lua_gettop(L) >= 5)
			{
				if (!lua_isboolean(L, 5))
					return luaL_error(L, "engine.add_component(script): expected boolean autoStart as arg #5");
				script.autoStart = lua_toboolean(L, 5) != 0;
			}
			script.requestReload = true;
		}
		else
		{
			return luaL_error(L, "engine.add_component: unsupported component '%s'", componentNameRaw ? componentNameRaw : "");
		}

		system->DispatchGlobalEventWithString("OnComponentAdded", entityID, componentName.c_str());

		lua_pushboolean(L, 1);
		return 1;
	}

	int ScriptSystem::LuaRemoveComponent(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.remove_component: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		const char* componentNameRaw = luaL_checkstring(L, 2);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity)
			return luaL_error(L, "engine.remove_component: entity %u does not exist", entityID);

		const std::string componentName = NormalizeComponentName(componentNameRaw ? componentNameRaw : "");
		if (componentName == "transform")
		{
			if (!entity->HasComponent<TransformComponent>())
				return luaL_error(L, "engine.remove_component: entity %u has no Transform component", entityID);
			entity->RemoveComponent<TransformComponent>();
		}
		else if (componentName == "rigidbody")
		{
			if (!entity->HasComponent<RigidbodyComponent>())
				return luaL_error(L, "engine.remove_component: entity %u has no Rigidbody component", entityID);
			entity->RemoveComponent<RigidbodyComponent>();
		}
		else if (componentName == "light")
		{
			if (!entity->HasComponent<LightComponent>())
				return luaL_error(L, "engine.remove_component: entity %u has no Light component", entityID);
			entity->RemoveComponent<LightComponent>();
		}
		else if (componentName == "script")
		{
			if (!entity->HasComponent<ScriptComponent>())
				return luaL_error(L, "engine.remove_component: entity %u has no Script component", entityID);
			entity->RemoveComponent<ScriptComponent>();
			system->UnloadScript(entityID);
		}
		else
		{
			return luaL_error(L, "engine.remove_component: unsupported component '%s'", componentNameRaw ? componentNameRaw : "");
		}

		system->DispatchGlobalEventWithString("OnComponentRemoved", entityID, componentName.c_str());

		lua_pushboolean(L, 1);
		return 1;
	}

	int ScriptSystem::LuaGetPositionOf(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_position_of: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.get_position_of: entity %u has no Transform component", entityID);

		const auto& transform = entity->GetComponent<TransformComponent>();
		lua_pushnumber(L, transform.position.x);
		lua_pushnumber(L, transform.position.y);
		lua_pushnumber(L, transform.position.z);
		return 3;
	}

	int ScriptSystem::LuaSetPositionOf(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_position_of: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.set_position_of: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.position.x = static_cast<float>(luaL_checknumber(L, 2));
		transform.position.y = static_cast<float>(luaL_checknumber(L, 3));
		transform.position.z = static_cast<float>(luaL_checknumber(L, 4));
		return 0;
	}

	int ScriptSystem::LuaTranslateOf(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.translate_of: script runtime context is unavailable");

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return luaL_error(L, "engine.translate_of: entity %u has no Transform component", entityID);

		auto& transform = entity->GetComponent<TransformComponent>();
		transform.position.x += static_cast<float>(luaL_checknumber(L, 2));
		transform.position.y += static_cast<float>(luaL_checknumber(L, 3));
		transform.position.z += static_cast<float>(luaL_checknumber(L, 4));
		return 0;
	}

	int ScriptSystem::LuaIsKeyDown(lua_State* L)
	{
		lua_pushboolean(L, Input::IsKeyDown(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaIsKeyPressed(lua_State* L)
	{
		lua_pushboolean(L, Input::IsKeyPressed(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaIsKeyReleased(lua_State* L)
	{
		lua_pushboolean(L, Input::IsKeyReleased(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaIsMouseButtonDown(lua_State* L)
	{
		lua_pushboolean(L, Input::IsMouseButtonDown(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaIsMouseButtonPressed(lua_State* L)
	{
		lua_pushboolean(L, Input::IsMouseButtonPressed(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaIsMouseButtonReleased(lua_State* L)
	{
		lua_pushboolean(L, Input::IsMouseButtonReleased(static_cast<int>(luaL_checkinteger(L, 1))));
		return 1;
	}

	int ScriptSystem::LuaGetMouseDelta(lua_State* L)
	{
		lua_pushnumber(L, Input::GetMouseDeltaX());
		lua_pushnumber(L, Input::GetMouseDeltaY());
		return 2;
	}

	int ScriptSystem::LuaGetMouseWheel(lua_State* L)
	{
		lua_pushnumber(L, Input::GetMouseWheelX());
		lua_pushnumber(L, Input::GetMouseWheelY());
		return 2;
	}

	int ScriptSystem::LuaGetMousePosition(lua_State* L)
	{
		GLFWwindow* window = Input::GetWindow();
		if (!window)
		{
			lua_pushnil(L);
			return 1;
		}

		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		lua_pushnumber(L, x);
		lua_pushnumber(L, y);
		return 2;
	}

	int ScriptSystem::LuaIsMouseCaptured(lua_State* L)
	{
		lua_pushboolean(L, Input::IsMouseCaptured());
		return 1;
	}

	int ScriptSystem::LuaIsAction(lua_State* L)
	{
		lua_pushboolean(L, MyEngine::InputActions::IsAction(luaL_checkstring(L, 1)));
		return 1;
	}

	int ScriptSystem::LuaIsActionPressed(lua_State* L)
	{
		lua_pushboolean(L, MyEngine::InputActions::IsActionPressed(luaL_checkstring(L, 1)));
		return 1;
	}

	int ScriptSystem::LuaIsActionReleased(lua_State* L)
	{
		lua_pushboolean(L, MyEngine::InputActions::IsActionReleased(luaL_checkstring(L, 1)));
		return 1;
	}

	int ScriptSystem::LuaGetAxis(lua_State* L)
	{
		lua_pushnumber(L, MyEngine::InputActions::GetAxis(luaL_checkstring(L, 1)));
		return 1;
	}

	int ScriptSystem::LuaIsGamepadConnected(lua_State* L)
	{
		lua_pushboolean(L, MyEngine::InputActions::IsGamepadConnected());
		return 1;
	}

	int ScriptSystem::LuaHasRigidbody(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		lua_pushboolean(L, entity && entity->HasComponent<RigidbodyComponent>());
		return 1;
	}

	int ScriptSystem::LuaGetVelocity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_velocity: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_velocity: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.get_velocity: entity %u has no Rigidbody component", targetEntityID);

		const auto& rb = entity->GetComponent<RigidbodyComponent>();
		lua_pushnumber(L, rb.velocity.x);
		lua_pushnumber(L, rb.velocity.y);
		lua_pushnumber(L, rb.velocity.z);
		return 3;
	}

	int ScriptSystem::LuaSetVelocity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_velocity: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 3, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_velocity: expected (x, y, z) or (entityID, x, y, z)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.set_velocity: entity %u has no Rigidbody component", targetEntityID);

		auto& rb = entity->GetComponent<RigidbodyComponent>();
		rb.velocity.x = static_cast<float>(luaL_checknumber(L, valueStartIndex));
		rb.velocity.y = static_cast<float>(luaL_checknumber(L, valueStartIndex + 1));
		rb.velocity.z = static_cast<float>(luaL_checknumber(L, valueStartIndex + 2));
		return 0;
	}

	int ScriptSystem::LuaGetGravityScale(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_gravity_scale: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_gravity_scale: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.get_gravity_scale: entity %u has no Rigidbody component", targetEntityID);

		lua_pushnumber(L, entity->GetComponent<RigidbodyComponent>().gravityScale);
		return 1;
	}

	int ScriptSystem::LuaSetGravityScale(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_gravity_scale: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_gravity_scale: expected (value) or (entityID, value)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.set_gravity_scale: entity %u has no Rigidbody component", targetEntityID);

		entity->GetComponent<RigidbodyComponent>().gravityScale = static_cast<float>(luaL_checknumber(L, valueStartIndex));
		return 0;
	}

	int ScriptSystem::LuaGetKinematic(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_kinematic: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_kinematic: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.get_kinematic: entity %u has no Rigidbody component", targetEntityID);

		lua_pushboolean(L, entity->GetComponent<RigidbodyComponent>().isKinematic ? 1 : 0);
		return 1;
	}

	int ScriptSystem::LuaSetKinematic(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_kinematic: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_kinematic: expected (isKinematic) or (entityID, isKinematic)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<RigidbodyComponent>())
			return luaL_error(L, "engine.set_kinematic: entity %u has no Rigidbody component", targetEntityID);

		entity->GetComponent<RigidbodyComponent>().isKinematic = lua_toboolean(L, valueStartIndex) != 0;
		return 0;
	}

	int ScriptSystem::LuaHasAnimation(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		const bool hasAnimation = entity && entity->HasComponent<AnimationComponent>() &&
			entity->GetComponent<AnimationComponent>().clips &&
			!entity->GetComponent<AnimationComponent>().clips->empty();
		lua_pushboolean(L, hasAnimation ? 1 : 0);
		return 1;
	}

	int ScriptSystem::LuaGetAnimationClipCount(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_animation_clip_count: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_animation_clip_count: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<AnimationComponent>())
			return luaL_error(L, "engine.get_animation_clip_count: entity %u has no Animation component", targetEntityID);

		const auto& anim = entity->GetComponent<AnimationComponent>();
		lua_pushinteger(L, static_cast<lua_Integer>(anim.clips ? anim.clips->size() : 0));
		return 1;
	}

	int ScriptSystem::LuaGetAnimationClipName(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_animation_clip_name: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.get_animation_clip_name: expected (clipIndex) or (entityID, clipIndex)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<AnimationComponent>())
			return luaL_error(L, "engine.get_animation_clip_name: entity %u has no Animation component", targetEntityID);

		const auto& anim = entity->GetComponent<AnimationComponent>();
		if (!anim.clips)
			return luaL_error(L, "engine.get_animation_clip_name: entity %u has no animation clips", targetEntityID);

		int clipIndex = static_cast<int>(luaL_checkinteger(L, valueStartIndex));
		if (clipIndex < 0 || clipIndex >= static_cast<int>(anim.clips->size()))
			return luaL_error(L, "engine.get_animation_clip_name: clip index %d is out of range [0, %d)", clipIndex, static_cast<int>(anim.clips->size()));

		lua_pushstring(L, (*anim.clips)[clipIndex].name.c_str());
		return 1;
	}

	int ScriptSystem::LuaGetActiveAnimationClip(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_active_animation_clip: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_active_animation_clip: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<AnimationComponent>())
			return luaL_error(L, "engine.get_active_animation_clip: entity %u has no Animation component", targetEntityID);

		lua_pushinteger(L, static_cast<lua_Integer>(entity->GetComponent<AnimationComponent>().activeClipIndex));
		return 1;
	}

	int ScriptSystem::LuaPlayAnimationClip(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.play_animation_clip: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.play_animation_clip: expected (clipIndex [, blendDuration]) or (entityID, clipIndex [, blendDuration])");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<AnimationComponent>())
			return luaL_error(L, "engine.play_animation_clip: entity %u has no Animation component", targetEntityID);

		auto& anim = entity->GetComponent<AnimationComponent>();
		if (!anim.clips || anim.clips->empty())
			return luaL_error(L, "engine.play_animation_clip: entity %u has no animation clips", targetEntityID);

		int clipIndex = static_cast<int>(luaL_checkinteger(L, valueStartIndex));
		if (clipIndex < 0 || clipIndex >= static_cast<int>(anim.clips->size()))
			return luaL_error(L, "engine.play_animation_clip: clip index %d is out of range [0, %d)", clipIndex, static_cast<int>(anim.clips->size()));

		float blendDuration = 0.15f;
		if (lua_gettop(L) >= valueStartIndex + 1 && lua_isnumber(L, valueStartIndex + 1))
			blendDuration = static_cast<float>(lua_tonumber(L, valueStartIndex + 1));

		anim.playing = true;
		anim.TransitionTo(clipIndex, blendDuration);
		lua_pushboolean(L, 1);
		return 1;
	}

	int ScriptSystem::LuaHasLight(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		lua_pushboolean(L, entity && entity->HasComponent<LightComponent>());
		return 1;
	}

	int ScriptSystem::LuaGetLightColor(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_light_color: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_light_color: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.get_light_color: entity %u has no Light component", targetEntityID);

		const auto& light = entity->GetComponent<LightComponent>();
		lua_pushnumber(L, light.color.x);
		lua_pushnumber(L, light.color.y);
		lua_pushnumber(L, light.color.z);
		return 3;
	}

	int ScriptSystem::LuaSetLightColor(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_light_color: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 3, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_light_color: expected (r, g, b) or (entityID, r, g, b)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.set_light_color: entity %u has no Light component", targetEntityID);

		auto& light = entity->GetComponent<LightComponent>();
		light.color.x = static_cast<float>(luaL_checknumber(L, valueStartIndex));
		light.color.y = static_cast<float>(luaL_checknumber(L, valueStartIndex + 1));
		light.color.z = static_cast<float>(luaL_checknumber(L, valueStartIndex + 2));
		return 0;
	}

	int ScriptSystem::LuaGetLightIntensity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_light_intensity: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_light_intensity: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.get_light_intensity: entity %u has no Light component", targetEntityID);

		lua_pushnumber(L, entity->GetComponent<LightComponent>().intensity);
		return 1;
	}

	int ScriptSystem::LuaSetLightIntensity(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_light_intensity: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_light_intensity: expected (value) or (entityID, value)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.set_light_intensity: entity %u has no Light component", targetEntityID);

		entity->GetComponent<LightComponent>().intensity = static_cast<float>(luaL_checknumber(L, valueStartIndex));
		return 0;
	}

	int ScriptSystem::LuaGetLightRange(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.get_light_range: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
			return luaL_error(L, "engine.get_light_range: invalid entity target or arguments");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.get_light_range: entity %u has no Light component", targetEntityID);

		lua_pushnumber(L, entity->GetComponent<LightComponent>().range);
		return 1;
	}

	int ScriptSystem::LuaSetLightRange(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_light_range: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_light_range: expected (value) or (entityID, value)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.set_light_range: entity %u has no Light component", targetEntityID);

		entity->GetComponent<LightComponent>().range = static_cast<float>(luaL_checknumber(L, valueStartIndex));
		return 0;
	}

	int ScriptSystem::LuaGetLightCastShadows(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		uint32_t targetEntityID = 0;
		int ignoredValueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 0, targetEntityID, ignoredValueStartIndex))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		lua_pushboolean(L, entity && entity->HasComponent<LightComponent>() && entity->GetComponent<LightComponent>().castShadows);
		return 1;
	}

	int ScriptSystem::LuaSetLightCastShadows(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
			return luaL_error(L, "engine.set_light_cast_shadows: script runtime context is unavailable");

		uint32_t targetEntityID = 0;
		int valueStartIndex = 1;
		if (!ResolveTargetEntity(L, *system->m_CurrentScene, GetBoundEntityID(L), 1, targetEntityID, valueStartIndex))
			return luaL_error(L, "engine.set_light_cast_shadows: expected (enabled) or (entityID, enabled)");

		Entity* entity = FindEntity(*system->m_CurrentScene, targetEntityID);
		if (!entity || !entity->HasComponent<LightComponent>())
			return luaL_error(L, "engine.set_light_cast_shadows: entity %u has no Light component", targetEntityID);

		entity->GetComponent<LightComponent>().castShadows = lua_toboolean(L, valueStartIndex) != 0;
		return 0;
	}

	int ScriptSystem::LuaTriggerAudioEvent(lua_State* L)
	{
		const char* eventName = luaL_checkstring(L, 1);
		AudioSystem::QueueEvent(eventName);
		return 0;
	}

	int ScriptSystem::LuaSetAudioBusVolume(lua_State* L)
	{
		const char* busName = luaL_checkstring(L, 1);
		float volume = static_cast<float>(luaL_checknumber(L, 2));
		AudioEngine::SetBusVolume(busName, volume);
		return 0;
	}

	int ScriptSystem::LuaGetAudioBusVolume(lua_State* L)
	{
		const char* busName = luaL_optstring(L, 1, "Master");
		lua_pushnumber(L, AudioEngine::GetBusVolume(busName));
		return 1;
	}
}
