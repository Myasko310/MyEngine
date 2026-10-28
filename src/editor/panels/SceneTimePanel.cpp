#include "SceneTimePanel.h"

#ifdef USE_IMGUI
#include "imgui.h"

#include <algorithm>

namespace MyEngine::Editor::Panels
{
	void DrawSceneTimePanel(SceneTimePanelContext& context)
	{
		if (!context.isOpen || !*context.isOpen || !context.sceneTimeScale)
			return;

		ImGui::SetNextWindowPos(ImVec2(10, 560), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280, 170), ImGuiCond_FirstUseEver);
		ImGui::Begin("Scene Time", context.isOpen);

		if (context.isPlaying)
			ImGui::Text("Session: %s", *context.isPlaying ? "Playing" : "Editing");
		ImGui::Separator();

		float& timeScale = *context.sceneTimeScale;
		timeScale = std::clamp(timeScale, 0.125f, 1.0f);

		if (ImGui::RadioButton("1.0x", timeScale >= 0.875f))
			timeScale = 1.0f;
		ImGui::SameLine();
		if (ImGui::RadioButton("0.5x", timeScale >= 0.375f && timeScale < 0.875f))
			timeScale = 0.5f;
		ImGui::SameLine();
		if (ImGui::RadioButton("0.25x", timeScale >= 0.1875f && timeScale < 0.375f))
			timeScale = 0.25f;
		ImGui::SameLine();
		if (ImGui::RadioButton("0.125x", timeScale < 0.1875f))
			timeScale = 0.125f;

		ImGui::SliderFloat("Scale", &timeScale, 0.125f, 1.0f, "%.3fx");
		if (ImGui::Button("Reset to 1.0x"))
			timeScale = 1.0f;

		ImGui::End();
	}
}
#endif
