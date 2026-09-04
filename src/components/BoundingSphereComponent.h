#pragma once

#include <glm/glm.hpp>

struct BoundingSphereComponent
{
	// Local-space center and radius
	glm::vec3 center = glm::vec3(0.0f);
	float radius = 1.0f;

	// If true, this collider only reports overlap (OnTriggerEnter/Exit) and does
	// not participate in physical collision response.
	bool isTrigger = false;
};
