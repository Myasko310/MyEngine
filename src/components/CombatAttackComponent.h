#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MyEngine
{
	struct CombatAttackDefinition
	{
		std::string name = "LightSlash";
		int startupFrames = 12;
		int activeFrames = 5;
		int recoveryFrames = 20;

		float damage = 10.0f;
		float postureDamage = 8.0f;

		glm::vec3 hitboxCenter = glm::vec3(0.0f, 1.0f, 1.0f);
		float hitboxRadius = 0.45f;

		float cooldownSeconds = 0.15f;
	};

	struct CombatAttackComponent
	{
		// Optional JSON source for authoring attack definitions.
		std::string attackSetPath;
		bool autoLoadFromJson = false;
		bool requestReloadFromJson = false;
		bool loadedFromJson = false;
		std::string lastLoadError;

		// C++ fallback attack definitions used when JSON load is disabled/failed.
		std::vector<CombatAttackDefinition> attacks;

		// Runtime controls
		int selectedAttackIndex = 0;
		bool attackRequested = false;
		bool isAttacking = false;

		enum class AttackPhase : uint8_t
		{
			Idle = 0,
			Startup,
			Active,
			Recovery
		};

		AttackPhase phase = AttackPhase::Idle;
		int phaseFrame = 0;
		int activeAttackIndex = -1;
		float cooldownTimer = 0.0f;

		// Used by CombatHitboxComponent single-hit gating.
		uint32_t attackActivationCounter = 0;
	};
}
