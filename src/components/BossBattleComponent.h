#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace MyEngine
{
	// ============================================================
	// BossBattleComponent
	// ============================================================
	// Marks an entity as a boss and tracks battle state,
	// phase transitions, and special attacks.
	// ============================================================
	struct BossBattleComponent
	{
		// Boss identity
		uint32_t bossID = 0;
		std::string bossName = "Boss";

		// Health and state
		uint32_t maxHealth = 500;
		uint32_t currentHealth = 500;
		uint32_t phase = 1; // 1, 2, 3 (harder each phase)

		// Battle mechanics
		float attackCooldown = 2.0f;
		float attackCooldownRemaining = 0.0f;
		bool isAttacking = false;

		// Phase thresholds
		float phaseTransitionHealth1 = 0.66f; // Switch to phase 2 at 66% health
		float phaseTransitionHealth2 = 0.33f; // Switch to phase 3 at 33% health

		// Special ability
		float specialAbilityCooldown = 10.0f;
		float specialAbilityCooldownRemaining = 0.0f;
		uint32_t specialAbilityType = 0; // 0=none, 1=summon, 2=areaattack, 3=heal

		// Networked state
		bool isDead = false;
		uint32_t playerCount = 0;
	};
}
