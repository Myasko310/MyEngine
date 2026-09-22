#pragma once

#include "ecs/System.h"
#include <glm/glm.hpp>

class Scene;

namespace MyEngine
{
	// ============================================================
	// BossBattleSystem
	// ============================================================
	// Manages boss behavior, health synchronization, telegraph
	// feedback, and special abilities.
	// NOTE: Attack pattern selection and AI state machine are
	// handled by BossBattleAI system (companion system).
	// ============================================================
	class BossBattleSystem : public System
	{
	public:
		BossBattleSystem();
		~BossBattleSystem() override = default;

		void OnUpdate(Scene& scene, float deltaTime) override;

		// Debug settings
		bool enableDebugLogging = false;

	private:
		void UpdateBossHealth(Scene& scene, float deltaTime);
		void ApplyTelegraphFeedback(Scene& scene);
		void UpdateBossAttacks(Scene& scene, float deltaTime);
		void UpdateSpecialAbilities(Scene& scene, float deltaTime);
		void TriggerSpecialAbility(Scene& scene, uint32_t bossID, uint32_t abilityType);
	};
}
