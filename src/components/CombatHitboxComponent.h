#pragma once

#include <cstdint>

#include <glm/glm.hpp>

// Data-driven attack collision shape used for melee combat.
// Keep this active only during the attack's active frames.
struct CombatHitboxComponent
{
	// Local-space center offset from the entity transform.
	glm::vec3 center = glm::vec3(0.0f);

	// Radius for simple sphere hit detection.
	float radius = 0.45f;

	// Team/faction id. Matching team IDs do not hit each other.
	int team = 0;

	// Damage payload applied when this hitbox hits a hurtbox.
	float damage = 10.0f;

	// Enable only during active frames.
	bool active = false;

	// If true, this hitbox can only hit each target once per activationId value.
	bool singleHitPerActivation = true;

	// Increment this whenever a new attack window starts.
	// Reusing a value keeps the previous hit memory for each target.
	uint32_t activationId = 0;
};
