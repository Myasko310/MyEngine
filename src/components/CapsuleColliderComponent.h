#pragma once

#include <glm/glm.hpp>

// Capsule collider defined by a local-space segment (pointA -> pointB) and a radius.
// For an upright character capsule of total height H and radius R, use:
//   pointA = (0, -(H/2 - R), 0), pointB = (0, (H/2 - R), 0)
struct CapsuleColliderComponent
{
	glm::vec3 pointA = glm::vec3(0.0f, -0.5f, 0.0f);
	glm::vec3 pointB = glm::vec3(0.0f, 0.5f, 0.0f);
	float radius = 0.5f;

	// If true, this collider only reports overlap (OnTriggerEnter/Exit) and does
	// not participate in physical collision response.
	bool isTrigger = false;
};
