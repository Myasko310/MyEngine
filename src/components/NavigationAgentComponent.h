#pragma once

#include <glm/glm.hpp>
#include <vector>

struct NavigationAgentComponent
{
	// World-space destination the agent should walk toward.
	glm::vec3 targetPosition{ 0.0f };

	// Whether the agent is actively navigating.
	bool active = false;

	// Movement speed (world units per second).
	float speed = 5.0f;

	// How close to the target (and intermediate waypoints) counts as "arrived".
	float stoppingDistance = 0.25f;

	// Current computed waypoint path (filled by NavMeshSystem::FindPath).
	// Index 0 is the next waypoint; back() is the destination.
	std::vector<glm::vec3> path;

	// Index of the currently pursued waypoint in `path`.
	int waypointIndex = 0;

	// Set to true when the agent has reached `targetPosition`.
	bool arrived = true;
};
