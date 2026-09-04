#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>

namespace MyEngine
{
	struct RigidbodyComponent : public Component
	{
		// Linear motion
		glm::vec3 velocity = glm::vec3(0.0f);
		glm::vec3 acceleration = glm::vec3(0.0f);

		// Physical properties
		float mass = 1.0f;
		float drag = 0.01f;           // Air resistance
		float bounciness = 0.0f;      // Coefficient of restitution (0 = no bounce, 1 = perfect bounce)

		// Gravity settings
		bool useGravity = true;
		float gravityScale = 1.0f;    // Multiplier for gravity effect

		// Constraints
		bool isKinematic = false;     // If true, not affected by forces (but still collides)
		bool freezePositionX = false;
		bool freezePositionY = false;
		bool freezePositionZ = false;

		// Continuous Collision Detection: when true the integrator sub-steps this
		// body's linear sweep each physics tick, preventing tunnel-through against
		// plane colliders at high speeds.
		bool useCCD = false;

		RigidbodyComponent() = default;

		RigidbodyComponent(float m, bool gravity = true)
			: mass(m), useGravity(gravity)
		{}
	};
}
