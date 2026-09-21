#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "editor/EditorUndo.h"
#include "systems/ScriptSystem.h"

namespace MyEngine
{
	class Shader;

	namespace Editor
	{
		struct PlayModeRestoreContext
		{
			::Scene* scene = nullptr;
			::Entity** selectedEntity = nullptr;
			std::shared_ptr<::Entity>* playerEntity = nullptr;
			EditorUndo::UndoStack* undoStack = nullptr;
			std::shared_ptr<Shader> defaultShader;
			std::vector<ScriptSystem::GlobalScriptConfig>* globalScripts = nullptr;
		};

		struct StopPlayPromptContext
		{
			bool* isPlaying = nullptr;
			::Scene* scene = nullptr;
			std::vector<ScriptSystem::GlobalScriptConfig>* globalScripts = nullptr;
			std::vector<std::string>* recentScenes = nullptr;
			std::function<void(std::vector<std::string>&, const std::string&)> addRecentScene;
			PlayModeRestoreContext restoreContext;
		};

		class PlayModeSession
		{
		public:
			void CaptureSnapshot(const ::Scene& scene, const std::vector<ScriptSystem::GlobalScriptConfig>& globalScripts);
			void RestoreSnapshot(PlayModeRestoreContext& context);

			bool HasSnapshot() const;
			const std::string& GetSnapshotJson() const;
			void ClearSnapshot();

			void RequestStopPrompt();
			bool IsStopPromptPending() const;

#ifdef USE_IMGUI
			void DrawStopPlayPrompt(StopPlayPromptContext& context);
#endif

		private:
			bool m_HasSnapshot = false;
			std::string m_SnapshotJson;
			bool m_StopPromptPending = false;
		};
	}
}
