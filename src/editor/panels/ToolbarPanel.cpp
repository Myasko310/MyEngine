#include "ToolbarPanel.h"

#ifdef USE_IMGUI
#include <cstdlib>
#include <cstring>
#include "components/ExplorerComponent.h"
#include "components/SparringPartnerComponent.h"
#include "ecs/Scene.h"
#include "editor/EditorStyle.h"
#include "editor/PanelDockManager.h"

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
		constexpr const char* explorerFlag = "MYENGINE_EXPLORER_ENCOUNTER";
		const char* value = std::getenv(explorerFlag);
		const bool explorerEnabled = value && std::strcmp(value, "1") == 0;
		static bool explorerSettingFailed = false;
		ImGui::BeginDisabled(*context.isPlaying);
		if (toolbarToggleButton(explorerEnabled ? "Explorer: On###ExplorerEncounter" : "Explorer: Off###ExplorerEncounter", explorerEnabled))
		{
#ifdef _WIN32
			explorerSettingFailed = _putenv_s(explorerFlag, explorerEnabled ? "" : "1") != 0;
#else
			explorerSettingFailed = (explorerEnabled ? unsetenv(explorerFlag) : setenv(explorerFlag, "1", 1)) != 0;
#endif
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", explorerSettingFailed ? "Could not change the explorer encounter setting." :
				"Spawn a noncombat AI copy of the player on the next Play. Applies only to this editor session.");
		if (context.sparringEnabled)
		{
			ImGui::SameLine();
			ImGui::BeginDisabled(*context.isPlaying);
			if (toolbarToggleButton(*context.sparringEnabled ? "Sparring: On###SparringPartner" : "Sparring: Off###SparringPartner", *context.sparringEnabled))
				*context.sparringEnabled = !*context.sparringEnabled;
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Spawn a spacing partner that evades swings and counterattacks on the next Play.");
			if (*context.isPlaying && *context.sparringEnabled)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("AI decisions")) ImGui::OpenPopup("Sparring decisions");
				if (ImGui::BeginPopup("Sparring decisions"))
				{
					const SparringPartnerComponent* brain = nullptr;
					for (const auto& e : context.scene->GetEntities())
						if (e && e->HasComponent<SparringPartnerComponent>()) { brain = &e->GetComponent<SparringPartnerComponent>(); break; }
					if (brain)
					{
						ImGui::Text("Dodge: %s", brain->dodgeReason);
						ImGui::Text("Counter: %s", brain->counterReason);
						ImGui::Text("Dodge side scores (- / +): %d / %d", brain->laneScore[0], brain->laneScore[1]);
						ImGui::Text("Recent matching swings: %u", brain->patternTime > 0 ? brain->repeatCount : 0u);
						ImGui::Text("Remembered stall locations: %u", static_cast<unsigned>(brain->memory.stalls.size()));
					}
					else ImGui::TextDisabled("No sparring partner in this scene");
					ImGui::EndPopup();
				}
			}
		}
		if (!*context.isPlaying && context.ui && !context.ui->lastSparringDecisions.empty())
		{
			static int cursor = 0;
			ImGui::SameLine();
			if (ImGui::SmallButton("Decision replay")) { cursor = 0; ImGui::OpenPopup("Last sparring session"); }
			if (ImGui::BeginPopup("Last sparring session"))
			{
				const auto& events = context.ui->lastSparringDecisions;
				cursor = std::clamp(cursor, 0, static_cast<int>(events.size()) - 1);
				ImGui::SliderInt("Event", &cursor, 0, static_cast<int>(events.size()) - 1);
				ImGui::TextUnformatted(events[cursor].c_str());
				ImGui::EndPopup();
			}
		}
		if (*context.isPlaying && explorerEnabled)
		{
			for (const auto& entity : context.scene->GetEntities())
			{
				if (!entity || !entity->HasComponent<ExplorerComponent>()) continue;
				auto& ai = entity->GetComponent<ExplorerComponent>();
				ImGui::SameLine();
				if (toolbarToggleButton(ai.following ? "Resume Exploring###ExplorerFollow" : "Follow Me###ExplorerFollow", ai.following))
					ai.SetFollowing(!ai.following);
				break;
			}
		}

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

		toolbarSectionBreak();
		toolbarLabel("UI");
		float uiScale = MyEngine::Editor::GetEditorUiScale();
		ImGui::SetNextItemWidth(105.0f);
		if (ImGui::SliderFloat("##ToolbarUiScale", &uiScale, 0.80f, 1.35f, "%.2fx"))
			MyEngine::Editor::SetEditorUiScale(uiScale);
		ImGui::SameLine();
		bool compactMode = MyEngine::Editor::IsEditorUiCompactMode();
		if (ImGui::Checkbox("Compact", &compactMode))
			MyEngine::Editor::SetEditorUiCompactMode(compactMode);
		if (context.ui)
		{
			ImGui::SameLine();
			if (toolbarToggleButton("HUD", context.ui->showViewportHud, 56.0f))
				context.ui->showViewportHud = !context.ui->showViewportHud;
			ImGui::SameLine();
			if (toolbarToggleButton("Crosshair", context.ui->showViewportHudCrosshair, 84.0f))
				context.ui->showViewportHudCrosshair = !context.ui->showViewportHudCrosshair;
			ImGui::SameLine();
			if (toolbarToggleButton("Dense", context.ui->viewportHudCompact, 64.0f))
				context.ui->viewportHudCompact = !context.ui->viewportHudCompact;
			ImGui::SameLine();
			if (toolbarToggleButton("Icons", context.ui->viewportHudUseIcons, 62.0f))
				context.ui->viewportHudUseIcons = !context.ui->viewportHudUseIcons;
			ImGui::SameLine();
			if (ImGui::Button("HUD Preset", ImVec2(78.0f, 0.0f)))
				context.ui->viewportHudPreset = (context.ui->viewportHudPreset + 1) % 3;
			ImGui::SameLine();
			if (ImGui::Button("HUD Anchor", ImVec2(84.0f, 0.0f)))
				context.ui->viewportHudAnchorPreset = (context.ui->viewportHudAnchorPreset + 1) % 3;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Layout", ImVec2(108.0f, 0.0f)))
			MyEngine::Editor::PanelDockManager::Instance().ResetLayout();

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::TextDisabled("Mode: %s | Space: %s | %s | UI %.2fx%s",
#ifdef USE_IMGUIZMO
			(context.getGizmoOperation && context.getGizmoOperation() == 0) ? "Move" : ((context.getGizmoOperation && context.getGizmoOperation() == 1) ? "Rotate" : "Scale"),
			(context.getGizmoMode && context.getGizmoMode() == 0) ? "World" : "Local",
#else
			"N/A",
			"N/A",
#endif
			*context.isPlaying ? "Playing" : "Editing",
			MyEngine::Editor::GetEditorUiScale(),
			MyEngine::Editor::IsEditorUiCompactMode() ? " (Compact)" : "");
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
