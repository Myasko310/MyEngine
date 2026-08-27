#pragma once

#include <memory>
#include <string>
#include <vector>

#include "systems/ScriptSystem.h"

namespace MyEngine
{
	class Material;
	class AnimationStateMachine;

	namespace Editor
	{
		struct UIState
		{
			bool showSceneHierarchy = true;
			bool showInspector = true;
			bool showLightingPanel = true;
			bool showPostProcessPanel = true;
			bool showSkyboxPanel = true;
			bool showScriptingPanel = true;
			bool showPerformancePanel = true;
			bool showPhysicsPanel = true;
			bool showAssetBrowser = true;
			bool showMaterialBrowser = false;
			bool showIBLPanel = false;
			bool showLayerManager = false;

			std::string assetBrowserPath = "assets";
			std::string materialBrowserPath = "assets/materials";
			std::string selectedMaterialPath;
			std::shared_ptr<Material> editingMaterial;
			char materialRenameBuffer[256] = {};
			bool materialRenameActive = false;
			char newMaterialNameBuffer[256] = "new_material.material.json";
			bool showNewMaterialDialog = false;

			std::string animationStateMachineBrowserPath = "assets/animation";
			std::string selectedAnimationStateMachinePath;
			std::shared_ptr<AnimationStateMachine> editingAnimationStateMachine;

			std::vector<std::string> matBrowserTextures;
			bool matBrowserTexturesScanned = false;
			std::vector<ScriptSystem::GlobalScriptConfig> globalScripts = { ScriptSystem::GlobalScriptConfig{} };
		};
	}
}
