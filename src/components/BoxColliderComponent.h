#pragma once

#include <glm/glm.hpp>

struct BoxColliderComponent
{
	// Local-space center offset and half-extents (half-width/height/depth)
	glm::vec3 center = glm::vec3(0.0f);
	glm::vec3 halfExtents = glm::vec3(0.5f);
};
