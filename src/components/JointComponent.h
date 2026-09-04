#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace MyEngine
{
	enum class JointType
	{
		Fixed,   // Rigidly welds the two anchor points together (no relative motion)
		Spring,  // Hooke's-law spring pulling the two anchor points toward restLength apart
		Hinge    // Ball-and-socket style pivot: keeps the owning body within hingeDistance
				 // of the anchor point, allowing free swing (no rotation locking since
				 // RigidbodyComponent has no angular velocity/orientation dynamics).
	};

	struct JointComponent : public Component
	{
		JointType type = JointType::Fixed;

		// ID of the entity this joint connects to. 0 means "connect to a fixed
		// point in world space" (see connectedAnchor below) instead of another body.
		uint32_t connectedEntityID = 0;

		// Anchor offset in the *owning* entity's local space (relative to its
		// TransformComponent::position).
		glm::vec3 anchor = glm::vec3(0.0f);

		// Anchor offset in the connected entity's local space. Ignored (treated as
		// a world-space point) when connectedEntityID == 0.
		glm::vec3 connectedAnchor = glm::vec3(0.0f);

		// --- Spring parameters ---
		float restLength = 1.0f;   // Distance the spring tries to maintain
		float stiffness = 50.0f;   // Spring constant (k)
		float damping = 2.0f;      // Velocity damping along the spring axis

		// --- Hinge parameters ---
		float hingeDistance = 1.0f; // Allowed pivot-arm length from the anchor

		// --- Fixed parameters ---
		float breakForce = 0.0f;   // If > 0, joint is removed once corrective force exceeds this (0 = unbreakable)

		bool enabled = true;
	};
}
