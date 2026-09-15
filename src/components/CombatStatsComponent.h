#pragma once

#include <cstdint>

namespace MyEngine
{
	struct CombatStatsComponent
	{
		float maxHealth = 100.0f;
		float health = 100.0f;

		float maxPosture = 100.0f;
		float posture = 0.0f;
		float postureRecoveryPerSecond = 12.0f;

		float guardPostureMultiplier = 0.5f;
		bool guarding = false;

		bool isDead = false;

		enum class HitReactionState : uint8_t
		{
			None = 0,
			Hitstun,
			Staggered,
			KnockedDown
		};

		HitReactionState reactionState = HitReactionState::None;
		float reactionTimer = 0.0f;

		// Runtime diagnostics
		float lastDamageTaken = 0.0f;
		float lastPostureDamageTaken = 0.0f;
		uint32_t lastHitByEntityId = 0;
	};
}
