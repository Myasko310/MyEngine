#include "PhysicsSystem.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/PlaneColliderComponent.h"

#include <iostream>
#include <algorithm>

namespace MyEngine
{
	PhysicsSystem::PhysicsSystem()
	{
		std::cout << "[PhysicsSystem] Initialized with gravity: ("
				  << gravity.x << ", " << gravity.y << ", " << gravity.z << ")" << std::endl;
	}

	void PhysicsSystem::OnUpdate(Scene& scene, float deltaTime)
	{
		// Fixed timestep physics
		// Clamp deltaTime to prevent spiral of death
		deltaTime = std::min(deltaTime, 0.1f);

		accumulator += deltaTime;

		int substeps = 0;
		while (accumulator >= fixedTimestep && substeps < maxSubsteps)
		{
			FixedUpdate(scene, fixedTimestep);
			accumulator -= fixedTimestep;
			substeps++;
		}
	}

	void PhysicsSystem::FixedUpdate(Scene& scene, float dt)
	{
		// Reset stats
		collisionChecks = 0;
		collisionsDetected = 0;

		// Physics pipeline
		ApplyForces(scene, dt);
		IntegrateVelocity(scene, dt);

		if (enableCollisions)
		{
			DetectAndResolveCollisions(scene);
		}
	}

	void PhysicsSystem::ApplyForces(Scene& scene, float dt)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<RigidbodyComponent>())
				continue;

			auto& rb = entity->GetComponent<RigidbodyComponent>();

			// Skip kinematic bodies
			if (rb.isKinematic)
				continue;

			// Reset acceleration
			rb.acceleration = glm::vec3(0.0f);

			// Apply gravity
			if (rb.useGravity)
			{
				rb.acceleration += gravity * rb.gravityScale;
			}

			// Apply drag (air resistance)
			if (rb.drag > 0.0f)
			{
				glm::vec3 dragForce = -rb.velocity * rb.drag;
				rb.acceleration += dragForce / rb.mass;
			}
		}
	}

	void PhysicsSystem::IntegrateVelocity(Scene& scene, float dt)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<RigidbodyComponent>() || !entity->HasComponent<TransformComponent>())
				continue;

			auto& rb = entity->GetComponent<RigidbodyComponent>();
			auto& transform = entity->GetComponent<TransformComponent>();

			// Skip kinematic bodies
			if (rb.isKinematic)
				continue;

			// Semi-implicit Euler integration
			// v = v + a * dt
			rb.velocity += rb.acceleration * dt;

			// Apply position constraints
			glm::vec3 deltaPosition = rb.velocity * dt;
			if (rb.freezePositionX) deltaPosition.x = 0.0f;
			if (rb.freezePositionY) deltaPosition.y = 0.0f;
			if (rb.freezePositionZ) deltaPosition.z = 0.0f;

			// p = p + v * dt
			transform.position += deltaPosition;
		}
	}

	void PhysicsSystem::DetectAndResolveCollisions(Scene& scene)
	{
		// First, check sphere vs plane collisions
		for (const auto& dynamicEntity : scene.GetEntities())
		{
			if (!dynamicEntity || !dynamicEntity->HasComponent<RigidbodyComponent>() ||
				!dynamicEntity->HasComponent<TransformComponent>() ||
				!dynamicEntity->HasComponent<BoundingSphereComponent>())
				continue;

			auto& rb = dynamicEntity->GetComponent<RigidbodyComponent>();
			auto& transform = dynamicEntity->GetComponent<TransformComponent>();
			auto& bs = dynamicEntity->GetComponent<BoundingSphereComponent>();

			// Skip kinematic objects (they don't collide with planes)
			if (rb.isKinematic)
				continue;

			// Debug: log once when a dynamic rigid body with bounding sphere exists
			static bool loggedDynamic = false;
			if (!loggedDynamic) {
				std::cout << "[Physics] Found dynamic body at Y=" << transform.position.y 
					<< " radius=" << bs.radius 
					<< " useGravity=" << rb.useGravity 
					<< " velocity.y=" << rb.velocity.y << std::endl;
				loggedDynamic = true;
			}

			// Check against all plane colliders
			for (const auto& planeEntity : scene.GetEntities())
			{
				if (!planeEntity || !planeEntity->HasComponent<PlaneColliderComponent>() ||
					!planeEntity->HasComponent<TransformComponent>())
					continue;

				auto& planeTransform = planeEntity->GetComponent<TransformComponent>();
				auto& plane = planeEntity->GetComponent<PlaneColliderComponent>();

				collisionChecks++;

				// Check collision
				glm::vec3 normal;
				float penetration;
				if (CheckSpherePlaneCollision(
					transform.position, bs.radius,
					plane.normal, plane.distance,
					normal, penetration))
				{
					collisionsDetected++;

					// Only apply position correction if there's significant penetration
					// Small threshold prevents constant micro-corrections when resting
					const float penetrationThreshold = 0.005f;
					if (penetration > penetrationThreshold)
					{
						// Move sphere out of plane
						transform.position += normal * penetration;
					}

					// Reflect velocity with bounciness
					float velAlongNormal = glm::dot(rb.velocity, normal);
					if (velAlongNormal < 0.0f)
					{
						rb.velocity -= (1.0f + rb.bounciness) * velAlongNormal * normal;

						// Apply resting contact threshold to prevent micro-bouncing
						// Increased threshold to catch slow-falling objects
						if (glm::length(rb.velocity) < 0.5f && rb.bounciness < 0.01f)
						{
							rb.velocity = glm::vec3(0.0f);
						}
					}
					// If object is resting on ground (minimal velocity), zero out any remaining drift
					else if (std::abs(velAlongNormal) < 0.01f && penetration <= penetrationThreshold)
					{
						// Remove velocity component along normal to prevent drift
						rb.velocity -= velAlongNormal * normal;
					}
				}
			}
		}

		// Then, collect all entities with rigidbodies and bounding spheres for sphere-sphere
		std::vector<std::shared_ptr<Entity>> entities;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity && entity->HasComponent<RigidbodyComponent>() && 
				entity->HasComponent<TransformComponent>() && 
				entity->HasComponent<BoundingSphereComponent>())
			{
				entities.push_back(entity);
			}
		}

		// Broad phase: simple N² check for sphere-sphere
		for (size_t i = 0; i < entities.size(); ++i)
		{
			auto& entityA = entities[i];
			auto& rbA = entityA->GetComponent<RigidbodyComponent>();
			auto& transformA = entityA->GetComponent<TransformComponent>();
			auto& bsA = entityA->GetComponent<BoundingSphereComponent>();

			for (size_t j = i + 1; j < entities.size(); ++j)
			{
				auto& entityB = entities[j];
				auto& rbB = entityB->GetComponent<RigidbodyComponent>();
				auto& transformB = entityB->GetComponent<TransformComponent>();
				auto& bsB = entityB->GetComponent<BoundingSphereComponent>();

				collisionChecks++;

				// Skip if both are kinematic
				if (rbA.isKinematic && rbB.isKinematic)
					continue;

				// Check collision
				glm::vec3 normal;
				float penetration;
				if (CheckSphereSphereCollision(
					transformA.position, bsA.radius,
					transformB.position, bsB.radius,
					normal, penetration))
				{
					collisionsDetected++;

					// Resolve collision
					ResolveSphereCollision(
						transformA.position, rbA.velocity, rbA.mass, rbA.bounciness, rbA.isKinematic,
						transformB.position, rbB.velocity, rbB.mass, rbB.bounciness, rbB.isKinematic,
						normal, penetration
					);
				}
			}
		}
	}

	bool PhysicsSystem::CheckSphereSphereCollision(
		const glm::vec3& posA, float radiusA,
		const glm::vec3& posB, float radiusB,
		glm::vec3& outNormal, float& outPenetration)
	{
		glm::vec3 delta = posB - posA;
		float distSq = glm::dot(delta, delta);
		float radiusSum = radiusA + radiusB;
		float radiusSumSq = radiusSum * radiusSum;

		if (distSq < radiusSumSq)
		{
			float dist = std::sqrt(distSq);

			// Avoid division by zero
			if (dist < 0.0001f)
			{
				outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
				outPenetration = radiusSum;
			}
			else
			{
				outNormal = delta / dist;
				outPenetration = radiusSum - dist;
			}

			return true;
		}

		return false;
	}

	bool PhysicsSystem::CheckSpherePlaneCollision(
		const glm::vec3& spherePos, float sphereRadius,
		const glm::vec3& planeNormal, float planeDistance,
		glm::vec3& outNormal, float& outPenetration)
	{
		// Distance from sphere center to plane
		float distToPlane = glm::dot(spherePos, planeNormal) - planeDistance;

		// Check if sphere intersects plane (use < to avoid triggering when just touching)
		if (distToPlane < sphereRadius)
		{
			outNormal = planeNormal;
			outPenetration = sphereRadius - distToPlane;
			return true;
		}

		return false;
	}

	void PhysicsSystem::ResolveSphereCollision(
		glm::vec3& posA, glm::vec3& velA, float massA, float bouncinessA, bool isKinematicA,
		glm::vec3& posB, glm::vec3& velB, float massB, float bouncinessB, bool isKinematicB,
		const glm::vec3& normal, float penetration)
	{
		// Allow larger penetration for resting contact (slop/tolerance)
		// This is important for stacked objects that settle into each other
		const float penetrationSlop = 0.1f;  // Increased from 0.01f
		float correctionPenetration = std::max(0.0f, penetration - penetrationSlop);

		// Velocity resolution: impulse-based
		// Relative velocity along collision normal
		glm::vec3 relativeVel = velB - velA;
		float velAlongNormal = glm::dot(relativeVel, normal);

		// If objects are separating, don't do anything
		if (velAlongNormal > 0.0f)
			return;

		// If penetration is minor and objects aren't moving fast toward each other, skip everything
		if (correctionPenetration <= 0.0f && velAlongNormal >= -0.5f)
			return;

		// Only apply position correction if penetration is significant
		if (correctionPenetration > 0.0f)
		{
			// Position correction: separate the spheres
			// Kinematic objects don't move during collision resolution
			if (!isKinematicA && !isKinematicB)
			{
				// Both dynamic - split correction based on mass
				float totalMass = massA + massB;
				float ratioA = massB / totalMass;  // Inverted: heavier object moves less
				float ratioB = massA / totalMass;

				posA -= normal * (correctionPenetration * ratioA);
				posB += normal * (correctionPenetration * ratioB);
			}
			else if (!isKinematicA && isKinematicB)
			{
				// Only A is dynamic - move A entirely
				posA -= normal * correctionPenetration;
			}
			else if (isKinematicA && !isKinematicB)
			{
				// Only B is dynamic - move B entirely
				posB += normal * correctionPenetration;
			}
					// If both kinematic, no position correction needed (shouldn't happen due to earlier check)
				}

				// Calculate restitution (bounciness)
				float restitution = std::min(bouncinessA, bouncinessB);

				// Calculate impulse scalar
				// For kinematic objects, treat them as having infinite mass
				float invMassA = isKinematicA ? 0.0f : (1.0f / massA);
				float invMassB = isKinematicB ? 0.0f : (1.0f / massB);

				float impulseMagnitude = -(1.0f + restitution) * velAlongNormal;
				impulseMagnitude /= (invMassA + invMassB);

				// Apply impulse only to dynamic objects
				glm::vec3 impulse = impulseMagnitude * normal;
				if (!isKinematicA)
					velA -= impulse * invMassA;
				if (!isKinematicB)
					velB += impulse * invMassB;

				// Apply resting contact threshold to prevent micro-bouncing between stacked objects
				// If low relative velocity and low bounciness, clamp to zero
				glm::vec3 newRelativeVel = velB - velA;
				float newVelAlongNormal = glm::dot(newRelativeVel, normal);
				if (std::abs(newVelAlongNormal) < 0.5f && restitution < 0.01f)
				{
					// Remove velocity component along collision normal for both objects
					if (!isKinematicA)
						velA += normal * (glm::dot(velA, normal));
					if (!isKinematicB)
						velB -= normal * (glm::dot(velB, normal));
				}
			}
}
