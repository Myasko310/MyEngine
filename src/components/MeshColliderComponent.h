#pragma once

#include <glm/glm.hpp>
#include <array>
#include <string>
#include <vector>

// A static mesh collider built from an explicit triangle list.
// Intended for concave, non-moving geometry (e.g. level terrain, platforms).
// Dynamic bodies with a BoundingSphere or CapsuleCollider will collide against
// each triangle individually using closest-point tests.
//
// Workflow:
//   1. In the Inspector click "Build from Entity Mesh" to extract triangles
//      from the entity's MeshComponent (if any), or manually load via path.
//   2. The resulting triangle list is cached in-memory and saved to the scene
//      file as a flat array of vertex positions.
struct MeshColliderComponent
{
	// Each triangle is stored as three world-space-independent local vertices.
	// The PhysicsSystem will apply the entity's world transform each frame.
	std::vector<std::array<glm::vec3, 3>> triangles;

	// Optional path to the model asset this collider was built from.
	// Used to allow re-baking when the asset changes.
	std::string modelPath;

	// If true, overlaps fire trigger events but no collision response occurs.
	bool isTrigger = false;

	// Cached AABB in local space (recomputed whenever triangles are rebuilt).
	glm::vec3 localAABBMin = glm::vec3(0.0f);
	glm::vec3 localAABBMax = glm::vec3(0.0f);

	// Rebuild the local-space AABB from the current triangle list.
	void RebuildAABB()
	{
		if (triangles.empty())
		{
			localAABBMin = localAABBMax = glm::vec3(0.0f);
			return;
		}
		localAABBMin = glm::vec3(std::numeric_limits<float>::max());
		localAABBMax = glm::vec3(-std::numeric_limits<float>::max());
		for (const auto& tri : triangles)
		{
			for (const auto& v : tri)
			{
				localAABBMin = glm::min(localAABBMin, v);
				localAABBMax = glm::max(localAABBMax, v);
			}
		}
	}
};
