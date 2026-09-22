#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace MyEngine
{
	struct WorldSphereBounds
	{
		glm::vec3 center;
		float radius;
	};

	inline WorldSphereBounds TransformSphereBounds(const glm::mat4& world, const glm::vec3& center, float radius)
	{
		const glm::mat3 linear(world);
		const glm::mat3 gram = glm::transpose(linear) * linear;
		float maxRowSum = 0.0f;
		for (int row = 0; row < 3; ++row)
			maxRowSum = std::max(maxRowSum, std::abs(gram[0][row]) + std::abs(gram[1][row]) + std::abs(gram[2][row]));

		// This bounds the maximum stretch, including shear from rotated, nonuniformly scaled parents.
		return { glm::vec3(world * glm::vec4(center, 1.0f)), std::max(radius, 0.0f) * std::sqrt(maxRowSum) };
	}
}
