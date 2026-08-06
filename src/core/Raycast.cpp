#include "Raycast.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace MyEngine
{
	Ray ScreenPointToRay(
		double mouseX,
		double mouseY,
		int viewportWidth,
		int viewportHeight,
		const glm::mat4& view,
		const glm::mat4& projection
	)
	{
		Ray ray;

		if (viewportWidth <= 0 || viewportHeight <= 0)
			return ray;

		// Convert to normalized device coordinates [-1, 1]
		float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(viewportWidth) - 1.0f;
		float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(viewportHeight);

		glm::mat4 invVP = glm::inverse(projection * view);

		glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
		glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

		nearPoint /= nearPoint.w;
		farPoint /= farPoint.w;

		glm::vec3 origin = glm::vec3(nearPoint);
		glm::vec3 direction = glm::normalize(glm::vec3(farPoint) - glm::vec3(nearPoint));

		ray.origin = origin;
		ray.direction = direction;

		return ray;
	}

	bool RayIntersectsAABB(
		const Ray& ray,
		const glm::vec3& center,
		const glm::vec3& halfExtents,
		float& outDistance
	)
	{
		glm::vec3 minBounds = center - halfExtents;
		glm::vec3 maxBounds = center + halfExtents;

		float tMin = 0.0f;
		float tMax = std::numeric_limits<float>::max();

		for (int axis = 0; axis < 3; ++axis)
		{
			float origin = ray.origin[axis];
			float dir = ray.direction[axis];
			float minB = minBounds[axis];
			float maxB = maxBounds[axis];

			if (std::abs(dir) < 1e-8f)
			{
				// Ray is parallel to this slab; no hit if origin is outside the slab.
				if (origin < minB || origin > maxB)
					return false;
			}
			else
			{
				float invDir = 1.0f / dir;
				float t1 = (minB - origin) * invDir;
				float t2 = (maxB - origin) * invDir;

				if (t1 > t2)
					std::swap(t1, t2);

				tMin = std::max(tMin, t1);
				tMax = std::min(tMax, t2);

				if (tMin > tMax)
					return false;
			}
		}

		outDistance = tMin;
		return true;
	}

	bool RayIntersectsSphere(
		const Ray& ray,
		const glm::vec3& center,
		float radius,
		float& outDistance
	)
	{
		glm::vec3 originToCenter = center - ray.origin;
		float tCenter = glm::dot(originToCenter, ray.direction);

		if (tCenter < 0.0f)
		{
			// Sphere center is behind the ray origin; only a hit if the
			// origin itself is inside the sphere.
			float distSqr = glm::dot(originToCenter, originToCenter);
			if (distSqr > radius * radius)
				return false;

			outDistance = 0.0f;
			return true;
		}

		glm::vec3 closestPoint = ray.origin + ray.direction * tCenter;
		float distSqr = glm::dot(center - closestPoint, center - closestPoint);

		if (distSqr > radius * radius)
			return false;

		float halfChordLength = std::sqrt(radius * radius - distSqr);
		float tHit = tCenter - halfChordLength;

		outDistance = std::max(tHit, 0.0f);
		return true;
	}
}
