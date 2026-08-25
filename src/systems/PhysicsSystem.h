#pragma once

#include "ecs/System.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <cstdint>

class Scene;
class Entity;

namespace MyEngine
{
	struct RigidbodyComponent;
	struct CharacterControllerComponent;

	class PhysicsSystem : public System
	{
	public:
		PhysicsSystem();
		~PhysicsSystem() override = default;

		void OnUpdate(Scene& scene, float deltaTime) override;
		void OnUpdate(Scene& scene, float deltaTime, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight);

		// Physics settings
		glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);  // Default gravity (m/s²)
		float fixedTimestep = 0.02f;                         // 50 Hz physics update
		int maxSubsteps = 5;                                 // Prevent spiral of death

		// Debug/stats
		bool enableCollisions = true;
		int collisionChecks = 0;
		int collisionsDetected = 0;

		// Broad-phase settings
		bool enableBroadphase = true;   // Use spatial hash grid to prune pairwise checks
		float broadphaseCellSize = 2.0f;

	public:
		// --- Broad-phase (uniform spatial hash grid) ---
		// Buckets dynamic collider entities into world-space grid cells so the
		// narrow-phase pairwise checks only need to run for entities that
		// share (or neighbor) a cell, instead of every possible pair.
		struct BroadphaseGrid
		{
			float cellSize = 2.0f;

			// Maps a hashed cell coordinate to the list of entities whose AABB
			// overlaps that cell.
			std::unordered_map<long long, std::vector<std::shared_ptr<Entity>>> cells;

			static long long HashCell(int x, int y, int z);
			void Clear();
			void InsertAABB(const std::shared_ptr<Entity>& entity, const glm::vec3& aabbMin, const glm::vec3& aabbMax);

			// Collects unique candidate pairs whose grid cells overlap. The
			// callback is invoked once per unique pair.
			void ForEachCandidatePair(const std::function<void(const std::shared_ptr<Entity>&, const std::shared_ptr<Entity>&)>& callback) const;
		};

	private:
		float accumulator = 0.0f;  // Time accumulator for fixed timestep
		BroadphaseGrid broadphaseGrid;

			// --- Collision/trigger event tracking ---
			// Set of unique entity-ID pairs (packed into a single uint64) that were
			// in physical contact / trigger overlap during the last FixedUpdate.
			// Used to detect Enter (absent -> present) and Exit (present -> absent)
			// transitions so CollisionEventsComponent callbacks fire exactly once.
			std::unordered_set<uint64_t> activeCollisionPairs;
			std::unordered_set<uint64_t> activeTriggerPairs;

			static uint64_t MakePairKey(uint32_t idA, uint32_t idB);
			static void FireEnterEvent(const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b, bool isTrigger);
			static void FireExitEvent(const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b, bool isTrigger);

			// Physics pipeline methods
			void FixedUpdate(Scene& scene, float dt);
			void ApplyForces(Scene& scene, float dt);
			void IntegrateVelocity(Scene& scene, float dt);
			void UpdateCharacterControllers(Scene& scene, float dt);
			void CollectCharacterControllerInput(Scene& scene, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight);
			void DetectAndResolveCollisions(Scene& scene);
			void SolveJoints(Scene& scene, float dt);

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

			bool CheckBoxPlaneCollision(
				const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
				const glm::vec3& planeNormal, float planeDistance,
				glm::vec3& outNormal, float& outPenetration
			);

			bool CheckBoxBoxCollision(
				const glm::vec3& centerA, const glm::vec3& halfExtentsA,
				const glm::vec3& centerB, const glm::vec3& halfExtentsB,
				glm::vec3& outNormal, float& outPenetration
			);

					bool CheckBoxSphereCollision(
						const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
						const glm::vec3& spherePos, float sphereRadius,
						glm::vec3& outNormal, float& outPenetration
					) const;

					// --- Capsule collision detection ---
					// A capsule is defined by a world-space segment [segA, segB] and a radius.

					bool CheckCapsulePlaneCollision(
						const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
						const glm::vec3& planeNormal, float planeDistance,
						glm::vec3& outNormal, float& outPenetration
					) const;

					bool CheckCapsuleSphereCollision(
						const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
						const glm::vec3& spherePos, float sphereRadius,
						glm::vec3& outNormal, float& outPenetration
					) const;

					bool CheckCapsuleBoxCollision(
						const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
						const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
						glm::vec3& outNormal, float& outPenetration
					) const;

					bool CheckCapsuleCapsuleCollision(
						const glm::vec3& segA0, const glm::vec3& segA1, float radiusA,
						const glm::vec3& segB0, const glm::vec3& segB1, float radiusB,
						glm::vec3& outNormal, float& outPenetration
					) const;

					// Closest point between two segments (used by capsule-capsule collision)
					static void ClosestPointsBetweenSegments(
						const glm::vec3& p1, const glm::vec3& q1,
						const glm::vec3& p2, const glm::vec3& q2,
						glm::vec3& outC1, glm::vec3& outC2
					);

					// Closest point on a segment to a given point
					static glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& segA, const glm::vec3& segB);

					// Collision response
					void ResolveSphereCollision(
						glm::vec3& posA, glm::vec3& velA, float massA, float bouncinessA, bool isKinematicA,
						glm::vec3& posB, glm::vec3& velB, float massB, float bouncinessB, bool isKinematicB,
						const glm::vec3& normal, float penetration
					);

					bool SweepCharacterPlanes(const Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const;
					bool SweepCharacterPairs(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const;
					bool QueryCharacterSupport(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const;
					bool ResolveCharacterOverlaps(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, RigidbodyComponent& rb) const;
					bool TryStepUp(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, RigidbodyComponent& rb, const glm::vec3& horizontalDisplacement) const;
					void UpdateControllerAnimationState(const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller) const;
					bool IsWalkableSlope(const glm::vec3& normal, float maxSlopeAngleDegrees) const;
					static glm::vec3 MoveTowards(const glm::vec3& current, const glm::vec3& target, float maxDelta);
					static glm::vec3 FlattenToPlane(const glm::vec3& vector, const glm::vec3& planeNormal);

						private:
							// Computes a world-space AABB for an entity's collider (sphere, box, or capsule).
							bool ComputeColliderAABB(const Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outMin, glm::vec3& outMax) const;
						};
					}
