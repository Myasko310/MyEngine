#include "PhysicsPanel.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "systems/PhysicsSystem.h"

namespace MyEngine::Editor::Panels
{
	void DrawPhysicsPanel(PhysicsPanelContext& context)
	{
		if (!context.isOpen || !*context.isOpen || !context.isPlaying || !context.physicsSystem)
			return;

		auto& physicsSystem = *context.physicsSystem;
		ImGui::SetNextWindowPos(ImVec2(10, 700), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Physics", context.isOpen);

		ImGui::Text("Physics System");
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, *context.isPlaying ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::Text(*context.isPlaying ? "RUNNING" : "PAUSED");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button(*context.isPlaying ? "Stop" : "Play") && context.setPlaying)
			context.setPlaying(!*context.isPlaying);
		ImGui::Separator();

		ImGui::DragFloat3("Gravity", &physicsSystem.gravity.x, 0.1f, -50.0f, 50.0f);
		ImGui::DragFloat("Fixed Timestep", &physicsSystem.fixedTimestep, 0.001f, 0.001f, 0.1f, "%.3f");
		ImGui::SliderInt("Max Substeps", &physicsSystem.maxSubsteps, 1, 10);
		ImGui::Checkbox("Enable Collisions", &physicsSystem.enableCollisions);

		ImGui::Separator();
		ImGui::Text("Stats:");
		ImGui::Text("  Collision Checks: %d", physicsSystem.collisionChecks);
		ImGui::Text("  Collisions: %d", physicsSystem.collisionsDetected);

		ImGui::End();
	}
}
#endif
