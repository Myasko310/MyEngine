#pragma once
#include <glm/glm.hpp>

namespace MyEngine
{
	struct EncounterSteering
	{
		float requestedPace = 1.0f;
		float clearanceTimer = 0.0f, clearancePace = 1.0f;
		int clearanceLane = 0;
		glm::vec3 clearanceHeading{0.0f}, steering{0.0f}, avoidance{0.0f};
		float terrainProbeTimer = 0.0f, plannedFlight = 0.0f, jumpInput = 0.0f;
		bool unsafeEdge = false;
		glm::vec3 jumpLanding{0.0f};
	};
}
