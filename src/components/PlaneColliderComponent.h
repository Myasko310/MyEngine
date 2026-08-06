#pragma once

#include <glm/glm.hpp>

// Infinite plane collider defined by a point and normal
struct PlaneColliderComponent
{
	glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);  // Plane normal (default: up)
	float distance = 0.0f;  // Distance from origin along normal
};
