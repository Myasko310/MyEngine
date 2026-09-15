#pragma once

#include <cstdint>

#include <glm/glm.hpp>

// Receives melee hits from CombatHitboxComponent overlaps.
struct CombatHurtboxComponent
{
	// Local-space center offset from the entity transform.
	glm::vec3 center = glm::vec3(0.0f);

	// Radius for simple sphere hit detection.
	float radius = 0.5f;

	// Team/faction id. Matching team IDs do not hit each other.
	int team = 0;

	// If false, ignore incoming hits.
	bool canBeHit = true;

	// Runtime hit results (reset each fixed step by PhysicsSystem).
	bool wasHitThisStep = false;
	float damageTakenThisStep = 0.0f;
	uint32_t lastHitByEntityId = 0;
	uint32_t lastHitActivationId = 0;
};
