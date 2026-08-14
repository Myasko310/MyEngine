#include "systems/ScriptSystem.h"

#include "components/ScriptComponent.h"
#include "components/TransformComponent.h"
#include "core/Input.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"

#include <glm/glm.hpp>

#include <algorithm>
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

	void ScriptSystem::SyncEntity(Scene& scene, Entity& entity, float deltaTime)
	{
		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		uint32_t entityID = entity.GetID();

		if (!scriptComponent.enabled || scriptComponent.scriptPath.empty())
		{
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

		if (!state.luaState)
		{
			if (!LoadScript(scene, entity, state, scriptComponent))
			{
				UnloadScript(entityID);
				return;
			}
		}

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

			if (!config.enabled || config.scriptPath.empty())
			{
				if (state.luaState)
					lua_close(state.luaState);
				state = ScriptState{};
				continue;
			}

			if (state.scriptPath != config.scriptPath || config.requestReload)
			{
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
		{
			lua_pushnil(L);
			return 1;
		}

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
		{
			lua_pushnil(L);
			return 1;
		}

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
			return 0;

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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
			return 0;

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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
		{
			lua_pushnil(L);
			return 1;
		}

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
		{
			lua_pushnil(L);
			return 1;
		}

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
			return 0;

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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
		{
			lua_pushnil(L);
			return 1;
		}

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
		{
			lua_pushnil(L);
			return 1;
		}

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
			return 0;

		uint32_t entityID = GetBoundEntityID(L);
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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

	int ScriptSystem::LuaGetPositionOf(lua_State* L)
	{
		auto* system = GetSystem(L);
		if (!system || !system->m_CurrentScene)
		{
			lua_pushnil(L);
			return 1;
		}

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
		{
			lua_pushnil(L);
			return 1;
		}

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
			return 0;

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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
			return 0;

		uint32_t entityID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
		Entity* entity = FindEntity(*system->m_CurrentScene, entityID);
		if (!entity || !entity->HasComponent<TransformComponent>())
			return 0;

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
}
