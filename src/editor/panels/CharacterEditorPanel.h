#pragma once

#include "editor/panels/DebugPanelContexts.h"

namespace MyEngine::Editor::Panels
{
	// Character Editor (Milestone 1): bone hierarchy tree, skeleton overlay,
	// per-bone override sliders, ImGuizmo bone posing and *.character.json
	// preset save/load. Sets context.boneGizmoActive when a bone gizmo was
	// drawn this frame so the caller can suppress the entity gizmo.
	void DrawCharacterEditorPanel(CharacterEditorPanelContext& context);
}
