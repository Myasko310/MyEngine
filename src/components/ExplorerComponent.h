#pragma once
#include "ai/EncounterMemory.h"
#include "components/EncounterSteering.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstddef>
#include <cstdint>

class NavMeshSystem;

namespace MyEngine
{
	struct ExplorerComponent : EncounterSteering
	{
		EncounterMemory memory;
		glm::vec3 origin{0.0f};
		glm::vec3 checkpoint{0.0f};
		unsigned waypoint = 0;
		std::uint32_t playerId = 0;
		std::uint32_t roamSeed = 192837u;
		glm::vec3 goal{0.0f};
		std::vector<glm::vec3> recentGoals;
		bool hasGoal = false;
		bool revisitingLandmark = false;
		unsigned landmarkRetries = 0;
		bool following = false;
		bool waiting = false;
		int followSide = 1;
		std::uint32_t ferry = 0;
		float ferryTimer = 0.0f;
		bool riding = false;
		glm::vec3 lookDirection{0.0f};
		const NavMeshSystem* navigation = nullptr;
		std::vector<glm::vec3> route;
		std::size_t nextNode = 0;
		float replanDelay = 0.0f;
		float probeDelay = 0.0f;
		unsigned blockedChecks = 0;
		float checkTimer = 0.0f;
		float legTime = 0.0f;
		std::vector<std::uint32_t> recentInterests;
		std::uint32_t interest = 0, entrance = 0, exit = 0;
		bool crossing = false;
		float scanDelay = 0.0f, inspectTime = 0.0f;
		glm::vec3 progressTarget{0.0f};
		float bestDistance = -1.0f, noProgress = 0.0f;
		float escapeTime = 0.0f;
		glm::vec3 escapeDirection{0.0f};
		unsigned recoveries = 0;

		void SetFollowing(bool enabled)
		{
			following = enabled;
			revisitingLandmark = false;
			followSide = 1;
			clearanceTimer = 0.0f;
			ferry = 0; ferryTimer = 0.0f; riding = false;
			interest = entrance = exit = 0;
			crossing = false;
			scanDelay = inspectTime = noProgress = escapeTime = 0.0f;
			bestDistance = -1.0f;
			waiting = hasGoal = false;
			lookDirection = glm::vec3(0.0f);
			avoidance = steering = glm::vec3(0.0f);
			unsafeEdge = false;
			plannedFlight = 0.0f;
			terrainProbeTimer = 0.0f;
			route.clear();
			nextNode = blockedChecks = 0;
			replanDelay = checkTimer = legTime = probeDelay = 0.0f;
		}
	};
}
