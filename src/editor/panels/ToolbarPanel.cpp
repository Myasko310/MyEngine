#include "ToolbarPanel.h"

#ifdef USE_IMGUI
#include "editor/EditorStyle.h"

namespace MyEngine::Editor::Panels
{
	void DrawToolbarPanel(Context& context)
	{
		if (!context.undoStack || !context.scene || !context.isPlaying || !context.wireframe)
			return;

		float menuBarHeight = ImGui::GetFrameHeight();
		float toolbarHeight = ImGui::GetFrameHeight() + 12.0f;

		ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarHeight));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, toolbarHeight));
		ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.10f, 0.12f, 1.00f));
		ImGui::Begin("Toolbar", nullptr, toolbarFlags);

		auto toolbarSectionBreak = []()
		{
			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
		};
		auto toolbarLabel = [](const char* label)
		{
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine();
		};
		auto toolbarToggleButton = [](const char* label, bool active, float width = 0.0f) -> bool
		{
			if (active)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.43f, 0.73f, 0.95f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.29f, 0.50f, 0.82f, 1.00f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.34f, 0.58f, 0.92f, 1.00f));
				bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
				ImGui::PopStyleColor(3);
				ImGui::PopStyleVar();
				return pressed;
			}
			return ImGui::Button(label, ImVec2(width, 0.0f));
		};

		toolbarLabel("Session");
		ImGui::PushStyleColor(ImGuiCol_Button, *context.isPlaying ? ImVec4(0.80f, 0.30f, 0.30f, 1.00f) : ImVec4(0.30f, 0.75f, 0.35f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, *context.isPlaying ? ImVec4(0.88f, 0.36f, 0.36f, 1.00f) : ImVec4(0.36f, 0.82f, 0.42f, 1.00f));
		if (ImGui::Button(*context.isPlaying ? "Stop" : "Play", ImVec2(74.0f, 0.0f)) && context.setPlaying)
			context.setPlaying(!*context.isPlaying);
		ImGui::PopStyleColor(2);

		ImGui::SameLine();
		ImGui::BeginDisabled(!context.undoStack->CanUndo());
		if (ImGui::Button("Undo", ImVec2(64.0f, 0.0f)))
			context.undoStack->Undo(*context.scene);
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(!context.undoStack->CanRedo());
		if (ImGui::Button("Redo", ImVec2(64.0f, 0.0f)))
			context.undoStack->Redo(*context.scene);
		ImGui::EndDisabled();

#ifdef USE_IMGUIZMO
		toolbarSectionBreak();
		toolbarLabel("Gizmo");
		const int gizmoOperation = context.getGizmoOperation ? context.getGizmoOperation() : 0;
		if (toolbarToggleButton("Move", gizmoOperation == 0, 72.0f) && context.setGizmoOperation)
			context.setGizmoOperation(0);
		ImGui::SameLine();
		if (toolbarToggleButton("Rotate", gizmoOperation == 1, 72.0f) && context.setGizmoOperation)
			context.setGizmoOperation(1);
		ImGui::SameLine();
		if (toolbarToggleButton("Scale", gizmoOperation == 2, 72.0f) && context.setGizmoOperation)
			context.setGizmoOperation(2);

		toolbarSectionBreak();
		toolbarLabel("Space");
		const int gizmoMode = context.getGizmoMode ? context.getGizmoMode() : 0;
		bool isWorldSpaceToolbar = (gizmoMode == 0);
		if (toolbarToggleButton("World", isWorldSpaceToolbar, 64.0f) && context.setGizmoMode)
			context.setGizmoMode(0);
		ImGui::SameLine();
		if (toolbarToggleButton("Local", !isWorldSpaceToolbar, 64.0f) && context.setGizmoMode)
			context.setGizmoMode(1);

		toolbarSectionBreak();
#endif

		toolbarLabel("Render");
		if (toolbarToggleButton("Wireframe", *context.wireframe, 96.0f))
		{
			*context.wireframe = !*context.wireframe;
			if (context.applyWireframe)
				context.applyWireframe(*context.wireframe);
		}

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::TextDisabled("Mode: %s | Space: %s | %s",
#ifdef USE_IMGUIZMO
			(context.getGizmoOperation && context.getGizmoOperation() == 0) ? "Move" : ((context.getGizmoOperation && context.getGizmoOperation() == 1) ? "Rotate" : "Scale"),
			(context.getGizmoMode && context.getGizmoMode() == 0) ? "World" : "Local",
#else
			"N/A",
			"N/A",
#endif
			*context.isPlaying ? "Playing" : "Editing");
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::TextDisabled("Tip: Ctrl+Z / Ctrl+Y / Ctrl+P / Ctrl+O");

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}
}
#endif
