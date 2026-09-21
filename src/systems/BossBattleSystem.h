#pragma once

#include "ecs/System.h"
#include <glm/glm.hpp>

class Scene;

namespace MyEngine
{
	// ============================================================
	// BossBattleSystem
	// ============================================================
	// Manages boss behavior, health, attack phases, and
	// special abilities. Integrates with combat and networking.
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
		void UpdateBossPhases(Scene& scene);
		void UpdateBossAttacks(Scene& scene, float deltaTime);
		void UpdateSpecialAbilities(Scene& scene, float deltaTime);
		void TriggerSpecialAbility(Scene& scene, uint32_t bossID, uint32_t abilityType);
	};
}
