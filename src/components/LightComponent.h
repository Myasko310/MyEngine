#pragma once

#include <glm/glm.hpp>

namespace MyEngine
{
	struct LightComponent
	{
		enum class Type
		{
			Directional,
			Point,
			Spot
		} type = Type::Directional;

		// Common
		glm::vec3 color = glm::vec3(1.0f);
		float intensity = 1.0f;

		// Directional
		glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);

		// Point / Spot
		glm::vec3 position = glm::vec3(0.0f);
		float range = 10.0f;

		// Spot specific (degrees)
		float innerCone = 12.5f;
		float outerCone = 17.5f;

		// Shadow quality
		float shadowBias = 0.005f;
		bool castShadows = false;

		// Optional per-light point shadow overrides (0 / negative = use global)
		int pointShadowSizeOverride = 0;
		int pointShadowPCFSamplesOverride = 0;
		float pointShadowPCFRadiusOverride = -1.0f;

		// Optional per-light spot shadow overrides (0 / negative = use global)
		int spotShadowSizeOverride = 0;
		float spotShadowPCFRadiusOverride = -1.0f;
	};
}
