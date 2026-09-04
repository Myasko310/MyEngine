#include "ScriptingPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <string>

#include "core/FileDialog.h"
#include "editor/EditorStyle.h"

namespace MyEngine::Editor::Panels
{
	void DrawScriptingPanel(Context& context)
	{
		if (!context.ui || !context.undoStack)
			return;

		auto& ui = *context.ui;
		auto& globalScripts = ui.globalScripts;

		if (!ui.showScriptingPanel)
			return;

		ImGui::SetNextWindowPos(ImVec2(710, 800), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
		ImGui::Begin("Scripting", &ui.showScriptingPanel);

		ImGui::TextWrapped("Global scripts run once per frame (OnGlobalStart / OnGlobalUpdate). Entity scripts still run from ScriptComponent.");
		ImGui::TextDisabled("Global Scripts: %zu", globalScripts.size());
		InspectorGroupLabel("Script List");
		ImGui::SetNextItemWidth(-1.0f);
		static char scriptFilter[64] = "";
		ImGui::InputTextWithHint("##scriptFilter", "Filter by script path or status", scriptFilter, sizeof(scriptFilter));
		auto matchesFilter = [&](const std::string& value)
		{
			if (scriptFilter[0] == '\0')
				return true;
			std::string lhs = value;
			std::string rhs = scriptFilter;
			std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lhs.find(rhs) != std::string::npos;
		};

		for (size_t i = 0; i < globalScripts.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			const auto scriptsBeforeEdit = globalScripts;
			auto& gs = globalScripts[i];
			bool scriptListChanged = false;
			std::string scriptStatus = !gs.enabled ? "disabled" : (gs.requestReload ? "reload pending" : "ready");
			if (!matchesFilter(gs.scriptPath.empty() ? scriptStatus : gs.scriptPath + " " + scriptStatus))
			{
				ImGui::PopID();
				continue;
			}

			if (ImGui::CollapsingHeader(("Global Script " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enabled", &gs.enabled);
				if (ImGui::IsItemDeactivatedAfterEdit()) scriptListChanged = true;
				ImGui::SameLine();
				ImGui::Checkbox("Auto Start", &gs.autoStart);
				if (ImGui::IsItemDeactivatedAfterEdit()) scriptListChanged = true;

				std::string displayPath = gs.scriptPath.empty() ? "(none)" : gs.scriptPath;
				ImGui::TextWrapped("Path: %s", displayPath.c_str());

				const bool hasPath = !gs.scriptPath.empty();
				const char* statusText = !hasPath ? "Missing script path" : (!gs.enabled ? "Disabled" : (gs.requestReload ? "Reload pending" : "Ready"));
				ImVec4 statusColor = !hasPath ? ImVec4(1.00f, 0.60f, 0.20f, 1.0f) : (!gs.enabled ? ImVec4(0.75f, 0.75f, 0.75f, 1.0f) : (gs.requestReload ? ImVec4(1.00f, 0.90f, 0.20f, 1.0f) : ImVec4(0.30f, 1.00f, 0.50f, 1.0f)));
				ImGui::TextColored(statusColor, "Status: %s", statusText);

				if (InspectorActionButton("Browse Lua..."))
				{
					std::string path = MyEngine::FileDialog::OpenScriptFile();
					if (!path.empty())
					{
						gs.scriptPath = path;
						gs.requestReload = true;
						scriptListChanged = true;
					}
				}
				if (InspectorActionButton("Reload##globalscript"))
				{
					gs.requestReload = true;
					scriptListChanged = true;
				}
				if (InspectorActionButton("Clear##globalscript"))
				{
					gs.scriptPath.clear();
					gs.requestReload = true;
					scriptListChanged = true;
				}

				if (globalScripts.size() > 1)
				{
					if (i > 0 && InspectorActionButton("Move Up##globalscript"))
					{
						std::swap(globalScripts[i], globalScripts[i - 1]);
						context.undoStack->Push(std::make_unique<EditorUndo::GlobalScriptsCommand>(scriptsBeforeEdit, globalScripts, &globalScripts));
						ImGui::PopID();
						break;
					}

					if (i + 1 < globalScripts.size() && InspectorActionButton("Move Down##globalscript"))
					{
						std::swap(globalScripts[i], globalScripts[i + 1]);
						context.undoStack->Push(std::make_unique<EditorUndo::GlobalScriptsCommand>(scriptsBeforeEdit, globalScripts, &globalScripts));
						ImGui::PopID();
						break;
					}
				}

				if (InspectorDangerButton("Remove##globalscript"))
				{
					globalScripts.erase(globalScripts.begin() + static_cast<std::ptrdiff_t>(i));
					context.undoStack->Push(std::make_unique<EditorUndo::GlobalScriptsCommand>(scriptsBeforeEdit, globalScripts, &globalScripts));
					ImGui::PopID();
					break;
				}
			}

			if (scriptListChanged)
				context.undoStack->Push(std::make_unique<EditorUndo::GlobalScriptsCommand>(scriptsBeforeEdit, globalScripts, &globalScripts));

			ImGui::PopID();
		}

		if (InspectorActionButton("Add Global Script"))
		{
			const auto scriptsBeforeAdd = globalScripts;
			globalScripts.emplace_back();
			context.undoStack->Push(std::make_unique<EditorUndo::GlobalScriptsCommand>(scriptsBeforeAdd, globalScripts, &globalScripts));
		}

		ImGui::End();
	}
}
#endif
