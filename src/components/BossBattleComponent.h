#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>

namespace MyEngine
{
	// ============================================================
	// BossBattleComponent
	// ============================================================
	// Marks an entity as a boss and tracks battle state,
	// phase transitions, attack patterns, and AI state.
	// ============================================================
	struct BossBattleComponent
	{
		// ========== Boss Identity ==========
		uint32_t bossID = 0;
		std::string bossName = "Boss";

		// ========== Health & Phase ==========
		uint32_t maxHealth = 500;
		uint32_t currentHealth = 500;
		uint32_t phase = 1; // 1, 2, 3 (harder each phase)

		// Phase thresholds
		float phaseTransitionHealth1 = 0.66f; // Switch to phase 2 at 66% health
		float phaseTransitionHealth2 = 0.33f; // Switch to phase 3 at 33% health

		// ========== Attack Pattern System ==========
		// Current attack pattern being used
		uint32_t currentAttackPatternID = 0;
		std::string currentAttackPatternName;

		// Telegraph and timing
		float telegraphTimer = 0.0f;          // Time remaining in telegraph phase
		float telegraphDuration = 0.0f;       // Total telegraph duration for current attack
		bool isInTelegraphPhase = false;      // Currently telegraphing an attack?

		// Attack execution
		float attackTimer = 0.0f;             // Time remaining for attack execution
		float attackDuration = 0.0f;          // Total duration of current attack
		bool isExecutingAttack = false;       // Currently executing an attack?
		bool hasAttackHitThisFrame = false;   // Did this attack hit player this frame? (prevent multi-hit)

		// Cooldown
		float attackCooldownRemaining = 0.0f; // Time before next attack
		bool isOnCooldown = false;

		// ========== AI Mental State ==========
		enum class AIState : uint8_t
		{
			Idle = 0,           // Waiting for attack opportunity
			Telegraphing = 1,   // Showing attack tell
			Attacking = 2,      // Executing attack
			OnCooldown = 3,     // Recovering after attack
			HitStun = 4,        // Stunned from player hit
			PhaseTransition = 5 // Transitioning between phases
		};

		AIState aiState = AIState::Idle;
		AIState previousAIState = AIState::Idle;

		// ========== Combat Context ==========
		// Last recorded player distance (for attack selection)
		float lastPlayerDistance = 10.0f;

		// Phase transition state
		bool isTransitioningPhase = false;
		float phaseTransitionTimer = 0.0f;

		// ========== Special Abilities (legacy) ==========
		float specialAbilityCooldown = 10.0f;
		float specialAbilityCooldownRemaining = 0.0f;
		uint32_t specialAbilityType = 0; // 0=none, 1=summon, 2=areaattack, 3=heal

		// ========== Networked State ==========
		bool isDead = false;
		uint32_t playerCount = 0;

		// Debug
		bool enableDebugLogging = false;
	};
}
