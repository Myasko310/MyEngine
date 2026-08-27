#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "editor/EditorUIState.h"
#include "editor/EditorUndo.h"

struct GLFWwindow;

namespace MyEngine
{
	class Shader;

	namespace Editor
	{
		struct Context
		{
			Scene* scene = nullptr;
			std::shared_ptr<Shader> litShader;
			std::shared_ptr<Shader> litSkinnedShader;
			Entity** selectedEntity = nullptr;
			std::shared_ptr<Entity>* playerEntity = nullptr;
			UIState* ui = nullptr;
			EditorUndo::UndoStack* undoStack = nullptr;
			bool* isPlaying = nullptr;
			bool* wireframe = nullptr;
			std::string* currentScenePath = nullptr;
			std::vector<std::string>* recentScenes = nullptr;
			GLFWwindow* window = nullptr;
			std::function<void(bool)> setPlaying;
			std::function<void(bool)> applyWireframe;
			std::function<int()> getGizmoOperation;
			std::function<void(int)> setGizmoOperation;
			std::function<int()> getGizmoMode;
			std::function<void(int)> setGizmoMode;
			std::function<void(std::vector<std::string>&, const std::string&)> addRecentScene;
			std::function<void(GLFWwindow*, const std::string&)> updateWindowTitle;
			std::function<std::string()> captureSceneState;
			std::function<void(const std::string&, const std::string&)> pushSceneStateCommand;
		};
	}
}
