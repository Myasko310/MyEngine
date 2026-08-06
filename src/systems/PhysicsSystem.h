#pragma once

#include "ecs/System.h"
#include <glm/glm.hpp>

class Scene;

namespace MyEngine
{
	class PhysicsSystem : public System
	{
	public:
		PhysicsSystem();
		~PhysicsSystem() override = default;

		void OnUpdate(Scene& scene, float deltaTime) override;

		// Physics settings
		glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);  // Default gravity (m/s²)
		float fixedTimestep = 0.02f;                         // 50 Hz physics update
		int maxSubsteps = 5;                                 // Prevent spiral of death

		// Debug/stats
		bool enableCollisions = true;
		int collisionChecks = 0;
		int collisionsDetected = 0;

	private:
		float accumulator = 0.0f;  // Time accumulator for fixed timestep

			// Physics pipeline methods
			void FixedUpdate(Scene& scene, float dt);
			void ApplyForces(Scene& scene, float dt);
			void IntegrateVelocity(Scene& scene, float dt);
			void DetectAndResolveCollisions(Scene& scene);

			// Collision detection
			bool CheckSphereSphereCollision(
				const glm::vec3& posA, float radiusA,
				const glm::vec3& posB, float radiusB,
				glm::vec3& outNormal, float& outPenetration
			);

			bool CheckSpherePlaneCollision(
				const glm::vec3& spherePos, float sphereRadius,
				const glm::vec3& planeNormal, float planeDistance,
				glm::vec3& outNormal, float& outPenetration
			);

			// Collision response
			void ResolveSphereCollision(
				glm::vec3& posA, glm::vec3& velA, float massA, float bouncinessA, bool isKinematicA,
				glm::vec3& posB, glm::vec3& velB, float massB, float bouncinessB, bool isKinematicB,
				const glm::vec3& normal, float penetration
			);
		};
}
