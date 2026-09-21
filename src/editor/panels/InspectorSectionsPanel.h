#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "animation/AnimationStateMachine.h"
#include "ecs/Entity.h"
#include "editor/EditorUndo.h"

class Scene;

namespace MyEngine::Editor::Panels
{
	struct InspectorSectionState
	{
		int selectedOverrideIndex = 0;
		std::string prefabVariantStatus;
		bool prefabVariantStatusIsError = false;
		std::string prefabOverrideActionStatus;
		bool prefabOverrideActionStatusIsError = false;
		std::string animationBootstrapStatus;
		std::string animationImportStatus;
		std::string animationRetargetStatus;
	};

	struct InspectorSectionsContext
	{
		::Scene* scene = nullptr;
		::Entity* selectedEntity = nullptr;
		EditorUndo::UndoStack* undoStack = nullptr;
		InspectorSectionState* state = nullptr;
		std::string* selectedAnimationStateMachinePath = nullptr;
		std::shared_ptr<MyEngine::AnimationStateMachine>* editingAnimationStateMachine = nullptr;

		std::function<std::vector<std::string>(::Entity*)> describePrefabOverrides;
		std::function<bool(::Entity*, const std::string&, std::string&)> revertSelectedPrefabOverride;
		std::function<bool(::Entity*, std::string&)> applyAllPrefabOverrides;
	};

	void DrawInspectorPrefabAndEntitySections(InspectorSectionsContext& context);
	void DrawInspectorTransformAndCameraSections(InspectorSectionsContext& context);
	void DrawInspectorPhysicsSections(InspectorSectionsContext& context);
	void DrawInspectorCombatSections(InspectorSectionsContext& context);
	void DrawInspectorJointSections(InspectorSectionsContext& context);
	void DrawInspectorAnimationSections(InspectorSectionsContext& context);
	void DrawInspectorAudioSections(InspectorSectionsContext& context);
	void DrawInspectorScriptSections(InspectorSectionsContext& context);
	void DrawInspectorParticleSections(InspectorSectionsContext& context);
	void DrawInspectorAudioListenerSections(InspectorSectionsContext& context);
}
