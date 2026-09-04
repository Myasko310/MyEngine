#pragma once

#include <glm/glm.hpp>

namespace MyEngine
{
	struct Ray
	{
		glm::vec3 origin{ 0.0f };
		glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
	};

	// Builds a world-space ray from normalized device mouse coordinates using
	// the current view/projection matrices (standard unprojection).
	Ray ScreenPointToRay(
		double mouseX,
		double mouseY,
		int viewportWidth,
		int viewportHeight,
		const glm::mat4& view,
		const glm::mat4& projection
	);

	// Ray vs. axis-aligned bounding box (world-space center/halfExtents).
	// Returns true and sets outDistance to the nearest positive hit distance.
	bool RayIntersectsAABB(
		const Ray& ray,
		const glm::vec3& center,
		const glm::vec3& halfExtents,
		float& outDistance
	);

	// Ray vs. sphere (world-space center/radius).
	// Returns true and sets outDistance to the nearest positive hit distance.
	bool RayIntersectsSphere(
		const Ray& ray,
		const glm::vec3& center,
		float radius,
		float& outDistance
	);
}
