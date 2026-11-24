#include "CombatDebugPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cmath>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "character/CombatShapes.h"
#include "components/BossAIComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"

namespace
{
	bool ProjectWorldPointToScreen(
		const glm::vec3& worldPoint,
		const glm::mat4& view,
		const glm::mat4& projection,
		int windowWidth,
		int windowHeight,
		ImVec2& outScreen)
	{
		glm::vec3 projected = glm::project(
			worldPoint,
			view,
			projection,
			glm::vec4(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)));

		if (projected.z < 0.0f || projected.z > 1.0f)
			return false;

		outScreen = ImVec2(projected.x, static_cast<float>(windowHeight) - projected.y);
		return true;
	}
}

namespace MyEngine::Editor::Panels
{
	void DrawCombatDebugPanel(CombatDebugPanelContext& context)
	{
		if (!context.isOpen || !*context.isOpen || !context.scene || !context.drawCombatShapesOverlay)
			return;

		ImGui::SetNextWindowPos(ImVec2(320, 700), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420, 280), ImGuiCond_FirstUseEver);
		ImGui::Begin("Combat Debug", context.isOpen);

		ImGui::Checkbox("Draw Hitbox/Hurtbox Overlay", context.drawCombatShapesOverlay);

		int attackEntityCount = 0;
		int activeHitboxCount = 0;
		int hurtboxCount = 0;
		int recentlyHitCount = 0;

		for (const auto& entity : context.scene->GetEntities())
		{
			if (!entity)
				continue;

			if (entity->HasComponent<CombatAttackComponent>())
			{
				++attackEntityCount;
			}
			if (entity->HasComponent<::CombatHitboxComponent>() && entity->GetComponent<::CombatHitboxComponent>().active)
				++activeHitboxCount;
			if (entity->HasComponent<::CombatHurtboxComponent>())
			{
				++hurtboxCount;
				if (entity->GetComponent<::CombatHurtboxComponent>().wasHitThisStep)
					++recentlyHitCount;
			}
		}

		ImGui::Text("Attack Entities: %d", attackEntityCount);
		ImGui::Text("Active Hitboxes: %d", activeHitboxCount);
		ImGui::Text("Hurtboxes: %d", hurtboxCount);
		ImGui::Text("Hit This Step: %d", recentlyHitCount);

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Frame Windows", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto& entity : context.scene->GetEntities())
			{
				if (!entity || !entity->HasComponent<CombatAttackComponent>())
					continue;

				const auto& attack = entity->GetComponent<CombatAttackComponent>();
				std::string phaseText = "Idle";
				if (attack.phase == CombatAttackComponent::AttackPhase::Startup) phaseText = "Startup";
				else if (attack.phase == CombatAttackComponent::AttackPhase::Active) phaseText = "Active";
				else if (attack.phase == CombatAttackComponent::AttackPhase::Recovery) phaseText = "Recovery";

				std::string attackName = "<none>";
				if (attack.activeAttackIndex >= 0 && attack.activeAttackIndex < static_cast<int>(attack.attacks.size()))
					attackName = attack.attacks[attack.activeAttackIndex].name;

				ImGui::BulletText("%s | %s | frame %d | %s",
					entity->GetName().c_str(),
					phaseText.c_str(),
					attack.phaseFrame,
					attackName.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Health/Posture & AI", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto& entity : context.scene->GetEntities())
			{
				if (!entity)
					continue;

				if (entity->HasComponent<CombatStatsComponent>())
				{
					const auto& stats = entity->GetComponent<CombatStatsComponent>();
					ImGui::Text("%s HP %.1f/%.1f | Posture %.1f/%.1f | React %.2f",
						entity->GetName().c_str(),
						stats.health, stats.maxHealth,
						stats.posture, stats.maxPosture,
						stats.reactionTimer);
				}

				if (entity->HasComponent<BossAIComponent>())
				{
					const auto& ai = entity->GetComponent<BossAIComponent>();
					ImGui::TextDisabled("  AI %s | dist %.2f | face %.2f | punish %.2f",
						entity->GetName().c_str(), ai.distanceToTarget, ai.facingDotToTarget, ai.punishWindowTimer);
				}
			}
		}

		ImGui::End();

		if (!*context.drawCombatShapesOverlay)
			return;

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const glm::vec3 right = glm::normalize(glm::vec3(glm::inverse(context.viewMatrix)[0]));
		auto drawSphere = [&](const CombatShapes::Sphere& sphere, ImU32 color) {
			ImVec2 center, edge;
			if (ProjectWorldPointToScreen(sphere.center, context.viewMatrix, context.projectionMatrix, context.viewportWidth, context.viewportHeight, center) &&
				ProjectWorldPointToScreen(sphere.center + right * sphere.radius, context.viewMatrix, context.projectionMatrix, context.viewportWidth, context.viewportHeight, edge))
				drawList->AddCircle(center,std::max(2.0f,std::abs(edge.x-center.x)),color,24,2.0f);
		};
		for (const auto& entity : context.scene->GetEntities())
		{
			if (!entity || !entity->HasComponent<::TransformComponent>())
				continue;

			if (entity->HasComponent<::CombatHitboxComponent>())
			{
				const auto& hitbox = entity->GetComponent<::CombatHitboxComponent>();
				if (hitbox.active && hitbox.radius > 0)
				{
					const int slot = entity->HasComponent<CombatAttackComponent>() ? entity->GetComponent<CombatAttackComponent>().activeAttackIndex : -1;
					const auto strikes = CombatShapes::SampleStrikes(*context.scene,*entity,slot,hitbox.center,hitbox.radius);
					for (int i = 0; i < strikes.count; ++i) drawSphere(strikes.spheres[i],IM_COL32(255,80,80,230));
				}
			}

			if (entity->HasComponent<::CombatHurtboxComponent>())
			{
				const auto& hurtbox = entity->GetComponent<::CombatHurtboxComponent>();
				if (hurtbox.canBeHit && hurtbox.radius > 0)
					CombatShapes::ForEachHurtSphere(*context.scene,*entity,hurtbox,[&](int, const CombatShapes::Sphere& sphere) {
						drawSphere(sphere,hurtbox.wasHitThisStep ? IM_COL32(255,220,80,240) : IM_COL32(80,255,120,200));
					});
			}
		}
	}
}
#endif
