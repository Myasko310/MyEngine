#pragma once

#include "editor/panels/DebugPanelContexts.h"

namespace MyEngine::Editor::Panels
{
	bool SelectCharacterEditorTarget(CharacterEditorPanelContext& context, uint32_t entityID);

	// Called before rendering/picking. Replaces only the supplied matrices;
	// returns false when the scene camera should remain active.
	bool UpdateCharacterPreviewCamera(CharacterEditorPanelContext& context);

	// Character Editor (Milestone 1): bone hierarchy tree, skeleton overlay,
	// per-bone override sliders, ImGuizmo bone posing and *.character.json
	// preset save/load. Sets context.boneGizmoActive when a bone gizmo was
	// drawn this frame so the caller can suppress the entity gizmo.
	void DrawCharacterEditorPanel(CharacterEditorPanelContext& context);
}
