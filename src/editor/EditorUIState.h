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
			bool showInputBindingsPanel = true;
			bool showPostProcessPanel = true;
			bool showSkyboxPanel = true;
			bool showScriptingPanel = true;
			bool showPerformancePanel = true;
			bool showPhysicsPanel = true;
			bool showSceneTimePanel = true;
			bool showCombatPanel = true;
			bool drawCombatShapesOverlay = true;
			bool showCharacterEditorPanel = false;
			bool showColliderDebugOverlay = false;
			bool showAssetBrowser = true;
			bool showAiAssetGeneratorPanel = true;
			bool showMaterialBrowser = false;
			bool showIBLPanel = false;
			bool showLayerManager = false;
			bool showViewportHud = true;
			bool showViewportHudCrosshair = false;
			bool showViewportHudHints = true;
			bool viewportHudCompact = false;
			bool viewportHudUseIcons = true;
			int viewportHudAnchorPreset = 0;
			int viewportHudPreset = 1;
			float viewportHudOpacity = 0.85f;

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
