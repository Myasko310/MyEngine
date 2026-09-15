#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace MyEngine
{
	struct BossAIComponent
	{
		enum class AIState : uint8_t
		{
			Idle = 0,
			Approach,
			Strafe,
			Attack,
			Punish,
			Recover
		};

		bool enabled = true;
		uint32_t targetEntityID = 0;

		float approachSpeed = 2.8f;
		float strafeSpeed = 1.6f;
		float desiredRange = 2.2f;
		float attackRange = 2.6f;
		float punishRange = 2.8f;

		float decisionInterval = 0.18f;
		float decisionTimer = 0.0f;
		float stateTimer = 0.0f;

		float punishWindowSeconds = 1.2f;
		float punishWindowTimer = 0.0f;

		AIState state = AIState::Idle;

		// Runtime diagnostics for debug overlay.
		glm::vec3 desiredMoveDirection = glm::vec3(0.0f);
		float distanceToTarget = 0.0f;
		float facingDotToTarget = 1.0f;
	};
}
