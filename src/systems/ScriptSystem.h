#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class Scene;
class Entity;
struct ScriptComponent;

namespace MyEngine
{
	class ScriptSystem
	{
	public:
		struct GlobalScriptConfig
		{
			std::string scriptPath;
			bool enabled = true;
			bool autoStart = true;
			bool requestReload = false;
		};

		ScriptSystem() = default;
		~ScriptSystem();

		ScriptSystem(const ScriptSystem&) = delete;
		ScriptSystem& operator=(const ScriptSystem&) = delete;

		void OnUpdate(Scene& scene, float deltaTime);
		void SetGlobalScripts(const std::vector<GlobalScriptConfig>& scripts);

	private:
		struct ScriptState
		{
			lua_State* luaState = nullptr;
			std::string scriptPath;
			bool started = false;
			bool wasEnabled = false;
			bool disablePending = false;
		};

	private:
		void SyncEntity(Scene& scene, Entity& entity, float deltaTime);
		void SyncGlobalScripts(Scene& scene, float deltaTime);
		void SyncScriptCollisionCallbacks(Entity& entity);
		void ClearScriptCollisionCallbacks(Entity& entity);
		void DispatchScriptCollisionEvent(uint32_t entityID, const std::shared_ptr<Entity>& otherEntity, const char* functionName);
		void DispatchGlobalEvent(const char* functionName, uint32_t entityID);
		void DispatchGlobalEventWithString(const char* functionName, uint32_t entityID, const char* strArg);
		void UnloadScript(uint32_t entityID);
		bool LoadScript(Scene& scene, Entity& entity, ScriptState& state, const ::ScriptComponent& scriptComponent);
		bool LoadGlobalScript(Scene& scene, ScriptState& state, const GlobalScriptConfig& config);
		void RegisterBindings(lua_State* L, uint32_t entityID);
		void RegisterGlobalBindings(lua_State* L);
		void CallScriptFunction(lua_State* L, const char* functionName, int argCount, int resultCount);
		void CallEntityLifecycle(lua_State* L, const char* functionName, uint32_t entityID);
		void CallGlobalLifecycle(lua_State* L, const char* functionName);

		static ScriptSystem* GetSystem(lua_State* L);
		static uint32_t GetBoundEntityID(lua_State* L);
		static Entity* FindEntity(Scene& scene, uint32_t entityID);

		static int LuaLog(lua_State* L);
		static int LuaGetPosition(lua_State* L);
		static int LuaSetPosition(lua_State* L);
		static int LuaTranslate(lua_State* L);
		static int LuaGetRotation(lua_State* L);
		static int LuaSetRotation(lua_State* L);
		static int LuaGetScale(lua_State* L);
		static int LuaSetScale(lua_State* L);
		static int LuaFindEntityByName(lua_State* L);
		static int LuaEntityExists(lua_State* L);
		static int LuaGetEntityName(lua_State* L);
		static int LuaCreateEntity(lua_State* L);
		static int LuaDestroyEntity(lua_State* L);
		static int LuaAddComponent(lua_State* L);
		static int LuaRemoveComponent(lua_State* L);
		static int LuaGetPositionOf(lua_State* L);
		static int LuaSetPositionOf(lua_State* L);
		static int LuaTranslateOf(lua_State* L);
		static int LuaIsKeyDown(lua_State* L);
		static int LuaIsKeyPressed(lua_State* L);
		static int LuaIsKeyReleased(lua_State* L);
		static int LuaIsMouseButtonDown(lua_State* L);
		static int LuaIsMouseButtonPressed(lua_State* L);
		static int LuaIsMouseButtonReleased(lua_State* L);
		static int LuaGetMouseDelta(lua_State* L);
		static int LuaGetMouseWheel(lua_State* L);
		static int LuaGetMousePosition(lua_State* L);
		static int LuaIsMouseCaptured(lua_State* L);
		static int LuaIsAction(lua_State* L);
		static int LuaIsActionPressed(lua_State* L);
		static int LuaIsActionReleased(lua_State* L);
		static int LuaGetAxis(lua_State* L);
		static int LuaIsGamepadConnected(lua_State* L);
		static int LuaHasRigidbody(lua_State* L);
		static int LuaGetVelocity(lua_State* L);
		static int LuaSetVelocity(lua_State* L);
		static int LuaGetGravityScale(lua_State* L);
		static int LuaSetGravityScale(lua_State* L);
		static int LuaGetKinematic(lua_State* L);
		static int LuaSetKinematic(lua_State* L);
		static int LuaHasAnimation(lua_State* L);
		static int LuaGetAnimationClipCount(lua_State* L);
		static int LuaGetAnimationClipName(lua_State* L);
		static int LuaGetActiveAnimationClip(lua_State* L);
		static int LuaPlayAnimationClip(lua_State* L);
		static int LuaHasLight(lua_State* L);
		static int LuaGetLightColor(lua_State* L);
		static int LuaSetLightColor(lua_State* L);
		static int LuaGetLightIntensity(lua_State* L);
		static int LuaSetLightIntensity(lua_State* L);
		static int LuaGetLightRange(lua_State* L);
		static int LuaSetLightRange(lua_State* L);
		static int LuaGetLightCastShadows(lua_State* L);
		static int LuaSetLightCastShadows(lua_State* L);

	private:
		std::unordered_map<uint32_t, ScriptState> m_States;
		std::vector<GlobalScriptConfig> m_GlobalScripts;
		std::vector<ScriptState> m_GlobalStates;
		Scene* m_CurrentScene = nullptr;
	};
}
