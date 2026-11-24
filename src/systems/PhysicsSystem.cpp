#include "PhysicsSystem.h"
#include "character/CombatShapes.h"
#include "character/SparringEnergyBurst.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/MeshColliderComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/ExplorerComponent.h"
#include "components/SparringPartnerComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "components/JointComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/TerrainComponent.h"
#include "core/CollisionMatrix.h"
#include "core/Input.h"
#include "core/InputActions.h"
#include "core/PerformanceMetrics.h"
#include "systems/TerrainSystem.h"
#include "systems/NavMeshSystem.h"
#include "network/NetTypes.h"

#include <iostream>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <string>
#include <unordered_set>
#include <glm/gtx/component_wise.hpp>

namespace MyEngine
{
	static glm::vec3 ExtractWorldScale(const Scene& scene, const Entity& entity)
	{
		glm::mat4 world = TransformHierarchy::GetWorldMatrix(scene, entity);
		return glm::vec3(
			glm::length(glm::vec3(world[0])),
			glm::length(glm::vec3(world[1])),
			glm::length(glm::vec3(world[2]))
		);
	}

	PhysicsSystem::PhysicsSystem()
	{
		std::cout << "[PhysicsSystem] Initialized with gravity: ("
				  << gravity.x << ", " << gravity.y << ", " << gravity.z << ")" << std::endl;
	}

	void PhysicsSystem::OnUpdate(Scene& scene, float deltaTime)
	{
		OnUpdate(scene, deltaTime, nullptr, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), nullptr);
	}

	void PhysicsSystem::OnUpdate(Scene& scene, float deltaTime, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight, const Net::InputCommand* authoritativeInput)
	{
		vaultAttemptedThisFrame = false;
		vaultActiveThisFrame = false;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity && entity->HasComponent<CharacterControllerComponent>() && entity->GetComponent<CharacterControllerComponent>().isVaulting)
			{
				vaultActiveThisFrame = true;
				break;
			}
		}
		CollectCharacterControllerInput(scene, window, cameraForward, cameraRight, authoritativeInput);

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
		UpdateCharacterControllers(scene, dt);
		SolveJoints(scene, dt);

		if (enableCollisions)
		{
			DetectAndResolveCollisions(scene);
		}

		ProcessCombatHitboxes(scene);
		UpdateSparringBursts(scene,dt);
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

			if (rb.useCCD)
			{
				// CCD: sub-step this body's sweep against plane colliders to prevent
				// tunnel-through at high velocities.
				SweepCCDBody(scene, entity, dt);
			}
			else
			{
				// Apply position constraints
				glm::vec3 deltaPosition = rb.velocity * dt;
				if (rb.freezePositionX) deltaPosition.x = 0.0f;
				if (rb.freezePositionY) deltaPosition.y = 0.0f;
				if (rb.freezePositionZ) deltaPosition.z = 0.0f;

				// p = p + v * dt
				transform.position += deltaPosition;
			}
		}
	}

	void PhysicsSystem::SweepCCDBody(Scene& scene, const std::shared_ptr<Entity>& entity, float dt)
	{
		auto& rb        = entity->GetComponent<RigidbodyComponent>();
		auto& transform = entity->GetComponent<TransformComponent>();

		// Determine the sphere radius to sweep (prefer BoundingSphere, else use 0.1 fallback).
		float sweepRadius = 0.1f;
		glm::vec3 centerOffset(0.0f);
		if (entity->HasComponent<BoundingSphereComponent>())
		{
			auto& bs   = entity->GetComponent<BoundingSphereComponent>();
			sweepRadius = bs.radius;
			centerOffset = bs.center;
		}
		else if (entity->HasComponent<CapsuleColliderComponent>())
		{
			auto& cap  = entity->GetComponent<CapsuleColliderComponent>();
			sweepRadius = cap.radius;
			centerOffset = (cap.pointA + cap.pointB) * 0.5f;
		}
		else if (entity->HasComponent<BoxColliderComponent>())
		{
			auto& box  = entity->GetComponent<BoxColliderComponent>();
			sweepRadius = glm::length(box.halfExtents);
			centerOffset = box.center;
		}

		// Break the motion into sub-steps of at most (sweepRadius * 0.5) each.
		glm::vec3 totalDisplacement = rb.velocity * dt;
		if (rb.freezePositionX) totalDisplacement.x = 0.0f;
		if (rb.freezePositionY) totalDisplacement.y = 0.0f;
		if (rb.freezePositionZ) totalDisplacement.z = 0.0f;

		float totalDist = glm::length(totalDisplacement);
		if (totalDist < 1e-6f)
		{
			transform.position += totalDisplacement;
			return;
		}

		const float subStepSize = std::max(sweepRadius * 0.5f, 0.001f);
		const int   numSubSteps = static_cast<int>(std::ceil(totalDist / subStepSize));
		glm::vec3   stepDelta   = totalDisplacement / static_cast<float>(numSubSteps);

		for (int s = 0; s < numSubSteps; ++s)
		{
			transform.position += stepDelta;
			glm::vec3 sphereCenter = transform.position + centerOffset;

			// Check against all plane colliders and push out immediately.
			for (const auto& other : scene.GetEntities())
			{
				if (!other || !other->HasComponent<PlaneColliderComponent>())
					continue;

				auto& plane = other->GetComponent<PlaneColliderComponent>();
				if (plane.isTrigger)
					continue;

				glm::vec3 normal;
				float penetration;
				if (CheckSpherePlaneCollision(sphereCenter, sweepRadius, plane.normal, plane.distance, normal, penetration))
				{
					transform.position += normal * penetration;
					// Kill velocity component along normal
					float vDotN = glm::dot(rb.velocity, normal);
					if (vDotN < 0.0f)
						rb.velocity -= normal * vDotN * (1.0f + rb.bounciness);

					// Recompute sphere center after push-out for subsequent sub-steps.
					sphereCenter = transform.position + centerOffset;
					// Recompute stepDelta based on remaining velocity.
					const int remaining = numSubSteps - s - 1;
					if (remaining > 0)
					{
						glm::vec3 remainDisp = rb.velocity * (dt * static_cast<float>(remaining) / static_cast<float>(numSubSteps));
						if (rb.freezePositionX) remainDisp.x = 0.0f;
						if (rb.freezePositionY) remainDisp.y = 0.0f;
						if (rb.freezePositionZ) remainDisp.z = 0.0f;
						stepDelta = remainDisp / static_cast<float>(remaining);
					}
				}
			}
		}
	}

	void PhysicsSystem::CollectCharacterControllerInput(Scene& scene, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight, const Net::InputCommand* authoritativeInput)
	{
		(void)window;

		glm::vec3 flattenedForward = FlattenToPlane(cameraForward, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec3 flattenedRight = FlattenToPlane(cameraRight, glm::vec3(0.0f, 1.0f, 0.0f));
		if (glm::length(flattenedForward) > 0.0001f)
			flattenedForward = glm::normalize(flattenedForward);
		else
			flattenedForward = glm::vec3(0.0f, 0.0f, -1.0f);
		if (glm::length(flattenedRight) > 0.0001f)
			flattenedRight = glm::normalize(flattenedRight);
		else
			flattenedRight = glm::vec3(1.0f, 0.0f, 0.0f);

		float moveForward = 0.0f;
		float moveRight = 0.0f;
		bool jumpDown = false;
		bool jumpPressed = false;
		bool attack1Pressed = false;
		bool attack2Pressed = false;
		bool attack3Pressed = false;
		bool genericAttackPressed = false;
		bool dodgePressed = false;
		bool vaultPressed = false;
		bool wallRunPressed = false;

		if (authoritativeInput)
		{
			moveForward = authoritativeInput->moveAxis.y;
			moveRight = authoritativeInput->moveAxis.x;
			jumpDown = authoritativeInput->jumpPressed;
			jumpPressed = authoritativeInput->jumpPressed;
			dodgePressed = authoritativeInput->dodgePressed;
			vaultPressed = authoritativeInput->vaultPressed;
			wallRunPressed = authoritativeInput->wallRunPressed;
		}
		else
		{
			moveForward = InputActions::GetAxis("MoveForward");
			moveRight = InputActions::GetAxis("MoveRight");
			jumpDown = InputActions::IsAction("Jump");
			jumpPressed = InputActions::IsActionPressed("Jump");
			dodgePressed = InputActions::IsActionPressed("Dodge");
			vaultPressed = InputActions::IsActionPressed("Vault");
		}

		// Attack inputs are polled locally for now so combat remains responsive in
		// both offline and authoritative-network play modes.
		attack1Pressed = InputActions::IsActionPressed("Attack1");
		attack2Pressed = InputActions::IsActionPressed("Attack2");
		attack3Pressed = InputActions::IsActionPressed("Attack3");
		genericAttackPressed = InputActions::IsActionPressed("Attack") || InputActions::IsActionPressed("Fight");

		glm::vec3 moveInput = flattenedForward * moveForward + flattenedRight * moveRight;
		if (glm::length(moveInput) > 1.0f)
			moveInput = glm::normalize(moveInput);

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<CharacterControllerComponent>())
				continue;

			auto& controller = entity->GetComponent<CharacterControllerComponent>();
			if (entity->HasComponent<ExplorerComponent>() || entity->HasComponent<SparringPartnerComponent>())
				continue;
			controller.moveInput = moveInput;
			controller.jumpHeld = jumpDown;
			// Latch jump input until the next fixed-step character update consumes it.
			controller.jumpRequested = controller.jumpRequested || jumpPressed;
			const bool newMovementCommand = !authoritativeInput || !controller.hasMovementInputTick ||
				controller.lastMovementInputTick != authoritativeInput->tick;
			const bool sprintPressed = newMovementCommand && (authoritativeInput
				? authoritativeInput->sprintPressed : InputActions::IsActionPressed("Sprint"));
			const bool slidePressed = newMovementCommand && (authoritativeInput
				? authoritativeInput->slidePressed : InputActions::IsActionPressed("Crouch"));
			controller.sprintRequested |= sprintPressed;
			controller.slideRequested |= slidePressed;
			if (slidePressed)
				controller.slideInputBufferTimer = std::max(controller.slideInputBufferTimer,
					std::max(controller.slideInputBufferDuration, 0.0f));
			controller.dodgeRequested |= newMovementCommand && dodgePressed;
			controller.wallRunRequested = authoritativeInput ? wallRunPressed : InputActions::IsAction("WallRun");
			controller.vaultRequested |= newMovementCommand && vaultPressed;
			controller.crouchHeld = authoritativeInput ? authoritativeInput->crouchHeld : InputActions::IsAction("Crouch");
			controller.hasMovementInputTick = authoritativeInput != nullptr;
			if (authoritativeInput)
				controller.lastMovementInputTick = authoritativeInput->tick;

			bool queueAttack1 = attack1Pressed;
			bool queueAttack2 = attack2Pressed;
			bool queueAttack3 = attack3Pressed;

			// Treat the generic Attack/Fight action as an Attack1 input only.
			// Attack2 and Attack3 combos must be extended with their own inputs,
			// so Attack1 or generic inputs no longer re-arm those branches.
			if (!authoritativeInput && genericAttackPressed && !attack1Pressed && !attack2Pressed && !attack3Pressed)
			{
				std::string currentStateName;
				if (entity->HasComponent<AnimationStateMachineComponent>())
				{
					auto& sm = entity->GetComponent<AnimationStateMachineComponent>();
					if (sm.stateMachine && sm.stateMachine->IsValidStateIndex(sm.currentStateIndex))
						currentStateName = sm.stateMachine->states[sm.currentStateIndex].name;
				}

				std::transform(currentStateName.begin(), currentStateName.end(), currentStateName.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

				const bool inAttack2Branch = currentStateName.find("attack2_") != std::string::npos;
				const bool inAttack3Branch = currentStateName.find("attack3_") != std::string::npos;
				if (!inAttack2Branch && !inAttack3Branch)
					queueAttack1 = true;
			}

			// Latch attack input edges until consumed by animation parameter update.
			controller.attack1Requested = controller.attack1Requested || queueAttack1;
			controller.attack2Requested = controller.attack2Requested || queueAttack2;
			controller.attack3Requested = controller.attack3Requested || queueAttack3;
		}
	}

	glm::vec3 PhysicsSystem::MoveTowards(const glm::vec3& current, const glm::vec3& target, float maxDelta)
	{
		glm::vec3 delta = target - current;
		float distance = glm::length(delta);
		if (distance <= maxDelta || distance < 0.0001f)
			return target;
		return current + delta / distance * maxDelta;
	}

	glm::vec3 PhysicsSystem::FlattenToPlane(const glm::vec3& vector, const glm::vec3& planeNormal)
	{
		glm::vec3 normal = glm::length(planeNormal) > 0.0001f
			? glm::normalize(planeNormal)
			: glm::vec3(0.0f, 1.0f, 0.0f);
		return vector - glm::dot(vector, normal) * normal;
	}

	bool PhysicsSystem::IsWalkableSlope(const glm::vec3& normal, float maxSlopeAngleDegrees) const
	{
		if (glm::length(normal) < 0.0001f)
			return false;
		float upDot = glm::dot(glm::normalize(normal), glm::vec3(0.0f, 1.0f, 0.0f));
		float minUpDot = std::cos(glm::radians(std::clamp(maxSlopeAngleDegrees, 0.0f, 89.0f)));
		return upDot >= minUpDot;
	}

	bool PhysicsSystem::SweepCharacterPlanes(const Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const
	{
		if (!entity || !entity->HasComponent<TransformComponent>() || !entity->HasComponent<CapsuleColliderComponent>())
			return false;

		auto& transform = entity->GetComponent<TransformComponent>();
		auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		// Character-controller capsule is authored in world units.
		// Do not scale by visual transform scale.
		const float radius = std::max(capsule.radius, 0.001f);
		glm::vec3 pointA = capsule.pointA;
		glm::vec3 pointB = capsule.pointB;
		if (pointB.y < pointA.y)
			std::swap(pointA, pointB);
		if (glm::length(pointB - pointA) < 0.0001f)
			pointB.y = pointA.y + std::max(radius * 0.5f, 0.05f);
		if (entity->HasComponent<CharacterControllerComponent>())
		{
			const auto& controller = entity->GetComponent<CharacterControllerComponent>();
			pointA.y += std::max(controller.capsuleBottomAnimationLift, 0.0f);
		}
		glm::vec3 segA = transform.position + pointA;
		glm::vec3 segB = transform.position + pointB;

		bool foundHit = false;
		outPenetration = 0.0f;
		for (const auto& other : scene.GetEntities())
		{
			if (!other || other.get() == entity.get() || !other->HasComponent<PlaneColliderComponent>())
				continue;

			const auto& plane = other->GetComponent<PlaneColliderComponent>();
			if (plane.isTrigger)
				continue;

			glm::vec3 normal;
			float penetration = 0.0f;
			if (!CheckCapsulePlaneCollision(segA, segB, radius, plane.normal, plane.distance, normal, penetration))
				continue;

			if (!foundHit || penetration > outPenetration ||
				(std::abs(penetration - outPenetration) < 0.0001f && glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f)) > glm::dot(outNormal, glm::vec3(0.0f, 1.0f, 0.0f))))
			{
				foundHit = true;
				outNormal = normal;
				outPenetration = penetration;
			}
		}

		return foundHit;
	}

	PhysicsSystem::VaultQueryContext PhysicsSystem::BuildVaultQueryContext(Scene& scene, const std::shared_ptr<Entity>& entity, const glm::vec3& minimumPosition, const glm::vec3& maximumPosition) const
	{
		const auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		const float radius = std::max(capsule.radius, 0.001f);
		glm::vec3 a = capsule.pointA, b = capsule.pointB;
		a.y = std::max(a.y, radius);
		if (b.y <= a.y) b.y = a.y + std::max(radius, 0.1f);
		const glm::vec3 minimum = minimumPosition + glm::min(a, b) - glm::vec3(radius + 0.05f);
		const glm::vec3 maximum = maximumPosition + glm::max(a, b) + glm::vec3(radius + 0.05f);
		auto overlaps = [&](const glm::vec3& lo, const glm::vec3& hi)
		{
			return glm::all(glm::lessThanEqual(lo, maximum)) && glm::all(glm::greaterThanEqual(hi, minimum));
		};
		VaultQueryContext query;
		query.minimumPosition = minimumPosition;
		query.maximumPosition = maximumPosition;
		for (const auto& other : scene.GetEntities())
		{
			if (!other || other == entity || !other->HasComponent<TransformComponent>()) continue;
			if (!other->HasComponent<BoxColliderComponent>() && !other->HasComponent<CapsuleColliderComponent>() &&
				!other->HasComponent<BoundingSphereComponent>() && !other->HasComponent<MeshColliderComponent>()) continue;
			VaultCollider candidate{ other, ExtractWorldScale(scene, *other), {} };
			const auto& position = other->GetComponent<TransformComponent>().position;
			glm::vec3 lo, hi;
			// Match the narrow-phase shape precedence and world-unit conventions exactly.
			if (other->HasComponent<BoxColliderComponent>())
			{
				const auto& box = other->GetComponent<BoxColliderComponent>();
				if (box.isTrigger) continue;
				const glm::vec3 center = position + box.center * candidate.scale;
				lo = center - box.halfExtents * candidate.scale;
				hi = center + box.halfExtents * candidate.scale;
			}
			else if (other->HasComponent<CapsuleColliderComponent>())
			{
				const auto& shape = other->GetComponent<CapsuleColliderComponent>();
				if (shape.isTrigger) continue;
				const glm::vec3 extent(shape.radius * glm::compMax(glm::vec2(candidate.scale.x, candidate.scale.z)));
				lo = position + glm::min(shape.pointA * candidate.scale, shape.pointB * candidate.scale) - extent;
				hi = position + glm::max(shape.pointA * candidate.scale, shape.pointB * candidate.scale) + extent;
			}
			else if (other->HasComponent<BoundingSphereComponent>())
			{
				const auto& shape = other->GetComponent<BoundingSphereComponent>();
				if (shape.isTrigger) continue;
				const glm::vec3 center = position + shape.center * candidate.scale;
				const glm::vec3 extent(shape.radius * glm::compMax(candidate.scale));
				lo = center - extent;
				hi = center + extent;
			}
			else
			{
				const auto& mesh = other->GetComponent<MeshColliderComponent>();
				if (mesh.isTrigger) continue;
				const glm::mat4 world = TransformHierarchy::GetWorldMatrix(scene, *other);
				for (const auto& triangle : mesh.triangles)
				{
					std::array<glm::vec3, 3> vertices = { glm::vec3(world * glm::vec4(triangle[0], 1)),
						glm::vec3(world * glm::vec4(triangle[1], 1)), glm::vec3(world * glm::vec4(triangle[2], 1)) };
					if (overlaps(glm::min(vertices[0], glm::min(vertices[1], vertices[2])), glm::max(vertices[0], glm::max(vertices[1], vertices[2]))))
						candidate.triangles.push_back(vertices);
				}
				if (!candidate.triangles.empty()) query.colliders.push_back(std::move(candidate));
				continue;
			}
			if (overlaps(lo, hi)) query.colliders.push_back(std::move(candidate));
		}
		return query;
	}

	bool PhysicsSystem::SweepCharacterPairs(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration, glm::vec3* outSupportVelocity, const VaultQueryContext* vaultQuery) const
	{
		if (!entity || !entity->HasComponent<TransformComponent>() || !entity->HasComponent<CapsuleColliderComponent>())
			return false;

		auto& transform = entity->GetComponent<TransformComponent>();
		auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		const float radius = std::max(capsule.radius, 0.001f);
		glm::vec3 pointA = capsule.pointA;
		glm::vec3 pointB = capsule.pointB;
		if (pointB.y < pointA.y)
			std::swap(pointA, pointB);
		if (glm::length(pointB - pointA) < 0.0001f)
			pointB.y = pointA.y + std::max(radius * 0.5f, 0.05f);
		if (entity->HasComponent<CharacterControllerComponent>())
		{
			const auto& controller = entity->GetComponent<CharacterControllerComponent>();
			pointA.y += std::max(controller.capsuleBottomAnimationLift, 0.0f);
		}
		glm::vec3 segA = transform.position + pointA;
		glm::vec3 segB = transform.position + pointB;

		bool foundHit = false;
		outPenetration = 0.0f;
		glm::vec3 bestSupportVelocity(0.0f);
		// Shared contact ranking logic (also suitable for future vault contact checks).
		auto considerContact = [&](const glm::vec3& normal, float penetration, const glm::vec3& supportVelocity)
		{
			if (!foundHit || penetration > outPenetration ||
				(std::abs(penetration - outPenetration) < 0.0001f && glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f)) > glm::dot(outNormal, glm::vec3(0.0f, 1.0f, 0.0f))))
			{
				foundHit = true;
				outNormal = normal;
				outPenetration = penetration;
				bestSupportVelocity = supportVelocity;
			}
		};
		const size_t count = vaultQuery ? vaultQuery->colliders.size() : scene.GetEntities().size();
		for (size_t index = 0; index < count; ++index)
		{
			const auto& other = vaultQuery ? vaultQuery->colliders[index].entity : scene.GetEntities()[index];
			if (!other || other.get() == entity.get() || !other->HasComponent<TransformComponent>())
				continue;

			glm::vec3 otherScale = vaultQuery ? vaultQuery->colliders[index].scale : ExtractWorldScale(scene, *other);
			glm::vec3 normal;
			float penetration = 0.0f;
			bool hit = false;

			if (other->HasComponent<BoxColliderComponent>())
			{
				const auto& box = other->GetComponent<BoxColliderComponent>();
				const auto& otherTransform = other->GetComponent<TransformComponent>();
				if (box.isTrigger)
					continue;
				hit = CheckCapsuleBoxCollision(
					segA, segB, radius,
					otherTransform.position + box.center * otherScale,
					box.halfExtents * otherScale,
					normal, penetration);
			}
			else if (other->HasComponent<CapsuleColliderComponent>())
			{
				const auto& otherCapsule = other->GetComponent<CapsuleColliderComponent>();
				if (otherCapsule.isTrigger)
					continue;
				const bool character = other->HasComponent<CharacterControllerComponent>();
				const auto scale = character ? glm::vec3(1.0f) : otherScale;
				const auto origin = other->GetComponent<TransformComponent>().position;
				auto a = origin + otherCapsule.pointA * scale, b = origin + otherCapsule.pointB * scale;
				if (a.y > b.y) std::swap(a, b);
				if (character) a.y += std::max(other->GetComponent<CharacterControllerComponent>().capsuleBottomAnimationLift, 0.0f);
				const float r = otherCapsule.radius * std::max(scale.x, scale.z);
				hit = CheckCapsuleCapsuleCollision(segA, segB, radius, a, b, r, normal, penetration);
				if (hit)
				{
					// The pair solver returns A-to-B; controller depenetration moves A outward.
					normal = -normal;
					if (character && entity->HasComponent<CharacterControllerComponent>())
					{
						glm::vec3 onSelf, onOther;
						ClosestPointsBetweenSegments(segA, segB, a, b, onSelf, onOther);
						auto side = onSelf - onOther;
						const float dy = side.y;
						side.y = 0;
						const float distance = glm::length(side), reach = radius + r;
						// Preserve vertical motion even for stacked endcaps. Coincident axes
						// need opposite, stable lateral normals in the two query orders.
						normal = distance > 0.0001f ? side / distance : glm::vec3(entity->GetID() < other->GetID() ? -1.0f : 1.0f, 0, 0);
						penetration = std::max(0.0f, std::sqrt(std::max(0.0f, reach * reach - dy * dy)) - distance);
					}
				}
			}
			else if (other->HasComponent<BoundingSphereComponent>())
			{
				const auto& sphere = other->GetComponent<BoundingSphereComponent>();
				if (sphere.isTrigger)
					continue;
				hit = CheckCapsuleSphereCollision(
					segA, segB, radius,
					other->GetComponent<TransformComponent>().position + sphere.center * otherScale,
					sphere.radius * glm::compMax(otherScale),
					normal, penetration);
			}
			else if (other->HasComponent<MeshColliderComponent>())
			{
				const auto& mesh = other->GetComponent<MeshColliderComponent>();
				if (mesh.isTrigger)
					continue;
				const glm::mat4 world = vaultQuery ? glm::mat4(1.0f) : TransformHierarchy::GetWorldMatrix(scene, *other);
				const auto& triangles = vaultQuery ? vaultQuery->colliders[index].triangles : mesh.triangles;
				for (const auto& triangle : triangles)
				{
					glm::vec3 candidateNormal;
					float candidatePenetration = 0.0f;
					if (CheckCapsuleMeshCollision(segA, segB, radius,
						vaultQuery ? triangle[0] : glm::vec3(world * glm::vec4(triangle[0], 1.0f)),
						vaultQuery ? triangle[1] : glm::vec3(world * glm::vec4(triangle[1], 1.0f)),
						vaultQuery ? triangle[2] : glm::vec3(world * glm::vec4(triangle[2], 1.0f)), candidateNormal, candidatePenetration) &&
						(!hit || candidatePenetration > penetration))
					{
						hit = true;
						normal = candidateNormal;
						penetration = candidatePenetration;
					}
				}
			}

			if (!hit)
				continue;

			glm::vec3 supportVelocity(0.0f);
			if (other->HasComponent<RigidbodyComponent>())
				supportVelocity = other->GetComponent<RigidbodyComponent>().velocity;
			considerContact(normal, penetration, supportVelocity);
		}

		if (outSupportVelocity)
			*outSupportVelocity = bestSupportVelocity;
		return foundHit;
	}

	bool PhysicsSystem::QueryCharacterSupport(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration, glm::vec3* outSupportVelocity, const VaultQueryContext* vaultQuery) const
	{
		glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);
		float planePenetration = 0.0f;
		bool hitPlane = SweepCharacterPlanes(scene, entity, planeNormal, planePenetration);

		glm::vec3 pairNormal(0.0f, 1.0f, 0.0f);
		float pairPenetration = 0.0f;
		glm::vec3 pairSupportVelocity(0.0f);
		bool hitPair = SweepCharacterPairs(scene, entity, pairNormal, pairPenetration, &pairSupportVelocity, vaultQuery);

		glm::vec3 terrainNormal(0.0f, 1.0f, 0.0f);
		float terrainPenetration = 0.0f;
		bool hitTerrain = false;
		if (entity && entity->HasComponent<TransformComponent>() && entity->HasComponent<CapsuleColliderComponent>())
		{
			const auto& transform = entity->GetComponent<TransformComponent>();
			const auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
			const float radius = std::max(capsule.radius, 0.001f);
			const float supportWorldX = transform.position.x;
			const float supportWorldZ = transform.position.z;
			float supportFootY = transform.position.y + std::min(capsule.pointA.y, capsule.pointB.y) - radius;
			if (entity->HasComponent<CharacterControllerComponent>())
				supportFootY += std::max(entity->GetComponent<CharacterControllerComponent>().capsuleBottomAnimationLift, 0.0f);

			for (const auto& other : scene.GetEntities())
			{
				if (!other || !other->HasComponent<TerrainComponent>())
					continue;

				const auto& terrain = other->GetComponent<TerrainComponent>();
				if (terrain.heightData.empty() || terrain.resolution < 2)
					continue;

				glm::vec3 terrainWorldPos(0.0f);
				if (other->HasComponent<TransformComponent>())
					terrainWorldPos = other->GetComponent<TransformComponent>().position;

				if (std::abs(supportWorldX - terrainWorldPos.x) > terrain.width * 0.5f ||
					std::abs(supportWorldZ - terrainWorldPos.z) > terrain.depth * 0.5f)
					continue;
				const float sampledHeight = TerrainSystem::SampleHeight(terrain, supportWorldX, supportWorldZ, terrainWorldPos);
				const float penetration = sampledHeight - supportFootY;
				if (penetration < -0.05f)
					continue;

				glm::vec3 sampledNormal = TerrainSystem::SampleNormal(terrain, supportWorldX, supportWorldZ, terrainWorldPos);
				if (!hitTerrain || penetration > terrainPenetration ||
					(std::abs(penetration - terrainPenetration) < 0.0001f && glm::dot(sampledNormal, glm::vec3(0.0f, 1.0f, 0.0f)) > glm::dot(terrainNormal, glm::vec3(0.0f, 1.0f, 0.0f))))
				{
					hitTerrain = true;
					terrainNormal = sampledNormal;
					terrainPenetration = penetration;
				}
			}
		}

		if (!hitPlane && !hitPair && !hitTerrain)
			return false;

		glm::vec3 chosenSupportVelocity(0.0f);
		outNormal = planeNormal;
		outPenetration = planePenetration;

		auto preferCandidate = [&](const glm::vec3& normal, float penetration)
		{
			return (penetration > outPenetration) ||
				(std::abs(penetration - outPenetration) < 0.0001f &&
					glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f)) > glm::dot(outNormal, glm::vec3(0.0f, 1.0f, 0.0f)));
		};

		if (!hitPlane)
		{
			if (hitPair)
			{
				outNormal = pairNormal;
				outPenetration = pairPenetration;
				chosenSupportVelocity = pairSupportVelocity;
			}
			else
			{
				outNormal = terrainNormal;
				outPenetration = terrainPenetration;
			}
		}
		else
		{
			if (hitPair && preferCandidate(pairNormal, pairPenetration))
			{
				outNormal = pairNormal;
				outPenetration = pairPenetration;
				chosenSupportVelocity = pairSupportVelocity;
			}
			if (hitTerrain && preferCandidate(terrainNormal, terrainPenetration))
			{
				outNormal = terrainNormal;
				outPenetration = terrainPenetration;
				chosenSupportVelocity = glm::vec3(0.0f);
			}
		}

		if (!hitPlane && hitPair && hitTerrain && preferCandidate(terrainNormal, terrainPenetration))
		{
			outNormal = terrainNormal;
			outPenetration = terrainPenetration;
			chosenSupportVelocity = glm::vec3(0.0f);
		}

		if (outSupportVelocity)
			*outSupportVelocity = chosenSupportVelocity;
		return true;
	}

	bool PhysicsSystem::ResolveCharacterOverlaps(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, RigidbodyComponent& rb) const
	{
		auto& transform = entity->GetComponent<TransformComponent>();
		bool resolvedAny = false;
		for (int iteration = 0; iteration < 4; ++iteration)
		{
			glm::vec3 hitNormal(0.0f, 1.0f, 0.0f);
			float hitPenetration = 0.0f;
			if (!QueryCharacterSupport(scene, entity, hitNormal, hitPenetration))
				break;

			if (hitPenetration <= 0.001f)
				continue;

			resolvedAny = true;
			transform.position += hitNormal * (hitPenetration + controller.skinWidth);
			float intoSurface = glm::dot(rb.velocity, hitNormal);
			if (intoSurface < 0.0f)
				rb.velocity -= intoSurface * hitNormal;

			if (!IsWalkableSlope(hitNormal, controller.maxSlopeAngleDegrees))
			{
				controller.isOnSteepSlope = true;
			}
		}

		return resolvedAny;
	}

	bool PhysicsSystem::TryStepUp(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, RigidbodyComponent& rb, const glm::vec3& horizontalDisplacement) const
	{
		if (!controller.wasGrounded || controller.maxStepHeight <= 0.0001f || glm::length(horizontalDisplacement) <= 0.0001f)
			return false;

		auto& transform = entity->GetComponent<TransformComponent>();
		glm::vec3 originalPosition = transform.position;
		glm::vec3 originalGroundNormal = controller.groundNormal;
		bool originalGrounded = controller.isGrounded;
		bool originalSteep = controller.isOnSteepSlope;
		float originalVerticalVelocity = rb.velocity.y;

		transform.position += glm::vec3(0.0f, controller.maxStepHeight + controller.skinWidth, 0.0f);
		transform.position += horizontalDisplacement;
		ResolveCharacterOverlaps(scene, entity, controller, rb);
		transform.position -= glm::vec3(0.0f, controller.maxStepHeight + controller.groundSnapDistance + controller.skinWidth, 0.0f);

		glm::vec3 supportNormal(0.0f, 1.0f, 0.0f);
		float supportPenetration = 0.0f;
		if (QueryCharacterSupport(scene, entity, supportNormal, supportPenetration) && IsWalkableSlope(supportNormal, controller.maxSlopeAngleDegrees))
		{
			transform.position += supportNormal * (supportPenetration + controller.skinWidth);
			controller.isGrounded = true;
			controller.groundNormal = glm::normalize(supportNormal);
			controller.isOnSteepSlope = false;
			if (rb.velocity.y < 0.0f)
				rb.velocity.y = 0.0f;
			return true;
		}

		transform.position = originalPosition;
		controller.groundNormal = originalGroundNormal;
		controller.isGrounded = originalGrounded;
		controller.isOnSteepSlope = originalSteep;
		rb.velocity.y = originalVerticalVelocity;
		return false;
	}

	bool PhysicsSystem::QueryVaultContact(Scene& scene, const std::shared_ptr<Entity>& entity, const glm::vec3& position, glm::vec3& normal, float& penetration, const VaultQueryContext* vaultQuery) const
	{
		if (vaultQuery && (glm::any(glm::lessThan(position, vaultQuery->minimumPosition)) ||
			glm::any(glm::greaterThan(position, vaultQuery->maximumPosition))))
			vaultQuery = nullptr;
		auto& transform = entity->GetComponent<TransformComponent>();
		const glm::vec3 savedPosition = transform.position;
		transform.position = position;
		const bool hit = QueryCharacterSupport(scene, entity, normal, penetration, nullptr, vaultQuery);
		transform.position = savedPosition;
		return hit;
	}

	bool PhysicsSystem::FindVaultLanding(Scene& scene, const std::shared_ptr<Entity>& entity, const CharacterControllerComponent& controller, glm::vec3& position, float minimumY, const VaultQueryContext* vaultQuery) const
	{
		const float step = std::max(entity->GetComponent<CapsuleColliderComponent>().radius * 0.2f, 0.001f);
		const float startY = position.y;
		const int samples = static_cast<int>(std::ceil((startY - minimumY) / step));
		if (samples < 0 || samples > 4096)
			return false;
		for (int i = 0; i <= samples; ++i)
		{
			position.y = std::max(minimumY, startY - i * step);
			glm::vec3 normal;
			float penetration = 0.0f;
			if (!QueryVaultContact(scene, entity, position, normal, penetration, vaultQuery) || penetration < 0.0f)
				continue;
			if (!IsWalkableSlope(normal, controller.maxSlopeAngleDegrees))
				return false;
			position += normal * (penetration + std::max(controller.skinWidth, 0.005f));
			return !QueryVaultContact(scene, entity, position, normal, penetration, vaultQuery) || penetration <= 0.001f;
		}
		return false;
	}

	glm::vec3 PhysicsSystem::SampleVaultPosition(const CharacterControllerComponent& controller, float t)
	{
		if (controller.vaultPath.empty())
			return controller.vaultStartPosition;
		const float distance = std::clamp(t, 0.0f, 1.0f) * controller.vaultPath.back().w;
		const auto next = std::lower_bound(controller.vaultPath.begin(), controller.vaultPath.end(), distance,
			[](const glm::vec4& point, float value) { return point.w < value; });
		if (next == controller.vaultPath.begin())
			return glm::vec3(*next);
		if (next == controller.vaultPath.end())
			return controller.vaultEndPosition;
		const auto& previous = *(next - 1);
		return glm::mix(glm::vec3(previous), glm::vec3(*next),
			(distance - previous.w) / std::max(next->w - previous.w, 0.000001f));
	}

	glm::vec3 PhysicsSystem::GetCharacterRenderPosition(const Entity& entity) const
	{
		const auto& position = entity.GetComponent<TransformComponent>().position;
		if (!entity.HasComponent<CharacterControllerComponent>())
			return position;
		const auto& controller = entity.GetComponent<CharacterControllerComponent>();
		if (!controller.hasPhysicsPosition || fixedTimestep <= 0.0f)
			return position;
		return glm::mix(controller.previousPhysicsPosition, position, std::clamp(accumulator / fixedTimestep, 0.0f, 1.0f));
	}

	bool PhysicsSystem::TryBeginVault(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, const glm::vec3& moveInput) const
	{
		if (!controller.enableVault || !controller.enableSprintSlide || !controller.sprintLatched || controller.crouchHeld ||
			controller.isVaulting || !controller.vaultRequested || !controller.isGrounded ||
			controller.jumpRequested || controller.vaultCooldownTimer > 0.0f || controller.vaultDuration <= 0.0f || controller.vaultForwardDistance <= 0.0f)
			return false;
		if (!entity || !entity->HasComponent<TransformComponent>() || !entity->HasComponent<CapsuleColliderComponent>())
			return false;

		glm::vec3 forward = FlattenToPlane(moveInput, glm::vec3(0.0f, 1.0f, 0.0f));
		if (glm::length(forward) < 0.1f)
			return false;
		vaultAttemptedThisFrame = true;
		PROFILE_SCOPE("Vault activation");
		forward = glm::normalize(forward);

		auto& transform = entity->GetComponent<TransformComponent>();
		const auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		const float radius = std::max(capsule.radius, 0.001f);
		const float footOffset = std::max(capsule.pointA.y, radius) - radius;
		const float footY = transform.position.y + footOffset;
		const float margin = std::max(controller.skinWidth, 0.005f) + 0.02f;
		float nearest = controller.vaultForwardDistance;
		float farEdge = 0.0f;
		float bestTopY = 0.0f;
		bool foundObstacle = false;
		for (const auto& other : scene.GetEntities())
		{
			if (!other || other.get() == entity.get() || !other->HasComponent<BoxColliderComponent>() || !other->HasComponent<TransformComponent>())
				continue;
			const auto& box = other->GetComponent<BoxColliderComponent>();
			if (box.isTrigger)
				continue;

			glm::vec3 boundsMin, boundsMax;
			if (!ComputeColliderAABB(scene, other, boundsMin, boundsMax))
				continue;
			if (boundsMax.y - footY < controller.vaultMinHeight)
				continue;
			float entry = 0.0f;
			float exit = std::numeric_limits<float>::max();
			for (int axis : { 0, 2 })
			{
				const float lower = boundsMin[axis] - radius - margin;
				const float upper = boundsMax[axis] + radius + margin;
				if (std::abs(forward[axis]) < 0.0001f)
				{
					if (transform.position[axis] < lower || transform.position[axis] > upper)
						exit = -1.0f;
					continue;
				}
				const float a = (lower - transform.position[axis]) / forward[axis];
				const float b = (upper - transform.position[axis]) / forward[axis];
				entry = std::max(entry, std::min(a, b));
				exit = std::min(exit, std::max(a, b));
			}
			if (entry <= exit && exit > 0.0f && entry <= nearest)
			{
				foundObstacle = true;
				nearest = entry;
				farEdge = exit;
				bestTopY = boundsMax.y;
			}
		}

		// Bound the traversal separately from the approach probe; do not vault across entire buildings.
		if (!foundObstacle || bestTopY - footY > controller.vaultMaxHeight ||
			farEdge > std::max(controller.vaultForwardDistance * 4.0f, radius * 4.0f))
			return false;

		CharacterControllerComponent path = controller;
		path.vaultStartPosition = transform.position;
		const float obstacleHeight = std::max(bestTopY - footY, 0.0f);
		const float overheadArcScale = std::max(controller.vaultOverheadArcScale, 0.0f);
		const float overheadMarginScale = std::max(controller.vaultOverheadMarginScale, 0.0f);
		const float archArcScale = std::max(controller.vaultArchArcScale, 0.0f);
		const float archHeightScale = std::max(controller.vaultArchHeightScale, 0.0f);
		const float minimumApexBoost = std::max(controller.vaultArcHeight * 0.25f, 0.1f);
		const float overhead = std::max(controller.vaultArcHeight * overheadArcScale, margin * overheadMarginScale);
		path.vaultHeight = obstacleHeight + std::max(overhead, minimumApexBoost);
		const float sampleStep = std::max(radius * 0.2f, 0.001f);
		const float obstacleDepth = std::max(farEdge - nearest, 0.0f);
		const float preferredRunout = std::max(std::max(radius * 3.5f, path.vaultHeight * 1.4f), obstacleDepth * 0.75f);
		const float archRise = std::max(controller.vaultArcHeight * archArcScale, path.vaultHeight * archHeightScale);
		const glm::vec3 raisedStart = path.vaultStartPosition + glm::vec3(0.0f, path.vaultHeight, 0.0f);
		const glm::vec3 furthest = raisedStart + forward * (farEdge + preferredRunout);
		glm::vec3 minimum = glm::min(path.vaultStartPosition, furthest) - glm::vec3(0.5f);
		minimum.y = transform.position.y - controller.vaultMaxHeight - 0.5f;
		const auto query = BuildVaultQueryContext(scene, entity, minimum,
			glm::max(path.vaultStartPosition, furthest) + glm::vec3(0.5f, archRise + 0.5f, 0.5f));
		auto prepareRoute = [&](float runout)
		{
			path.vaultEndPosition = raisedStart + forward * (farEdge + runout);
			if (!FindVaultLanding(scene, entity, controller, path.vaultEndPosition, transform.position.y - controller.vaultMaxHeight, &query))
				return false;
			path.vaultPath.clear();
			path.vaultPath.reserve(129);
			auto appendPoint = [&](const glm::vec3& point)
			{
				if (!path.vaultPath.empty() && glm::length(point - glm::vec3(path.vaultPath.back())) < 0.000001f)
					return;
				const float distance = path.vaultPath.empty() ? 0.0f :
					path.vaultPath.back().w + glm::length(point - glm::vec3(path.vaultPath.back()));
				path.vaultPath.emplace_back(point, distance);
			};
			auto appendCurve = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)
			{
				for (int i = 0; i <= 32; ++i)
				{
					const float u = static_cast<float>(i) / 32;
					const float v = 1.0f - u;
					appendPoint(v * v * v * a + 3.0f * v * v * u * b + 3.0f * v * u * u * c + u * u * u * d);
				}
			};
			// Replace the level traverse with a parabola. Match its slopes at both
			// joins so the character keeps rising/falling instead of leveling off.
			const float approach = nearest + margin * 0.5f;
			const float archWidth = std::max(farEdge - approach, 0.001f);
			const float archSlope = 4.0f * archRise / archWidth;
			const glm::vec3 liftEnd = raisedStart + forward * approach;
			const float liftHandle = std::min(approach * 0.5f, path.vaultHeight * 0.5f / archSlope);
			appendCurve(path.vaultStartPosition, path.vaultStartPosition + forward * (approach * 0.5f),
				liftEnd - forward * liftHandle - glm::vec3(0, archSlope * liftHandle, 0), liftEnd);
			for (int i = 1; i <= 64; ++i)
			{
				const float u = static_cast<float>(i) / 64;
				appendPoint(liftEnd + forward * (archWidth * u) + glm::vec3(0, 4.0f * archRise * u * (1.0f - u), 0));
			}
			const glm::vec3 landStart = raisedStart + forward * farEdge;
			const float descent = std::max(landStart.y - path.vaultEndPosition.y, 0.0f);
			const float landHandle = std::min(runout * 0.5f, descent * 0.5f / archSlope);
			appendCurve(landStart, landStart + forward * landHandle - glm::vec3(0, archSlope * landHandle, 0),
				path.vaultEndPosition - forward * (runout * 0.5f), path.vaultEndPosition);
			const int samples = std::max(1, static_cast<int>(std::ceil(path.vaultPath.back().w / sampleStep)));
			if (samples > 4096) return false;
			for (int i = 0; i <= samples; ++i)
			{
				glm::vec3 normal;
				float penetration = 0.0f;
				if (QueryVaultContact(scene, entity, SampleVaultPosition(path, static_cast<float>(i) / samples), normal, penetration, &query) && penetration > 0.001f)
					return false;
			}
			return true;
		};
		// Keep short supported landings usable when the preferred runout is obstructed.
		if (!prepareRoute(preferredRunout) && !prepareRoute(margin)) return false;
		controller.vaultStartPosition = path.vaultStartPosition;
		controller.vaultEndPosition = path.vaultEndPosition;
		controller.vaultHeight = path.vaultHeight;
		controller.vaultPath = std::move(path.vaultPath);
		const float approachSpeed = glm::dot(entity->GetComponent<RigidbodyComponent>().velocity, forward);
		const float baseLocomotionSpeed = controller.moveSpeed *
			((controller.enableSprintSlide && controller.sprintLatched)
				? std::max(controller.sprintMultiplier, 1.0f)
				: 1.0f);
		const float targetVaultSpeed = std::max(baseLocomotionSpeed * 1.18f, approachSpeed < 0.1f ? controller.moveSpeed * 1.75f : approachSpeed * 1.18f);
		controller.vaultMotionDuration = std::max(controller.vaultDuration * 0.85f,
			controller.vaultPath.back().w / std::max(targetVaultSpeed, 1.0f));
		controller.isVaulting = true;
		controller.vaultTimer = controller.vaultMotionDuration;
		controller.vaultElapsed = 0.0f;
		controller.isSliding = false;
		controller.isDodging = false;
		controller.isSprinting = false;
		controller.slideTimer = 0.0f;
		controller.dodgeTimer = 0.0f;
		return true;
	}

	void PhysicsSystem::UpdateVaultMotion(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, RigidbodyComponent& rb, float dt) const
	{
		if (!controller.isVaulting || dt <= 0.0f)
			return;
		vaultActiveThisFrame = true;
		PROFILE_SCOPE("Vault fixed step");
		auto& transform = entity->GetComponent<TransformComponent>();
		const float duration = std::max(controller.vaultMotionDuration, 0.0001f);
		const glm::vec3 previousPosition = transform.position;
		glm::vec3 minimum = previousPosition, maximum = previousPosition;
		for (const auto& point : controller.vaultPath)
		{
			minimum = glm::min(minimum, glm::vec3(point));
			maximum = glm::max(maximum, glm::vec3(point));
		}
		// Rebuilt every fixed step: new/moved blockers must never use stale bounds.
		const auto query = BuildVaultQueryContext(scene, entity, minimum - glm::vec3(std::max(controller.skinWidth, 0.005f) + 0.01f), maximum);
		const float previousT = controller.vaultElapsed / duration;
		controller.vaultElapsed = std::min(controller.vaultElapsed + dt, duration);
		controller.vaultTimer = std::max(0.0f, duration - controller.vaultElapsed);
		const float t = std::clamp(controller.vaultElapsed / duration, 0.0f, 1.0f);
		const float distanceBound = controller.vaultPath.empty() ? 0.0f : controller.vaultPath.back().w;
		const float sampleStep = std::max(entity->GetComponent<CapsuleColliderComponent>().radius * 0.2f, 0.001f);
		const int samples = std::max(1, static_cast<int>(std::ceil(distanceBound * (t - previousT) / sampleStep)));
		bool blocked = false;
		for (int i = 1; i <= samples; ++i)
		{
			const glm::vec3 candidate = SampleVaultPosition(controller, glm::mix(previousT, t, static_cast<float>(i) / samples));
			glm::vec3 normal;
			float penetration = 0.0f;
			if (QueryVaultContact(scene, entity, candidate, normal, penetration, &query) && penetration > 0.001f)
			{
				blocked = true;
				break;
			}
			transform.position = candidate;
		}
		rb.velocity = (transform.position - previousPosition) / dt;
		if (blocked || t >= 1.0f)
		{
			controller.isVaulting = false;
			controller.vaultTimer = 0.0f;
			controller.vaultCooldownTimer = std::max(controller.vaultCooldown, 0.0f);
			rb.velocity = glm::vec3(0.0f);
			glm::vec3 normal;
			float penetration = 0.0f;
			controller.isGrounded = QueryVaultContact(scene, entity,
				transform.position - glm::vec3(0.0f, std::max(controller.skinWidth, 0.005f) + 0.002f, 0.0f), normal, penetration, &query) &&
				IsWalkableSlope(normal, controller.maxSlopeAngleDegrees);
			if (controller.isGrounded)
				controller.groundNormal = normal;
			if (!blocked && controller.isGrounded)
			{
				glm::vec3 exitDirection = FlattenToPlane(controller.vaultEndPosition - controller.vaultStartPosition, glm::vec3(0, 1, 0));
				if (glm::length(exitDirection) > 0.0001f)
				{
					exitDirection = glm::normalize(exitDirection);
					controller.isSprinting = controller.enableSprintSlide && controller.sprintLatched && !controller.crouchHeld && glm::length(controller.moveInput) >= 0.1f;
					// Hand off the path velocity; ordinary controller acceleration/braking
					// brings it back to the requested locomotion speed on subsequent steps.
					const float exitSpeed = distanceBound / duration;
					rb.velocity = exitDirection * exitSpeed * std::clamp(glm::dot(controller.moveInput, exitDirection), 0.0f, 1.0f);
				}
			}
		}
	}

	void PhysicsSystem::UpdateControllerAnimationState(const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller) const
	{
		if (!entity || !entity->HasComponent<AnimationStateMachineComponent>())
			return;

		auto& sm = entity->GetComponent<AnimationStateMachineComponent>();
		if (!sm.stateMachine)
			return;

		const float wallRunEndTransitionDuration = std::clamp(controller.wallRunEndTransitionDuration, 0.0f, 2.0f);
		auto isWallRunState = [](const AnimationStateMachineState& state)
		{
			return state.name == "WallRun" ||
				state.clipName == "Diagonal Wall Run" ||
				state.clipName.find("Wall Run") != std::string::npos ||
				state.clipName.find("wall run") != std::string::npos;
		};

		for (auto& state : sm.stateMachine->states)
		{
			if (!isWallRunState(state))
				continue;

			for (auto& transition : state.transitions)
			{
				const bool hasWallRunEndedCondition = std::any_of(
					transition.conditions.begin(),
					transition.conditions.end(),
					[](const AnimationStateMachineCondition& condition)
					{
						return condition.parameterName == "IsWallRunning" &&
							condition.op == AnimationStateMachineConditionOperator::IfFalse;
					});
				if (hasWallRunEndedCondition || transition.requiresExitTime)
					transition.blendDuration = wallRunEndTransitionDuration;
			}
		}

		auto setParameterByName = [&](const std::string& parameterName, auto&& applyValue)
		{
			if (parameterName.empty())
				return;

			int parameterIndex = sm.stateMachine->FindParameterIndex(parameterName);
			if (parameterIndex < 0)
				return;

			if (static_cast<size_t>(parameterIndex) >= sm.parameterValues.size())
				sm.parameterValues.resize(sm.stateMachine->parameters.size());
			if (static_cast<size_t>(parameterIndex) >= sm.parameterValues.size())
				return;

			applyValue(sm.stateMachine->parameters[parameterIndex], sm.parameterValues[parameterIndex]);
		};

		setParameterByName(controller.animationSpeedParameter, [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Float)
				value.floatValue = controller.currentSpeed;
		});

		setParameterByName(controller.animationGroundedParameter, [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isGrounded;
		});

		setParameterByName(controller.animationJumpTriggerParameter, [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.jumpedThisFrame && !controller.wallJumpTriggeredThisFrame)
				value.triggerValue = true;
		});

		// Fallback mappings for scenes/entities where controller parameter names
		// were not explicitly configured in the component.
		setParameterByName("Speed", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Float)
				value.floatValue = controller.currentSpeed;
		});
		setParameterByName("IsGrounded", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isGrounded;
		});
		setParameterByName("Jump", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.jumpedThisFrame && !controller.wallJumpTriggeredThisFrame)
				value.triggerValue = true;
		});
		setParameterByName("Attack1", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.attack1Requested)
			{
				value.triggerValue = true;
				controller.attack1Requested = false;
			}
		});
		setParameterByName("Attack2", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.attack2Requested)
			{
				value.triggerValue = true;
				controller.attack2Requested = false;
			}
		});
		setParameterByName("Attack3", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.attack3Requested)
			{
				value.triggerValue = true;
				controller.attack3Requested = false;
			}
		});

		setParameterByName("IsCrouching", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.crouchHeld && !controller.isSliding;
		});
		setParameterByName("IsSprinting", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isSprinting;
		});
		setParameterByName("IsSliding", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isSliding;
		});
		setParameterByName("IsDodging", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isDodging;
		});
		setParameterByName("IsVaulting", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isVaulting;
		});
		setParameterByName("IsWallJumping", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isWallJumping;
		});
		setParameterByName("WallJump", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.wallJumpTriggeredThisFrame)
				value.triggerValue = true;
		});
		setParameterByName("IsWallRunning", [&](const auto& parameter, auto& value)
		{
			if (parameter.type == AnimationStateMachineParameterType::Bool)
				value.boolValue = controller.isWallRunning;
		});

		// Update Fight/Attack parameter from action bindings
		{
			const bool isAttacking = entity->HasComponent<SparringPartnerComponent>() ?
				(entity->HasComponent<CombatAttackComponent>() && entity->GetComponent<CombatAttackComponent>().isAttacking) :
				(!entity->HasComponent<ExplorerComponent>() && (InputActions::IsAction("Fight") || InputActions::IsAction("Attack")));
			int fightIdx = sm.stateMachine->FindParameterIndex("IsFighting");
			if (fightIdx >= 0 && static_cast<size_t>(fightIdx) < sm.parameterValues.size())
			{
				sm.parameterValues[fightIdx].boolValue = isAttacking;
			}
		}
	}

	static void ObserveEncounter(Scene& scene, EncounterMemory& memory, glm::vec3 position, float dt, ExplorerComponent* explorer = nullptr)
	{
		memory.TrackCrossing(position);
		if (memory.file.empty()) return;
		memory.sampleDelay -= dt;
		if (memory.sampleDelay > 0.0f) return;
		memory.sampleDelay = 0.75f;
		float nearest = 6.0f;
		glm::vec3 sighting = position;
		std::string label;
		std::vector<std::pair<std::string, glm::vec3>> nearby;
		for (const auto& e : scene.GetEntities())
		{
			if (!e || !e->HasComponent<TransformComponent>() || e->HasComponent<CharacterControllerComponent>()) continue;
			const auto& t = e->GetComponent<TransformComponent>();
			if (t.parentID || (e->GetTag().empty() && e->GetName().empty())) continue;
			const float distance = glm::distance(position, t.position);
			if (explorer && distance <= 6.0f) nearby.emplace_back((e->GetTag().empty() ? e->GetName() : e->GetTag() + ":" + e->GetName()).substr(0,160), t.position);
			if (distance >= nearest) continue;
			nearest = distance;
			sighting = t.position;
			label = e->GetTag().empty() ? e->GetName() : e->GetTag() + ":" + e->GetName();
		}
		if (explorer && !label.empty() && std::any_of(memory.places.begin(), memory.places.end(), [&](const auto& place) {
			return place.object == label && glm::distance(place.objectPosition, sighting) > 1.5f;
		}))
		{
			unsigned matches = 0;
			for (const auto& e : scene.GetEntities())
			{
				if (!e || !e->HasComponent<TransformComponent>() || e->GetComponent<TransformComponent>().parentID || e->HasComponent<CharacterControllerComponent>()) continue;
				if ((e->GetTag().empty() ? e->GetName() : e->GetTag() + ":" + e->GetName()) == label) ++matches;
			}
			if (matches == 1) for (auto& place : memory.places)
			{
				if (place.object != label || glm::distance(place.objectPosition, sighting) <= 1.5f) continue;
				if (explorer->revisitingLandmark && !explorer->following && glm::distance(explorer->goal, place.position) < 0.1f)
				{
					explorer->goal = position; explorer->route.clear(); explorer->replanDelay = 0;
				}
				place.object.clear();
			}
		}
		if (explorer) for (auto& place : memory.places)
		{
			if (place.object.empty() || glm::distance(position, place.objectPosition) > 4.0f) { place.missingChecks = 0; continue; }
			const bool found = std::any_of(nearby.begin(), nearby.end(), [&](const auto& seen) {
				return seen.first == place.object && glm::distance(place.objectPosition, seen.second) <= 1.5f;
			});
			if (found) place.missingChecks = 0;
			else if (++place.missingChecks >= 3)
			{
				if (explorer->revisitingLandmark && !explorer->following && glm::distance(explorer->goal,place.position) < 0.1f)
				{
					explorer->hasGoal = explorer->revisitingLandmark = false;
					explorer->route.clear(); explorer->replanDelay = 0;
				}
				place.object.clear(); place.missingChecks = 0;
			}
		}
		memory.Observe(position, label, sighting, explorer != nullptr);
	}

	static glm::vec3 ChooseRoamingGoal(ExplorerComponent& ai, const glm::vec3& position)
	{
		ai.revisitingLandmark = false;
		ai.landmarkRetries = 0;
		const auto* grid = ai.navigation;
		if (ai.memory.loaded && ai.waypoint % 3 == 0)
		{
			const EncounterMemory::Place* best = nullptr;
			float bestScore = -1.0f;
			for (const auto& place : ai.memory.places)
			{
				const float distance = glm::distance(place.position, position);
				if (distance < 3.0f) continue;
				if (grid && grid->IsReady())
				{
					const auto cell = glm::floor((glm::vec2(place.position.x, place.position.z) - grid->GetOrigin()) / grid->GetCellSize());
					if (!grid->IsCellWalkable(static_cast<int>(cell.x), static_cast<int>(cell.y))) continue;
				}
				const float score = (place.object.empty() ? 1.0f : 2.0f) / (1.0f + distance * 0.02f + place.visits * 0.1f);
				if (score > bestScore) { bestScore = score; best = &place; }
			}
			if (best) { ai.revisitingLandmark = !best->object.empty(); return best->position; }
		}
		if (!grid || !grid->IsReady() || ai.waypoint == 0)
		{
			static const glm::vec3 offsets[] = {{7, 0, 0}, {7, 0, 6}, {-3, 0, 6}, {-3, 0, -4}, {4, 0, -4}};
			return ai.origin + offsets[ai.waypoint % 5];
		}
		auto sample = [&]() { ai.roamSeed = ai.roamSeed * 1664525u + 1013904223u; return ai.roamSeed >> 8; };
		glm::vec3 best = position;
		float bestScore = -1.0f;
		for (int i = 0; i < 24 && grid->GetCols() > 0 && grid->GetRows() > 0; ++i)
		{
			const int col = sample() % grid->GetCols(), row = sample() % grid->GetRows();
			if (!grid->IsCellWalkable(col, row)) continue;
			const glm::vec2 p = grid->GetOrigin() + glm::vec2(col + 0.5f, row + 0.5f) * grid->GetCellSize();
			float score = glm::distance(p, glm::vec2(position.x, position.z));
			for (const auto& past : ai.recentGoals) score = std::min(score, glm::distance(p, glm::vec2(past.x, past.z)));
			if (ai.memory.loaded) score /= 1.0f + ai.memory.Familiarity({p.x, position.y, p.y});
			if (score > bestScore) { bestScore = score; best = {p.x, position.y, p.y}; }
		}
		return best;
	}

	static void ScanExplorerMarkers(Scene& scene, ExplorerComponent& ai, glm::vec3 position)
	{
		if (!ai.following && !ai.interest)
		{
			float nearest = 14.0f;
			for (const auto& e : scene.GetEntities())
			{
				if (!e || e->GetTag() != "ExplorerPOI" || !e->HasComponent<TransformComponent>() ||
					std::find(ai.recentInterests.begin(), ai.recentInterests.end(), e->GetID()) != ai.recentInterests.end()) continue;
				const auto p = e->GetComponent<TransformComponent>().position;
				const float d = glm::distance(position, p);
				if (d < nearest) { ai.interest = e->GetID(); ai.goal = p; nearest = d; }
			}
			if (ai.interest)
			{
				ai.revisitingLandmark = false;
				ai.recentInterests.push_back(ai.interest);
				if (ai.recentInterests.size() > 8) ai.recentInterests.erase(ai.recentInterests.begin());
				ai.route.clear(); ai.hasGoal = true; ai.replanDelay = 0.0f;
			}
		}
		if (ai.entrance || std::abs(ai.goal.y - position.y) < 1.5f) return;
		std::unordered_map<std::string, const Entity*> ends;
		float best = std::numeric_limits<float>::max();
		for (const auto& e : scene.GetEntities())
		{
			const std::string prefix = "ExplorerLink:";
			if (!e || e->GetTag().compare(0, prefix.size(), prefix) != 0 || e->GetTag().size() == prefix.size() || !e->HasComponent<TransformComponent>()) continue;
			auto pair = ends.emplace(e->GetTag(), e.get());
			if (pair.second) continue;
			const Entity* entry = pair.first->second;
			const Entity* out = e.get();
			auto a = entry->GetComponent<TransformComponent>().position, b = out->GetComponent<TransformComponent>().position;
			if (std::abs(a.y - position.y) > std::abs(b.y - position.y)) { std::swap(a, b); std::swap(entry, out); }
			if (std::abs(a.y - position.y) > 1.25f || std::abs(b.y - ai.goal.y) + 0.75f >= std::abs(position.y - ai.goal.y)) continue;
			const float cost = glm::distance(position, a) + glm::distance(a, b) + glm::distance(b, ai.goal);
			if (cost < best) { best = cost; ai.entrance = entry->GetID(); ai.exit = out->GetID(); }
		}
		if (ai.entrance) { ai.route.clear(); ai.replanDelay = 0.0f; }
	}

	static bool ExplorerSlotOpen(const NavMeshSystem* nav, glm::vec3 slot, float radius)
	{
		if (!nav || !nav->IsReady()) return true;
		for (float x : {-radius, radius}) for (float z : {-radius, radius})
		{
			const auto cell = glm::floor((glm::vec2(slot.x + x, slot.z + z) - nav->GetOrigin()) / nav->GetCellSize());
			if (!nav->IsCellWalkable(static_cast<int>(cell.x), static_cast<int>(cell.y))) return false;
		}
		return true;
	}

	static void UpdateExplorerInput(Scene& scene, ExplorerComponent& ai, CharacterControllerComponent& controller, const glm::vec3& position, float radius, float dt)
	{
		auto direction = [&](glm::vec3 target) {
			glm::vec3 delta = target - position;
			delta.y = 0.0f;
			return delta;
		};
		ai.legTime += dt;
		ai.checkTimer += dt;
		ai.replanDelay = std::max(0.0f, ai.replanDelay - dt);
		ai.probeDelay = std::max(0.0f, ai.probeDelay - dt);
		controller.jumpHeld = false;
		controller.wallRunRequested = !controller.isGrounded;
		controller.crouchHeld = controller.isSliding;
		ai.requestedPace = 1.0f;
		bool movingFormation = false;
		if (ai.following)
		{
			const Entity* player = scene.GetEntityByID(ai.playerId);
			if (!player || !player->HasComponent<TransformComponent>()) ai.SetFollowing(false);
			else
			{
				const glm::vec3 p = player->GetComponent<TransformComponent>().position;
				ai.lookDirection = direction(p);
				glm::vec3 destination = p;
				if (player->HasComponent<RigidbodyComponent>())
				{
					glm::vec3 velocity = player->GetComponent<RigidbodyComponent>().velocity;
					velocity.y = 0.0f;
					const float speed = glm::length(velocity);
					if (speed > 0.5f)
					{
						movingFormation = true;
						const glm::vec3 forward = velocity / speed;
						const glm::vec3 side(forward.z, 0.0f, -forward.x);
						glm::vec3 slot = p - forward * 1.6f;
						float best = std::numeric_limits<float>::max();
						int chosen = ai.followSide;
						for (int sign : {ai.followSide, -ai.followSide})
						{
							const auto candidate = p + side * (1.6f * sign);
							if (!ExplorerSlotOpen(ai.navigation, candidate, radius) || !ExplorerSlotOpen(ai.navigation, candidate + velocity * 0.35f, radius)) continue;
							const float score = glm::length(direction(candidate)) + (sign == ai.followSide ? 0.0f : 0.8f);
							if (score < best) { best = score; chosen = sign; slot = candidate; }
						}
						ai.followSide = chosen;
						const glm::vec3 error = direction(slot);
						destination = slot + velocity * 0.35f;
						const float cruise = controller.moveSpeed * (controller.sprintLatched ? controller.sprintMultiplier : 1.0f);
						ai.requestedPace = std::clamp(glm::length(velocity + error * 1.5f) / std::max(cruise, 0.1f), 0.05f, 1.0f);
					}
				}
				ai.waiting = !movingFormation && glm::length(ai.lookDirection) < (ai.waiting ? 3.0f : 2.0f) && std::abs(p.y - position.y) < 1.5f;
				if (ai.waiting) { controller.moveInput = glm::vec3(0.0f); ai.hasGoal = false; ai.route.clear(); return; }
				if (!ai.hasGoal || (ai.replanDelay == 0.0f && glm::distance(destination, ai.goal) > (movingFormation ? 0.25f : 1.0f)))
				{ ai.goal = destination; ai.hasGoal = true; ai.route.clear(); }
			}
		}
		if (!ai.hasGoal) { ai.goal = ChooseRoamingGoal(ai, position); ai.hasGoal = true; }
		ai.scanDelay -= dt;
		if (ai.scanDelay <= 0.0f) { ScanExplorerMarkers(scene, ai, position); ai.scanDelay = 0.8f; }
		if (ai.interest && glm::distance(position, ai.goal) < 1.5f)
		{
			ai.inspectTime += dt;
			ai.lookDirection = direction(ai.goal);
			controller.moveInput = glm::vec3(0.0f);
			if (ai.inspectTime >= 1.5f)
			{
				ai.inspectTime = 0.0f;
				ai.interest = ai.entrance = ai.exit = 0;
				ai.crossing = ai.hasGoal = false;
				ai.route.clear();
			}
			return;
		}
		if (ai.checkTimer >= 0.65f)
		{
			glm::vec3 travel = position - ai.checkpoint;
			travel.y = 0.0f;
			ai.blockedChecks = glm::length(travel) < 0.3f ? ai.blockedChecks + 1 : 0;
			ai.checkpoint = position;
			ai.checkTimer = 0.0f;
			if ((ai.blockedChecks == 2 && controller.isGrounded) || controller.isWallRunning)
				controller.jumpRequested = controller.jumpHeld = true;
		}
		if ((!ai.following && glm::distance(position, ai.goal) < 0.6f) || (ai.blockedChecks >= 3 && !ai.revisitingLandmark))
		{
			ai.route.clear();
			ai.nextNode = 0;
			ai.entrance = ai.exit = ai.interest = 0; ai.crossing = false;
			ai.replanDelay = 0.0f;
			if (!ai.following)
			{
				if (ai.recentGoals.size() == 12) ai.recentGoals.erase(ai.recentGoals.begin());
				ai.recentGoals.push_back(ai.goal);
				++ai.waypoint;
				ai.goal = ChooseRoamingGoal(ai, position);
			}
			ai.legTime = ai.checkTimer = 0.0f;
			ai.blockedChecks = 0;
			ai.checkpoint = position;
			if (ai.waypoint == 1 && !controller.sprintLatched) controller.sprintRequested = true;
			if (ai.waypoint == 2 && controller.isSprinting)
			{
				controller.slideRequested = true;
				controller.slideInputBufferTimer = controller.slideInputBufferDuration;
			}
			if (ai.waypoint == 3) controller.dodgeRequested = true;
		}
		glm::vec3 target = ai.goal;
		if (ai.entrance)
		{
			const Entity* marker = scene.GetEntityByID(ai.crossing ? ai.exit : ai.entrance);
			if (!marker || !marker->HasComponent<TransformComponent>() || glm::distance(position, marker->GetComponent<TransformComponent>().position) < 0.85f)
			{
				if (ai.crossing || !marker || !marker->HasComponent<TransformComponent>()) { ai.entrance = ai.exit = 0; ai.crossing = false; }
				else ai.crossing = true;
				ai.route.clear(); ai.replanDelay = 0.0f;
				controller.moveInput = glm::vec3(0.0f); return;
			}
			target = marker->GetComponent<TransformComponent>().position;
		}
		const auto* grid = ai.navigation;
		if (!ai.crossing && grid && grid->IsReady())
		{
			auto walkable = [&](glm::vec3 p) {
				glm::vec2 cell = glm::floor((glm::vec2(p.x, p.z) - grid->GetOrigin()) / grid->GetCellSize());
				return grid->IsCellWalkable(static_cast<int>(cell.x), static_cast<int>(cell.y));
			};
			const float radius = std::min(0.45f, grid->GetCellSize() * 0.4f);
			while (ai.nextNode < ai.route.size() && glm::length(direction(ai.route[ai.nextNode])) <= radius)
				++ai.nextNode;
			if (ai.nextNode < ai.route.size() && !walkable(ai.route[ai.nextNode])) ai.route.clear();
			if (ai.nextNode >= ai.route.size() && ai.replanDelay == 0.0f)
			{
				ai.route = grid->FindPath(position, target, ai.memory.stalls);
				ai.nextNode = ai.route.size() > 1 ? 1 : 0;
				ai.replanDelay = 0.75f;
				if (ai.route.empty() && walkable(position))
				{
					if (!ai.following) { ++ai.waypoint; ai.hasGoal = false; ai.interest = ai.entrance = ai.exit = 0; }
					ai.legTime = ai.checkTimer = 0.0f;
					ai.blockedChecks = 0;
				}
			}
			if (ai.nextNode < ai.route.size()) target = ai.route[ai.nextNode];
			else if (walkable(position))
			{
				controller.moveInput = glm::vec3(0.0f);
				return;
			}
		}
		if (ai.probeDelay == 0.0f && controller.enableVault && controller.enableSprintSlide && controller.isGrounded && !controller.isSliding)
		{
			controller.sprintRequested |= !controller.sprintLatched;
			controller.vaultRequested = true;
			ai.probeDelay = 0.18f;
		}
		const glm::vec3 delta = direction(target);
		controller.moveInput = delta / std::max(glm::length(delta), 0.001f);
		const float cruise = std::max(0.1f, controller.moveSpeed * (controller.sprintLatched ? controller.sprintMultiplier : 1.0f));
		const float braking = std::max(0.5f, controller.braking * 0.45f);
		if (ai.nextNode + 1 < ai.route.size())
		{
			glm::vec3 outgoing = ai.route[ai.nextNode + 1] - target;
			outgoing.y = 0.0f;
			const float alignment = glm::dot(controller.moveInput, outgoing / std::max(glm::length(outgoing), 0.001f));
			const float cornerSpeed = cruise * std::clamp(0.48f + 0.52f * alignment, 0.25f, 1.0f);
			const float distance = std::max(0.0f, glm::length(delta) - 0.45f);
			ai.requestedPace = std::min(ai.requestedPace, std::sqrt(cornerSpeed * cornerSpeed + 2.0f * braking * distance) / cruise);
		}
		if (!movingFormation)
		{
			const float stopRadius = ai.following ? 2.0f : (ai.interest ? 1.5f : 0.6f);
			const float distance = std::max(0.0f, glm::length(direction(ai.goal)) - stopRadius);
			ai.requestedPace = std::min(ai.requestedPace, std::max(0.18f, std::sqrt(2.0f * braking * distance) / cruise));
		}
		const float remaining = glm::distance(position, target);
		if (ai.bestDistance < 0.0f || glm::distance(ai.progressTarget, target) > 0.5f || remaining < ai.bestDistance - 0.12f)
		{
			ai.bestDistance = remaining; ai.progressTarget = target; ai.noProgress = 0.0f;
		}
		else ai.noProgress += dt;
		if (ai.noProgress >= 2.0f || ai.blockedChecks >= 2)
		{
			const bool retryLandmark = ai.revisitingLandmark && !ai.following && !ai.entrance && ai.nextNode < ai.route.size() && ai.landmarkRetries < 3;
			if (retryLandmark) { ai.memory.NoteStall(ai.route[ai.nextNode]); ++ai.landmarkRetries; }
			++ai.recoveries;
			const float sign = ai.recoveries % 2 ? 1.0f : -1.0f;
			ai.escapeDirection = glm::vec3(-controller.moveInput.z, 0, controller.moveInput.x) * sign;
			ai.escapeTime = 0.45f;
			ai.route.clear(); ai.replanDelay = 0.0f; ai.bestDistance = -1.0f;
			ai.blockedChecks = 0; ai.noProgress = 0.0f;
			if (!ai.following && !retryLandmark) { ai.revisitingLandmark = false; ai.interest = ai.entrance = ai.exit = 0; ai.crossing = ai.hasGoal = false; ++ai.waypoint; }
		}
		if (ai.escapeTime > 0.0f)
		{
			ai.escapeTime -= dt;
			controller.moveInput = ai.escapeDirection;
		}
	}

	// Horizontal decks only. The radius-expanded footprint is convex, so checking
	// both ends of the step verifies reach throughout a constant-velocity window.
	static float BoardingETA(glm::vec3 dock, glm::vec3 lo, glm::vec3 hi, glm::vec3 velocity, float reach, float stepTime)
	{
		const glm::vec2 point(dock.x, dock.z), motion(velocity.x, velocity.z);
		auto inReach = [&](float t) {
			const glm::vec2 local = point - motion * t;
			const glm::vec2 offset = local - glm::clamp(local, glm::vec2(lo.x, lo.z), glm::vec2(hi.x, hi.z));
			return glm::dot(offset, offset) <= reach * reach;
		};
		for (int i = 0; i <= 80; ++i)
		{
			const float eta = i * 0.05f;
			if (inReach(eta) && inReach(eta + stepTime)) return eta;
		}
		return std::numeric_limits<float>::infinity();
	}

	static bool ContinueEncounterJump(EncounterSteering& ai, CharacterControllerComponent& controller, glm::vec3 position, float dt)
	{
		if (ai.plannedFlight <= 0) return false;
		ai.plannedFlight = std::max(0.0f, ai.plannedFlight - dt);
		if (!controller.isGrounded && !controller.isVaulting && !controller.isWallRunning)
		{
			auto toLanding = ai.jumpLanding - position; toLanding.y = 0;
			controller.moveInput = toLanding / std::max(glm::length(toLanding), 0.3f) * ai.jumpInput;
			controller.wallRunRequested = false;
			return true;
		}
		ai.plannedFlight = 0;
		return false;
	}

	void PhysicsSystem::SteerExplorer(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, float dt) const
	{
		auto& ai = entity->GetComponent<ExplorerComponent>();
		glm::vec3 desired = controller.moveInput;
		if (ContinueEncounterJump(ai, controller, entity->GetComponent<TransformComponent>().position, dt)) return;
		float pace = 1.0f;
		if (controller.isGrounded && !controller.isVaulting && ai.hasGoal && !ai.waiting)
		{
			const glm::vec3 p = entity->GetComponent<TransformComponent>().position;
			glm::vec3 bodyMin, bodyMax;
			ComputeColliderAABB(scene, entity, bodyMin, bodyMax);
			const float radius = entity->GetComponent<CapsuleColliderComponent>().radius;
			const float stepTime = std::clamp((radius + 0.12f) / std::max(0.1f, controller.moveSpeed * 0.65f) + 0.12f, 0.2f, 1.0f);
			glm::vec3 toward = ai.goal - p; toward.y = 0;
			toward /= std::max(glm::length(toward), 0.001f);
			ai.ferryTimer -= dt;
			if (!ai.ferry && ai.ferryTimer <= 0.0f)
			{
				ai.ferryTimer = 0.25f;
				float best = std::numeric_limits<float>::infinity();
				for (const auto& deck : scene.GetEntities())
				{
					if (!deck || deck->GetTag() != "ExplorerPlatform" || !deck->HasComponent<BoxColliderComponent>() || deck->GetComponent<BoxColliderComponent>().isTrigger || !deck->HasComponent<RigidbodyComponent>()) continue;
					glm::vec3 lo, hi;
					if (!ComputeColliderAABB(scene, deck, lo, hi) || std::abs(hi.y - bodyMin.y) > controller.maxStepHeight + 0.1f) continue;
					glm::vec3 edge = glm::clamp(p, lo, hi) - p; edge.y = 0;
					const float gap = glm::length(edge);
					if (gap > radius && !(ai.unsafeEdge && glm::dot(edge, toward) > 0.6f * gap)) continue;
					const float eta = gap == 0.0f ? -1.0f : BoardingETA(p, lo, hi, deck->GetComponent<RigidbodyComponent>().velocity, radius + 0.12f, stepTime);
					if (eta < best) { best = eta; ai.ferry = deck->GetID(); ai.ferryTimer = 5.0f; }
				}
			}
			const auto deck = ai.ferry ? scene.GetEntitySharedByID(ai.ferry) : nullptr;
			glm::vec3 lo, hi;
			if (deck && ComputeColliderAABB(scene, deck, lo, hi) && std::abs(hi.y - bodyMin.y) <= controller.maxStepHeight + 0.1f)
			{
				const bool onDeck = p.x >= lo.x && p.x <= hi.x && p.z >= lo.z && p.z <= hi.z;
				const float eta = onDeck || !deck->HasComponent<RigidbodyComponent>() ? std::numeric_limits<float>::infinity() :
					BoardingETA(p, lo, hi, deck->GetComponent<RigidbodyComponent>().velocity, radius + 0.12f, stepTime);
				if (onDeck || (!ai.riding && ai.ferryTimer > 0.0f && std::isfinite(eta)))
				{
					ai.checkpoint = p; ai.noProgress = ai.checkTimer = 0; ai.blockedChecks = 0;
					controller.jumpRequested = controller.vaultRequested = controller.dodgeRequested = controller.slideRequested = false;
					glm::vec3 command(0.0f);
					if (onDeck)
					{
						ai.riding = true;
						float edgeDist = std::numeric_limits<float>::max();
						for (int axis : {0, 2}) if (std::abs(toward[axis]) > 0.001f)
							edgeDist = std::min(edgeDist, ((toward[axis] > 0 ? hi[axis] : lo[axis]) - p[axis]) / toward[axis]);
						if (edgeDist < 8.0f)
						{
							const float step = std::max(controller.maxStepHeight, 0.1f);
							glm::vec3 landing = p + toward * (edgeDist + radius + 0.2f);
							landing.y += step + 0.1f;
							glm::vec3 seam = p + toward * (edgeDist + radius + 0.04f);
							seam.y = landing.y;
							if (FindVaultLanding(scene, entity, controller, landing, p.y - step) && FindVaultLanding(scene, entity, controller, seam, p.y - step)) command = toward;
						}
					}
					else if (eta == 0.0f)
					{
						glm::vec3 center = (hi + lo) * 0.5f - p; center.y = 0;
						command = center / std::max(glm::length(center), 0.01f);
					}
					ai.steering = controller.moveInput = command * 0.65f;
					return;
				}
			}
			if (ai.ferry) { ai.ferry = 0; ai.riding = false; ai.ferryTimer = 0.5f; }
		}
		if (ai.following && glm::length(desired) > 0.1f)
		{
			const glm::vec3 p = entity->GetComponent<TransformComponent>().position;
			const glm::vec3 side(-desired.z, 0, desired.x);
			const float radius = entity->GetComponent<CapsuleColliderComponent>().radius;
			float closest = 2.5f;
			float lateral = 0.0f;
			for (const auto& other : scene.GetEntities())
			{
				if (!other || other == entity || other->GetID() == ai.playerId || !other->HasComponent<CharacterControllerComponent>() || !other->HasComponent<TransformComponent>()) continue;
				const glm::vec3 relative = other->GetComponent<TransformComponent>().position - p;
				const float ahead = glm::dot(relative, desired), offset = glm::dot(relative, side);
				if (std::abs(relative.y) < 1.2f && ahead >= 0.0f && ahead < closest && std::abs(offset) < radius * 2.0f + 0.4f)
				{ closest = ahead; lateral = offset; }
			}
			if (closest < 2.5f)
			{
				float sign = lateral > 0.0f ? -1.0f : 1.0f;
				auto open = [&](float s) {
					if (!ai.navigation || !ai.navigation->IsReady()) return true;
					const auto& nav = *ai.navigation;
					const auto q = p + desired * 0.65f + side * s * std::max(nav.GetCellSize(), radius * 2.5f);
					const auto c = glm::floor((glm::vec2(q.x, q.z) - nav.GetOrigin()) / nav.GetCellSize());
					return nav.IsCellWalkable(static_cast<int>(c.x), static_cast<int>(c.y));
				};
				if (!open(sign)) sign = -sign;
				if (open(sign)) { desired = glm::normalize(desired + side * sign); pace = 0.65f; }
				else pace = std::clamp((closest - 2.0f * radius - 0.25f) / 2.0f, 0.0f, 0.5f);
			}
		}
		SteerTerrain(scene, entity, controller, ai, desired, pace, dt);
	}

	void PhysicsSystem::SteerTerrain(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller, EncounterSteering& ai, glm::vec3 desired, float pace, float dt) const
	{
		if (glm::length(desired) < 0.01f) { ai.steering = glm::vec3(0.0f); ai.terrainProbeTimer = 0.0f; return; }
		const glm::vec3 lateral(desired.z, 0, -desired.x);
		auto laneDirection = [&](int lane) { return glm::normalize(desired + lateral * (0.65f * lane)); };
		ai.clearanceTimer -= dt;
		if (ai.clearanceTimer <= 0.0f || glm::dot(desired, ai.clearanceHeading) < 0.85f)
		{
			ai.clearanceTimer = 0.12f;
			ai.clearanceHeading = desired;
			glm::vec3 bodyMin, bodyMax;
			ComputeColliderAABB(scene, entity, bodyMin, bodyMax);
			const glm::vec3 center = (bodyMin + bodyMax) * 0.5f;
			const glm::vec3 padding = (bodyMax - bodyMin) * 0.5f + glm::vec3(0.08f, 0, 0.08f);
			const float speed = std::max(1.0f, controller.moveSpeed * ai.requestedPace);
			float clearTime[3] = {0.65f, 0.65f, 0.65f};
			for (const auto& other : scene.GetEntities())
			{
				if (!other || other == entity || other->GetTag() == "ExplorerPlatform") continue;
				if (entity->HasComponent<SparringPartnerComponent>())
				{
					const auto& partner = entity->GetComponent<SparringPartnerComponent>();
					if ((partner.counterWindow > 0 || partner.closingForJab) && other->GetID() == partner.playerId) continue;
				}
				const bool box = other->HasComponent<BoxColliderComponent>();
				const bool capsule = other->HasComponent<CapsuleColliderComponent>();
				const bool sphere = other->HasComponent<BoundingSphereComponent>();
				if (!(box || capsule || sphere) || (box && other->GetComponent<BoxColliderComponent>().isTrigger) ||
					(capsule && other->GetComponent<CapsuleColliderComponent>().isTrigger) || (sphere && other->GetComponent<BoundingSphereComponent>().isTrigger)) continue;
				glm::vec3 lo, hi;
				if (!ComputeColliderAABB(scene, other, lo, hi) || hi.y <= bodyMin.y + controller.maxStepHeight) continue;
				const glm::vec3 motion = other->HasComponent<RigidbodyComponent>() ? other->GetComponent<RigidbodyComponent>().velocity : glm::vec3(0);
				if (box && controller.enableVault && controller.enableSprintSlide && hi.y <= bodyMin.y + controller.vaultMaxHeight && glm::length(motion) < 0.1f) continue;
				lo -= padding; hi += padding;
				for (int lane = -1; lane <= 1; ++lane)
				{
					const glm::vec3 v = laneDirection(lane) * speed - motion;
					float enter = 0, leave = clearTime[lane + 1];
					for (int axis = 0; axis < 3 && enter <= leave; ++axis)
					{
						if (std::abs(v[axis]) < 0.0001f) { if (center[axis] < lo[axis] || center[axis] > hi[axis]) leave = -1; continue; }
						const float a = (lo[axis] - center[axis]) / v[axis], b = (hi[axis] - center[axis]) / v[axis];
						enter = std::max(enter, std::min(a,b)); leave = std::min(leave, std::max(a,b));
					}
					if (enter <= leave) clearTime[lane + 1] = enter;
				}
			}
			ai.clearanceLane = 0;
			for (int lane : {-1, 1}) if (clearTime[lane + 1] > clearTime[ai.clearanceLane + 1] + 0.06f) ai.clearanceLane = lane;
			ai.clearancePace = std::clamp((clearTime[ai.clearanceLane + 1] - 0.07f) / 0.35f, 0.0f, 1.0f);
		}
		desired = laneDirection(ai.clearanceLane);
		pace = std::min(pace, ai.clearancePace);
		ai.terrainProbeTimer -= dt;
		if (controller.isGrounded && !controller.isVaulting && ai.terrainProbeTimer <= 0.0f)
		{
			ai.terrainProbeTimer = 0.16f;
			ai.avoidance = glm::vec3(0.0f);
			const glm::vec3 p = entity->GetComponent<TransformComponent>().position;
			const float speed = glm::length(entity->GetComponent<RigidbodyComponent>().velocity);
			const float ahead = std::clamp(speed * 0.22f + 0.35f, 0.55f, 2.0f);
			const float step = std::max(controller.maxStepHeight, 0.1f);
			auto landing = [&](glm::vec3 dir, float distance, float rise, float& height) {
				glm::vec3 sample = p + dir * distance;
				sample.y += rise + 0.1f;
				if (!FindVaultLanding(scene, entity, controller, sample, p.y - step - 0.15f)) return false;
				height = sample.y - p.y;
				return height <= rise + 0.05f;
			};
			float height = 0.0f;
			ai.unsafeEdge = !landing(desired, ahead, step, height);
			if (ai.unsafeEdge)
			{
				const float g = std::abs(gravity.y * controller.gravityScale);
				const float rise = g > 0.01f ? std::min(1.5f, 0.4f * controller.jumpSpeed * controller.jumpSpeed / g) : 0.0f;
				glm::vec3 velocity = entity->GetComponent<RigidbodyComponent>().velocity;
				velocity.y = 0.0f;
				const float horizontalSpeed = glm::length(velocity);
				const glm::vec3 launch = velocity / std::max(horizontalSpeed, 0.001f);
				if (rise > step && horizontalSpeed > 0.5f && glm::dot(launch, desired) > 0.9f && pace > 0.8f)
				{
					for (float candidateHeight : {rise, 0.5f * rise, 0.0f, -step})
					{
						auto flightTime = [&](float h) { return (controller.jumpSpeed + std::sqrt(std::max(0.0f, controller.jumpSpeed * controller.jumpSpeed - 2.0f * g * h))) / g; };
						float distance = horizontalSpeed * flightTime(candidateHeight);
						if (distance <= ahead || !landing(launch, distance, rise, height)) continue;
						const float flight = flightTime(height);
						distance = horizontalSpeed * flight;
						float actualHeight = 0.0f;
						if (!landing(launch, distance, rise, actualHeight) || std::abs(actualHeight - height) > 0.1f) continue;
						bool clear = true;
						for (int i = 1; i < 10 && clear; ++i)
						{
							const float t = flight * i / 10.0f;
							glm::vec3 sample = p + velocity * t;
							sample.y += controller.jumpSpeed * t - 0.5f * g * t * t;
							glm::vec3 normal; float penetration;
							clear = !QueryVaultContact(scene, entity, sample, normal, penetration) || penetration <= 0.001f;
						}
						if (!clear) continue;
						ai.jumpLanding = p + launch * distance + glm::vec3(0, actualHeight, 0);
						ai.plannedFlight = flight + dt;
						ai.jumpInput = std::min(1.0f, horizontalSpeed / std::max(controller.moveSpeed, 0.1f));
						controller.jumpRequested = controller.jumpHeld = true;
						ai.unsafeEdge = false;
						break;
					}
				}
				if (ai.unsafeEdge)
				{
					const glm::vec3 side(-desired.z, 0, desired.x);
					for (float sign : {1.0f, -1.0f})
					{
						const glm::vec3 turn = glm::normalize(desired * 0.35f + side * sign);
						if (landing(turn, ahead, step, height)) { ai.avoidance = turn; break; }
					}
				}
			}
		}
		if (controller.isGrounded && ai.unsafeEdge)
		{
			controller.jumpRequested = controller.dodgeRequested = controller.slideRequested = false;
			desired = ai.avoidance;
			ai.steering = desired;
		}
		else
		{
			const glm::vec3 demand = desired * ai.requestedPace;
			const float rate = glm::length(demand) > glm::length(ai.steering) ? 3.0f : 5.0f;
			ai.steering = MoveTowards(ai.steering, demand, dt * rate);
		}
		controller.moveInput = ai.steering * pace;
	}

	void PhysicsSystem::SteerPartner(Scene& scene, const std::shared_ptr<Entity>& entity, CharacterControllerComponent& ctl, float distance, float dt) const
	{
		auto& ai = entity->GetComponent<SparringPartnerComponent>();
		auto& terrain = ai.traversal;
		const auto p = entity->GetComponent<TransformComponent>().position;
		if (ContinueEncounterJump(terrain, ctl, p, dt)) return;
		const float speed = glm::length(ctl.moveInput);
		if (speed < 0.01f) { terrain.steering = {}; ai.replan = ai.progressTime = ai.escapeTime = 0; ai.route.clear(); return; }
		auto dir = ctl.moveInput / speed;
		const bool closing = glm::dot(dir, ai.facing) > 0.8f;
		ai.replan -= dt;
		if (((closing && distance > 3) || (ai.flanking && distance > 2)) && ai.navigation && ai.navigation->IsReady())
		{
			if (ai.replan <= 0)
			{
				const auto target = p + ai.facing * distance;
				const auto& grid = *ai.navigation;
				auto visible = [&](glm::vec3 from, glm::vec3 to) {
					const int steps = std::clamp(static_cast<int>(std::ceil(glm::distance(from,to) / (0.45f * grid.GetCellSize()))), 1, 512);
					for (int i = 0; i <= steps; ++i)
					{
						const auto q = glm::mix(from,to,static_cast<float>(i)/steps);
						const auto cell = glm::floor((glm::vec2(q.x,q.z) - grid.GetOrigin()) / grid.GetCellSize());
						if (!grid.IsCellWalkable(static_cast<int>(cell.x),static_cast<int>(cell.y))) return false;
					}
					return true;
				};
				const bool previouslyFlanking = ai.flanking;
				ai.flanking = false;
				if (ai.flankHoldoff <= 0 && !visible(p,target))
				{
					const glm::vec3 side(-ai.facing.z,0,ai.facing.x);
					for (int sign : {ai.flankSide,-ai.flankSide})
					{
						const auto slot = target + side * (2.4f * sign);
						if (!visible(slot,target)) continue;
						auto path = grid.FindPath(p,slot,ai.memory.stalls);
						if (path.empty()) continue;
						ai.route = std::move(path); ai.flanking = true; ai.flankSide = sign; break;
					}
				}
				if (!ai.flanking) ai.route = grid.FindPath(p,target,ai.memory.stalls);
				if (ai.flanking != previouslyFlanking) ai.RecordDecision(ai.flanking ? "Cover blocks approach; taking flank" : "Flank ended; resuming pursuit");
				ai.node = ai.route.size() > 1 ? 1 : 0;
				ai.replan = 0.75f;
			}
			while (ai.node < ai.route.size())
			{
				auto next = ai.route[ai.node] - p; next.y = 0;
				if (glm::length(next) > 0.4f) { dir = glm::normalize(next); break; }
				++ai.node;
			}
		}
		else { ai.replan = 0; ai.route.clear(); }
		if (ai.progressTime <= 0) ai.checkpoint = p;
		ai.progressTime += dt;
		if (ai.progressTime >= 0.8f)
		{
			if (glm::distance(glm::vec2(p.x,p.z), glm::vec2(ai.checkpoint.x,ai.checkpoint.z)) < 0.2f)
			{
				if (closing && ai.node < ai.route.size()) ai.memory.NoteStall(ai.route[ai.node]);
				ai.escape = glm::vec3(-dir.z,0,dir.x) * static_cast<float>(ai.escapeSide);
				ai.escapeSide = -ai.escapeSide; ai.escapeTime = 0.45f; ai.replan = 0;
			}
			ai.progressTime = 0;
		}
		if (ai.escapeTime > 0) { dir = ai.escape; ai.escapeTime -= dt; }
		if (!ctl.isGrounded) { ctl.moveInput = dir * speed; return; }
		terrain.requestedPace = speed;
		if (glm::dot(dir, terrain.clearanceHeading) < 0.85f) terrain.terrainProbeTimer = 0;
		SteerTerrain(scene, entity, ctl, terrain, dir, 1.0f, dt);
		if (closing && !(ai.closingForJab && distance < 2.5f) && ctl.enableVault && ctl.enableSprintSlide && !terrain.unsafeEdge && !ctl.jumpRequested && glm::length(ctl.moveInput) > 0.1f)
		{
			ctl.sprintRequested = !ctl.sprintLatched;
			ctl.vaultRequested = true;
		}
	}

	void PhysicsSystem::StepSparringRagdoll(Scene& scene, const std::shared_ptr<Entity>& entity, float dt)
	{
		auto& rig = entity->GetComponent<SparringRig>();
		auto& ctl = entity->GetComponent<CharacterControllerComponent>();
		auto& rb = entity->GetComponent<RigidbodyComponent>();
		auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		auto& transform = entity->GetComponent<TransformComponent>();
		const auto world = TransformHierarchy::GetWorldMatrix(scene,*entity), inverse = glm::inverse(world);
		if (!rig.initialized)
		{
			rig.initial = rig.previous = rig.points = rig.Pose(*entity);
			if (entity->HasComponent<AnimationComponent>()) rig.basePalette = entity->GetComponent<AnimationComponent>().boneMatrices;
			rig.savedTrigger = capsule.isTrigger; capsule.isTrigger = true;
			for (int j = 0; j < SparringRig::Count; ++j) rig.previous[j].z += dt * (0.5f + rig.rest[j].y);
			rig.initialized = true;
			ctl.isDodging = ctl.isSliding = ctl.isWallRunning = ctl.isVaulting = ctl.sprintLatched = false;
			auto& ai = entity->GetComponent<SparringPartnerComponent>();
			ai.counterWindow = ai.escapeTime = 0; ai.responding = ai.flanking = false; ai.route.clear();
		}
		ctl.ClearFrameInput(); ctl.capsuleBottomAnimationLift = 0;
		rb.isKinematic = true; rb.velocity = rb.acceleration = glm::vec3(0);
		rig.downTime += dt;
		const auto g = glm::vec3(inverse * glm::vec4(gravity,0));
		const auto impactStep = glm::vec3(inverse * glm::vec4(rig.burstRecoil * dt,0));
		rig.burstRecoil = glm::vec3(0);
		for (int j = 0; j < SparringRig::Count; ++j)
		{
			const auto p = rig.points[j];
			rig.points[j] += (p - rig.previous[j]) * .96f + g * dt * dt + impactStep;
			rig.previous[j] = p;
		}
		for (int pass = 0; pass < 5; ++pass) for (int j = 1; j < SparringRig::Count; ++j)
		{
			const int k = rig.parent[j];
			const auto d = rig.points[j] - rig.points[k];
			const float len = glm::length(d);
			if (len < .001f) continue;
			const auto correction = d * (.5f * (1 - glm::distance(rig.initial[j],rig.initial[k]) / len));
			rig.points[j] -= correction; rig.points[k] += correction;
		}
		const auto savedCapsule = capsule;
		const float scale = glm::compMax(ExtractWorldScale(scene,*entity));
		for (int j = 0; j < SparringRig::Count; ++j)
		{
			glm::vec3 p = world * glm::vec4(rig.points[j],1);
			capsule.radius = rig.radius[j] * scale;
			capsule.pointA = glm::vec3(0); capsule.pointB = glm::vec3(0,.001f,0);
			glm::vec3 normal; float depth;
			if (QueryVaultContact(scene,entity,p,normal,depth) && depth > 0) p += normal * depth;
			rig.points[j] = glm::vec3(inverse * glm::vec4(p,1));
		}
		capsule = savedCapsule;
		auto& stats = entity->GetComponent<CombatStatsComponent>();
		if (!stats.isDead && rig.downTime >= SparringRig::recoverySeconds)
		{
			glm::vec3 p = world * glm::vec4(rig.points[SparringRig::Hip],1);
			p.y += 1.0f;
			if (!FindVaultLanding(scene,entity,ctl,p,p.y-2.0f)) return;
			transform.position = p; ctl.hasPhysicsPosition = false;
			capsule.isTrigger = rig.savedTrigger; rig.down = rig.initialized = false;
			rig.standingHits = 0; rig.burstRecoil = glm::vec3(0); stats.posture = 0;
			entity->GetComponent<SparringPartnerComponent>().initiativeDelay = .65f;
			if (entity->HasComponent<AnimationComponent>()) entity->GetComponent<AnimationComponent>().boneMatrices = rig.basePalette;
			stats.reactionState = CombatStatsComponent::HitReactionState::None; stats.reactionTimer = 0;
		}
	}

	void PhysicsSystem::UpdateCharacterControllers(Scene& scene, float dt)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<CharacterControllerComponent>() || !entity->HasComponent<TransformComponent>() ||
				!entity->HasComponent<CapsuleColliderComponent>() || !entity->HasComponent<RigidbodyComponent>())
			{
				continue;
			}

			auto& controller = entity->GetComponent<CharacterControllerComponent>();
			auto& transform = entity->GetComponent<TransformComponent>();
			auto& rb = entity->GetComponent<RigidbodyComponent>();
			if (entity->HasComponent<SparringRig>() && entity->GetComponent<SparringRig>().down)
			{
				StepSparringRagdoll(scene, entity, dt); continue;
			}
			if (entity->HasComponent<ExplorerComponent>())
			{
				auto& ai = entity->GetComponent<ExplorerComponent>();
				ObserveEncounter(scene, ai.memory, transform.position, dt, &ai);
				UpdateExplorerInput(scene, ai, controller, transform.position, entity->GetComponent<CapsuleColliderComponent>().radius, dt);
				SteerExplorer(scene, entity, controller, dt);
			}
			else if (entity->HasComponent<SparringPartnerComponent>())
			{
				auto& ai = entity->GetComponent<SparringPartnerComponent>();
				ObserveEncounter(scene, ai.memory, transform.position, dt);
				const bool wasClosingForJab = ai.closingForJab;
				ai.closingForJab = false;
				ai.laneDisplayTime = std::max(0.0f, ai.laneDisplayTime - dt);
				ai.encounterTime += dt;
				const auto previousMode = ai.footwork;
				using Mode = SparringPartnerComponent::Footwork;
				controller.ClearFrameInput();
				auto player = scene.GetEntitySharedByID(ai.playerId);
				if (player && player->HasComponent<TransformComponent>())
				{
					auto delta = player->GetComponent<TransformComponent>().position - transform.position;
					delta.y = 0.0f;
					const float distance = glm::length(delta);
					if (distance > 0.001f) ai.facing = delta / distance;
					if (distance > 3.0f) ai.footwork = Mode::Approach;
					else if (distance < 2.0f) ai.footwork = Mode::Retreat;
					else if ((ai.footwork == Mode::Approach && distance <= 2.7f) ||
						(ai.footwork == Mode::Retreat && distance >= 2.3f)) ai.footwork = Mode::Hold;
					if (ai.footwork != Mode::Hold)
					{
						const glm::vec3 side(-ai.facing.z, 0.0f, ai.facing.x);
						if (ai.memory.loaded && previousMode != ai.footwork)
						{
							const auto left = ai.memory.Familiarity(transform.position + side * 3.0f);
							const auto right = ai.memory.Familiarity(transform.position - side * 3.0f);
							const bool revisit = ai.spacingDecisions++ % 3 == 0;
							ai.sideBias = left == right ? 0.0f : ((left > right) == revisit ? 0.12f : -0.12f);
						}
						controller.moveInput = ai.facing * (ai.footwork == Mode::Approach ? 0.65f : -0.45f) + side * ai.sideBias;
					}
					using Phase = CombatAttackComponent::AttackPhase;
					ai.flankHoldoff = std::max(0.0f, ai.flankHoldoff - dt);
					if (ai.DoubledBack(player->GetComponent<TransformComponent>().position) && ai.flanking)
					{
						ai.flanking = false; ai.route.clear(); ai.replan = ai.escapeTime = 0; ai.flankHoldoff = 0.8f;
						ai.RecordDecision("Player doubled back; abandoned flank");
					}
					const auto phase = player->HasComponent<CombatAttackComponent>() ? player->GetComponent<CombatAttackComponent>().phase : Phase::Idle;
					bool committed = phase == Phase::Active;
					if (phase == Phase::Startup)
					{
						const auto& attack = player->GetComponent<CombatAttackComponent>();
						committed = attack.activeAttackIndex >= 0 && attack.activeAttackIndex < static_cast<int>(attack.attacks.size()) &&
							attack.phaseFrame >= std::max(1,attack.attacks[attack.activeAttackIndex].startupFrames / 5);
					}
					auto& counter = entity->GetComponent<CombatAttackComponent>();
					ai.patternTime = std::max(0.0f, ai.patternTime - dt);
					if (ai.counterWindow > 0 && ai.counterWindow <= dt && !ai.counterSpent)
						ai.counterReason = "Opening expired before a counter was possible";
					ai.counterWindow = std::max(0.0f, ai.counterWindow - dt);
					if (ai.responding && (phase == Phase::Recovery || phase == Phase::Idle))
					{
						ai.responding = false;
						ai.counterWindow = phase == Phase::Recovery ? 1.15f : 0.0f;
						ai.counterReason = ai.counterWindow > 0 ? "Player recovering; closing into hitbox range" : "Windup canceled; no counter opening";
						ai.RecordDecision(ai.counterReason);
					}
					const bool disabled = entity->GetComponent<CombatStatsComponent>().isDead || (player->HasComponent<CombatStatsComponent>() && player->GetComponent<CombatStatsComponent>().isDead);
					const bool evadeSwing = (phase == Phase::Startup || phase == Phase::Active) && distance < 4.5f &&
						std::abs(transform.position.y - player->GetComponent<TransformComponent>().position.y) < 1.4f;
					if (distance < 3.4f && (phase != Phase::Idle || ai.counterWindow > 0)) ai.initiativeDelay = SparringPartnerComponent::openingDelay;
					else ai.initiativeDelay = std::max(0.f, ai.initiativeDelay-dt);
					const bool freeToAct = !disabled && !counter.isAttacking && !counter.attackRequested;
					const glm::vec3 burstLane = freeToAct && controller.enableDodge && controller.isGrounded && !controller.isDodging &&
						controller.dodgeCooldownTimer <= 0 && entity->HasComponent<CombatHurtboxComponent>() ? IncomingBurstLane(scene,*entity) : glm::vec3(0);
					const bool evadeBurst = glm::dot(burstLane,burstLane) > .5f;
					if (disabled || evadeBurst || evadeSwing) ai.EndCombo();
					if (ai.combo >= 0 && freeToAct && (ai.followupTime -= dt) <= 0) { ai.EndCombo(); ai.initiativeDelay = SparringPartnerComponent::openingDelay; }
					const int pattern = ai.combo >= 0 ? ai.combo : ai.Opening(distance >= 3.4f);
					const auto beat = ai.combos[pattern][ai.nextBeat];
					const int slot = std::min(beat.slot,std::max(0,static_cast<int>(counter.attacks.size())-1));
					auto requestBeat = [&]() {
						counter.selectedAttackIndex = slot; counter.attackRequested = true; ai.rangedAttack = beat.burst;
						ai.CommitBeat(pattern); controller.moveInput = glm::vec3(0);
						ai.RecordDecision(beat.burst ? "Combo: winding up a burst" : "Combo: striking in reach");
					};
					if (disabled || counter.isAttacking || counter.attackRequested) controller.moveInput = glm::vec3(0);
					else if (evadeBurst) controller.moveInput = glm::vec3(0);
					else if (!evadeSwing && beat.burst && distance <= 9.f && std::abs(transform.position.y-player->GetComponent<TransformComponent>().position.y) < 3.f && !ai.flanking &&
						player->HasComponent<CombatHurtboxComponent>() && player->GetComponent<CombatHurtboxComponent>().canBeHit && ClearBurstLane(scene,*entity,*player))
					{
						controller.moveInput = glm::vec3(0);
						if (ai.initiativeDelay <= 0 && counter.cooldownTimer <= 0 && !controller.isVaulting && !controller.isDodging && !controller.isSliding)
						{
							requestBeat();
						}
					}
					else if (evadeSwing)
					{
						if (!ai.responding)
						{
							ai.WitnessSwing(player->GetComponent<CombatAttackComponent>().activeAttackIndex);
							ai.dodgeReason = "Watching windup; holding dodge until commitment";
							ai.RecordDecision(ai.dodgeReason);
							ai.evadeSide = ai.ChooseLane();
							ai.counterWindow = 0;
							ai.counterSpent = ai.dodgeIssued = false;
						}
						ai.responding = true;
						controller.moveInput = ai.DodgeLane(1);
					}
					else if (ai.counterWindow > 0 && !ai.counterSpent && !controller.isDodging && (phase == Phase::Recovery || phase == Phase::Idle) && player->HasComponent<CombatHurtboxComponent>())
					{
						controller.moveInput = ai.facing;
						const CombatAttackDefinition fallback;
						const auto& hurt = player->GetComponent<CombatHurtboxComponent>();
						const auto world = TransformHierarchy::GetWorldMatrix(scene, *entity);
						const auto hurtPos = TransformHierarchy::GetWorldMatrix(scene, *player) * glm::vec4(hurt.center, 1);
						const float hitScale = glm::compMax(ExtractWorldScale(scene, *entity));
						const float hurtRadius = hurt.radius * glm::compMax(ExtractWorldScale(scene, *player));
						int recovery = 0;
						if (phase == Phase::Recovery)
						{
							const auto& swing = player->GetComponent<CombatAttackComponent>();
							if (swing.activeAttackIndex >= 0 && swing.activeAttackIndex < static_cast<int>(swing.attacks.size()))
								recovery = swing.attacks[swing.activeAttackIndex].recoveryFrames - swing.phaseFrame;
						}
						int pick = -1;
						const CombatAttackDefinition* best = nullptr;
						for (int i = 0; i < std::max(1, static_cast<int>(counter.attacks.size())); ++i)
						{
							const auto& candidate = counter.attacks.empty() ? fallback : counter.attacks[i];
							const auto hitPos = world * glm::vec4(candidate.hitboxCenter, 1);
							if (glm::distance(hitPos, hurtPos) > candidate.hitboxRadius * hitScale + hurtRadius) continue;
							const bool fits = candidate.startupFrames + 2 <= recovery;
							const bool bestFits = best && best->startupFrames + 2 <= recovery;
							if (!best || (fits && !bestFits) || (fits && bestFits && candidate.damage > best->damage) ||
								(!fits && !bestFits && candidate.startupFrames < best->startupFrames)) { pick = i; best = &candidate; }
						}
						ai.counterReason = !hurt.canBeHit ? "Target cannot be hit" : counter.cooldownTimer > 0 ? "Counter is cooling down" : "Hitboxes out of reach; closing";
						if (counter.cooldownTimer <= 0 && pick >= 0 && hurt.canBeHit)
						{
							counter.selectedAttackIndex = pick;
							ai.counterReason = best->startupFrames + 2 <= recovery ? "Recovery opening: strongest timely counter" : "Short opening: quickest reachable counter";
							ai.RecordDecision(ai.counterReason);
							counter.attackRequested = ai.counterSpent = true;
							ai.counterTrial = {true, false, false, counter.attackActivationCounter + 1u};
							controller.moveInput = glm::vec3(0);
						}
					}
					else if (!beat.burst && phase == Phase::Idle && ai.initiativeDelay <= 0 && controller.isGrounded && !controller.isDodging &&
						!controller.isVaulting && !controller.isSliding && !counter.attacks.empty() && player->HasComponent<CombatHurtboxComponent>())
					{
						const auto& hurt = player->GetComponent<CombatHurtboxComponent>();
						const auto& jab = counter.attacks[slot];
						const auto reach = CombatShapes::WorldSphere(TransformHierarchy::GetWorldMatrix(scene,*entity),jab.hitboxCenter,jab.hitboxRadius);
						const auto body = CombatShapes::WorldSphere(TransformHierarchy::GetWorldMatrix(scene,*player),hurt.center,hurt.radius);
						ai.closingForJab = true;
						controller.moveInput = ai.facing;
						if (hurt.canBeHit && counter.cooldownTimer <= 0 && glm::distance(reach.center,body.center) <= reach.radius + body.radius)
						{
							requestBeat();
						}
					}
					if (ai.flanking && ai.footwork == Mode::Hold && phase == Phase::Idle && !disabled && !counter.isAttacking && !counter.attackRequested && distance > 2.0f)
						controller.moveInput = ai.facing * 0.65f;
					else if (ai.responding || disabled || distance <= 2.0f) ai.flanking = false;
					if (ai.closingForJab != wasClosingForJab) ai.traversal.clearanceTimer = 0;
					SteerPartner(scene, entity, controller, distance, dt);
					if ((evadeBurst || (ai.responding && committed && !ai.dodgeIssued)) && !disabled && !counter.isAttacking && !controller.isDodging && controller.enableDodge && controller.isGrounded && !controller.isVaulting && !controller.isSliding && controller.dodgeCooldownTimer <= 0 && !ai.traversal.unsafeEdge)
					{
						ai.dodgeReason = "No safe dodge landing";
						ai.laneStart = transform.position; ai.laneCount = 0; ai.laneDisplayTime = 0.8f;
						const float stride = std::max(controller.moveSpeed, glm::length(glm::vec2(rb.velocity.x, rb.velocity.z))) * std::clamp(controller.dodgeSpeedMultiplier, 1.2f, 3.5f) * controller.dodgeDuration;
						for (float sign : {1.0f, -1.0f})
						{
							const auto lane = evadeBurst ? burstLane * sign * ai.evadeSide : ai.DodgeLane(sign);
							bool clear = true;
							for (int i = 1; i <= 5 && clear; ++i)
							{
								auto sample = transform.position + lane * (stride * i / 5.0f);
								sample.y += controller.maxStepHeight + 0.1f;
								clear = FindVaultLanding(scene, entity, controller, sample, transform.position.y - controller.maxStepHeight);
							}
							ai.laneProbes[ai.laneCount++] = {ai.laneStart + lane * stride, clear};
							if (!clear) continue;
							controller.moveInput = lane;
							ai.dodgeReason = evadeBurst ? "Incoming burst: dodge across flight path" : ai.repeatCount >= 2 ? "Repeated attack: diagonal dodge toward counter range" : "Nearby attack: lateral dodge";
							ai.RecordDecision(ai.dodgeReason);
							controller.jumpRequested = controller.vaultRequested = false;
							controller.dodgeRequested = true;
							if (evadeBurst) break;
							ai.dodgeIssued = true;
							const auto& swing = player->GetComponent<CombatAttackComponent>();
							ai.actualLane = ai.evadeSide * sign > 0 ? 1 : 0;
							ai.dodgeTrial = {true, false, false, swing.attackActivationCounter + (phase == Phase::Startup ? 1u : 0u)};
							break;
						}
					}
				}
				else ai.footwork = Mode::Hold;
			}
			rb.isKinematic = true;
			controller.previousPhysicsPosition = transform.position;
			controller.hasPhysicsPosition = true;

			controller.wasGrounded = controller.isGrounded;
			const bool wasGroundedLastFrame = controller.wasGrounded;
			const glm::vec3 previousGroundVelocity = controller.groundVelocity;
			controller.isGrounded = false;
			controller.isOnSteepSlope = false;
			controller.groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
			controller.groundVelocity = glm::vec3(0.0f);
			controller.jumpedThisFrame = false;
			controller.wallJumpTriggeredThisFrame = false;

			controller.jumpUngroundedTimer = std::max(0.0f, controller.jumpUngroundedTimer - dt);
			const bool ignoreGrounding = controller.jumpUngroundedTimer > 0.0f;

			glm::vec3 probeNormal(0.0f, 1.0f, 0.0f);
			float probePenetration = 0.0f;
			glm::vec3 supportVelocity(0.0f);
			bool hasSupport = QueryCharacterSupport(scene, entity, probeNormal, probePenetration, &supportVelocity);
			if (!hasSupport && wasGroundedLastFrame && !ignoreGrounding && rb.velocity.y <= 0.0f)
			{
				// Ground snap leaves a skin-width gap. Recheck that gap before consuming a jump press.
				const glm::vec3 originalPosition = transform.position;
				transform.position.y -= std::max(controller.skinWidth, 0.0f) + 0.001f;
				hasSupport = QueryCharacterSupport(scene, entity, probeNormal, probePenetration, &supportVelocity);
				transform.position = originalPosition;
			}
			if (!ignoreGrounding && rb.velocity.y <= 0.0f && hasSupport && IsWalkableSlope(probeNormal, controller.maxSlopeAngleDegrees))
			{
				controller.isGrounded = true;
				controller.wasGrounded = true;
				controller.groundNormal = glm::normalize(probeNormal);
				glm::vec3 targetSupportVelocity(supportVelocity.x, 0.0f, supportVelocity.z);
				const float supportBlend = std::clamp(dt * (wasGroundedLastFrame ? 12.0f : 18.0f), 0.0f, 1.0f);
				controller.groundVelocity = glm::mix(previousGroundVelocity, targetSupportVelocity, supportBlend);
				if (rb.velocity.y < 0.0f)
					rb.velocity.y = 0.0f;
			}
			else if (!ignoreGrounding && rb.velocity.y <= 0.0f && hasSupport)
			{
				controller.isOnSteepSlope = true;
			}

			if (controller.isGrounded)
			{
				transform.position += controller.groundVelocity * dt;
				controller.wallJumpTimer = 0.0f;
				controller.isWallJumping = false;
				controller.wallJumpFallSyncRequested = false;
				controller.wallJumpEstimatedAirTimeRemaining = 0.0f;
				controller.wallRunEndMomentumTimer = 0.0f;
				controller.wallRunEndMomentumSpeed = 0.0f;
				controller.wallRunEndMomentumDirection = glm::vec3(0.0f);
				controller.wallRunAirborneFacingLock = false;
			}

			glm::vec3 moveInput = controller.moveInput;
			if (controller.isGrounded)
				moveInput = FlattenToPlane(moveInput, controller.groundNormal);
			if (glm::length(moveInput) > 1.0f)
				moveInput = glm::normalize(moveInput);

			glm::vec3 horizontalVelocity(rb.velocity.x, 0.0f, rb.velocity.z);
			controller.slideCooldownTimer = std::max(0.0f, controller.slideCooldownTimer - dt);
			controller.slideInputBufferTimer = std::max(0.0f, controller.slideInputBufferTimer - dt);
			controller.dodgeCooldownTimer = std::max(0.0f, controller.dodgeCooldownTimer - dt);
			controller.vaultCooldownTimer = std::max(0.0f, controller.vaultCooldownTimer - dt);
			controller.wallJumpCooldownTimer = std::max(0.0f, controller.wallJumpCooldownTimer - dt);
			controller.wallRunEndMomentumTimer = std::max(0.0f, controller.wallRunEndMomentumTimer - dt);
			const bool wasWallJumping = controller.isWallJumping;
			controller.wallJumpTimer = std::max(0.0f, controller.wallJumpTimer - dt);
			if (controller.isWallJumping && controller.wallJumpTimer <= 0.0f)
				controller.isWallJumping = false;
			if (wasWallJumping && !controller.isWallJumping && !controller.isGrounded && controller.wallJumpFallSyncRequested)
			{
				auto estimateRemainingAirTime = [&]()
				{
					const float gravityAcceleration = std::max(-gravity.y * std::max(controller.gravityScale, 0.0f), 0.001f);
					const float verticalVelocity = rb.velocity.y;
					const glm::vec3 originalPosition = transform.position;
					const float probeStep = 0.1f;
					const float maxProbeDistance = std::max(controller.groundSnapDistance + 8.0f, 8.0f);
					float groundDistance = -1.0f;

					for (float down = 0.0f; down <= maxProbeDistance; down += probeStep)
					{
						transform.position = originalPosition - glm::vec3(0.0f, down, 0.0f);
						glm::vec3 probeNormal(0.0f, 1.0f, 0.0f);
						float probePenetration = 0.0f;
						glm::vec3 probeSupportVelocity(0.0f);
						if (!QueryCharacterSupport(scene, entity, probeNormal, probePenetration, &probeSupportVelocity))
							continue;
						if (!IsWalkableSlope(probeNormal, controller.maxSlopeAngleDegrees))
							continue;
						groundDistance = std::max(0.0f, down - std::max(probePenetration, 0.0f));
						break;
					}

					transform.position = originalPosition;
					if (groundDistance < 0.0f)
						return 0.0f;

					const float discriminant = verticalVelocity * verticalVelocity + 2.0f * gravityAcceleration * groundDistance;
					if (discriminant < 0.0f)
						return 0.0f;

					const float timeToGround = (verticalVelocity + std::sqrt(discriminant)) / gravityAcceleration;
					if (!std::isfinite(timeToGround) || timeToGround <= 0.0f)
						return 0.0f;

					return std::clamp(timeToGround, 0.01f, 3.0f);
				};
				controller.wallJumpEstimatedAirTimeRemaining = estimateRemainingAirTime();
			}
			if (controller.wallJumpFallSyncRequested && !controller.isGrounded && controller.wallJumpEstimatedAirTimeRemaining > 0.0f)
				controller.wallJumpEstimatedAirTimeRemaining = std::max(0.01f, controller.wallJumpEstimatedAirTimeRemaining - dt);
			if (controller.enableSprintSlide && controller.sprintRequested)
				controller.sprintLatched = !controller.sprintLatched;
			controller.sprintRequested = false;
			if (!controller.enableSprintSlide || (glm::length(moveInput) < 0.1f && !controller.isSliding))
				controller.sprintLatched = false;
			if (controller.isSliding)
			{
				controller.slideTimer = std::max(0.0f, controller.slideTimer - dt);
				if (!controller.enableSprintSlide || !controller.isGrounded || controller.jumpRequested || controller.slideTimer <= 0.0f)
				{
					controller.isSliding = false;
					controller.slideCooldownTimer = std::max(controller.slideCooldown, 0.0f);
				}
			}
			if (controller.isDodging)
			{
				controller.dodgeTimer = std::max(0.0f, controller.dodgeTimer - dt);
				if (!controller.enableDodge || !controller.isGrounded || controller.jumpRequested || controller.dodgeTimer <= 0.0f)
				{
					controller.isDodging = false;
					controller.dodgeCooldownTimer = std::max(controller.dodgeCooldown, 0.0f);
				}
			}
			if (controller.enableDodge && controller.dodgeRequested && !controller.isDodging && !controller.isSliding && !controller.isVaulting &&
				controller.isGrounded && !controller.jumpRequested && controller.dodgeCooldownTimer <= 0.0f && controller.dodgeDuration > 0.0f)
			{
				glm::vec3 dodgeDirection = moveInput;
				if (glm::length(dodgeDirection) < 0.1f)
					dodgeDirection = glm::vec3(std::sin(transform.rotation.y), 0.0f, std::cos(transform.rotation.y));
				if (glm::length(dodgeDirection) > 0.0001f)
					dodgeDirection = glm::normalize(dodgeDirection);
				controller.isDodging = true;
				controller.dodgeTimer = controller.dodgeDuration;
				controller.dodgeDirection = dodgeDirection;
				controller.dodgeEntrySpeed = std::max(glm::length(horizontalVelocity), controller.moveSpeed) *
					std::clamp(controller.dodgeSpeedMultiplier, 1.2f, 3.5f);
				controller.isSliding = false;
				controller.slideTimer = 0.0f;
				controller.sprintLatched = false;
				controller.isSprinting = false;
			}
			if (!controller.isDodging && !controller.isSliding)
				TryBeginVault(scene, entity, controller, moveInput);
			if (controller.isVaulting)
			{
				controller.isGrounded = false;
				controller.isSprinting = false;
				controller.isSliding = false;
				controller.isDodging = false;
				controller.slideRequested = false;
				controller.slideInputBufferTimer = 0.0f;
				controller.dodgeRequested = false;
				controller.vaultRequested = false;
				controller.jumpRequested = false;
				UpdateVaultMotion(scene, entity, controller, rb, dt);
				controller.currentSpeed = glm::length(glm::vec2(rb.velocity.x, rb.velocity.z));
				UpdateControllerAnimationState(entity, controller);
				continue;
			}
			const bool hasBufferedSlideInput = controller.slideRequested || controller.slideInputBufferTimer > 0.0f;
			const float horizontalSpeed = glm::length(horizontalVelocity);
			const float sprintSpeedThreshold = controller.moveSpeed * std::max(controller.sprintMultiplier, 1.0f);
			const bool canSlideFromSprint = controller.isSprinting && controller.sprintLatched;
			const bool canSlideFromHighGroundSpeed = controller.isGrounded && horizontalSpeed >= sprintSpeedThreshold;
			if (controller.enableSprintSlide && (canSlideFromSprint || canSlideFromHighGroundSpeed) &&
				glm::length(moveInput) >= 0.1f && hasBufferedSlideInput && !controller.isSliding && !controller.isDodging &&
				controller.isGrounded && !controller.jumpRequested && controller.slideCooldownTimer <= 0.0f &&
				horizontalSpeed >= controller.moveSpeed * 0.8f && controller.slideDuration > 0.0f)
			{
				controller.isSliding = true;
				controller.slideTimer = controller.slideDuration;
				controller.slideDirection = glm::normalize(horizontalVelocity);
				const float slideStartMomentumBoost = std::clamp(controller.slideStartMomentumBoost, 1.0f, 1.25f);
				controller.slideEntrySpeed = std::min(horizontalSpeed * slideStartMomentumBoost,
					controller.moveSpeed * std::max(controller.sprintMultiplier, 1.0f) * slideStartMomentumBoost);
				controller.slideInputBufferTimer = 0.0f;
			}
			controller.slideRequested = false;
			controller.dodgeRequested = false;
			controller.vaultRequested = false;
			controller.isSprinting = controller.enableSprintSlide && controller.sprintLatched &&
				controller.isGrounded && !controller.isSliding && !controller.isDodging && !controller.isVaulting && !controller.crouchHeld;
			const float speedMultiplier = controller.isSprinting ? std::max(controller.sprintMultiplier, 1.0f) : 1.0f;
			glm::vec3 targetHorizontalVelocity = moveInput * controller.moveSpeed * speedMultiplier;
			if (controller.isDodging)
			{
				horizontalVelocity = MoveTowards(horizontalVelocity, controller.dodgeDirection * controller.dodgeEntrySpeed,
					std::max(controller.acceleration, 0.0f) * 2.0f * dt);
			}
			else if (controller.isSliding)
			{
				const float slideSpeed = controller.slideEntrySpeed * std::clamp(controller.slideSpeedMultiplier, 1.0f, 1.25f);
				const float slideFrictionScale = std::clamp(controller.slideGroundFrictionScale, 0.3f, 1.0f);
				horizontalVelocity = MoveTowards(horizontalVelocity, controller.slideDirection * slideSpeed,
					std::max(controller.acceleration, 0.0f) * slideFrictionScale * dt);
			}
			else if (glm::length(moveInput) > 0.0001f)
			{
				float accel = controller.isGrounded
					? controller.acceleration
					: controller.airAcceleration * std::max(controller.airControl, 0.0f);
				horizontalVelocity = MoveTowards(horizontalVelocity, targetHorizontalVelocity, accel * dt);
			}
			else if (!controller.isGrounded && controller.wallRunEndMomentumTimer > 0.0f && controller.wallRunEndMomentumSpeed > 0.0f)
			{
				horizontalVelocity = MoveTowards(horizontalVelocity,
					controller.wallRunEndMomentumDirection * controller.wallRunEndMomentumSpeed,
					std::max(controller.airAcceleration, 0.0f) * 0.4f * dt);
			}
			else
			{
				horizontalVelocity = MoveTowards(horizontalVelocity, glm::vec3(0.0f), controller.braking * dt);
			}
			rb.velocity.x = horizontalVelocity.x;
			rb.velocity.z = horizontalVelocity.z;
			controller.currentSpeed = glm::length(glm::vec2(rb.velocity.x, rb.velocity.z));

			if (controller.jumpRequested && !controller.isDodging && !controller.isVaulting)
			{
				if (controller.isGrounded)
				{
					rb.velocity.y = std::max(rb.velocity.y, controller.jumpSpeed);
					// Nudge upward on takeoff so we clearly separate from ground contact
					// before overlap/snap logic runs again.
					transform.position += controller.groundNormal * (controller.skinWidth * 2.0f + 0.05f);
					controller.capsuleBottomAnimationLift = std::max(controller.capsuleBottomAnimationLift,
						std::max(controller.maxCapsuleBottomAnimationLift, 0.0f));
					controller.isGrounded = false;
					controller.wasGrounded = false;
					controller.jumpedThisFrame = true;
					controller.jumpUngroundedTimer = std::max(controller.jumpUngroundedDuration, 0.0f);
					controller.jumpRequested = false;
				}
			}

			glm::vec3 wallNormal(0.0f);
			bool hasWallContact = false;
			if (hasSupport && !IsWalkableSlope(probeNormal, controller.maxSlopeAngleDegrees) && probePenetration > 0.0001f)
			{
				wallNormal = glm::normalize(probeNormal);
				hasWallContact = glm::length(wallNormal) > 0.0001f;
			}
			glm::vec3 facingDirection = glm::vec3(std::sin(transform.rotation.y), 0.0f, std::cos(transform.rotation.y));
			if (glm::length(moveInput) > 0.1f)
				facingDirection = glm::normalize(FlattenToPlane(moveInput, glm::vec3(0.0f, 1.0f, 0.0f)));
			else if (glm::length(horizontalVelocity) > 0.1f)
				facingDirection = glm::normalize(FlattenToPlane(horizontalVelocity, glm::vec3(0.0f, 1.0f, 0.0f)));

			if (!hasWallContact && controller.enableWallJump)
			{
				const auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
				const float radius = std::max(capsule.radius, 0.001f);
				glm::vec3 probeNormalWall(0.0f, 1.0f, 0.0f);
				float probePenetrationWall = 0.0f;
				const glm::vec3 originalPosition = transform.position;
				for (float distance : { std::max(controller.skinWidth + 0.06f, 0.03f), std::max(radius * 0.25f, 0.05f) })
				{
					transform.position = originalPosition + facingDirection * distance;
					if (QueryCharacterSupport(scene, entity, probeNormalWall, probePenetrationWall) &&
						!IsWalkableSlope(probeNormalWall, controller.maxSlopeAngleDegrees) && probePenetrationWall > 0.0001f)
					{
						wallNormal = glm::normalize(probeNormalWall);
						hasWallContact = glm::length(wallNormal) > 0.0001f;
						break;
					}
				}
				transform.position = originalPosition;
			}

			if (!hasWallContact && controller.enableWallRun)
			{
				const auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
				const float radius = std::max(capsule.radius, 0.001f);
				glm::vec3 probeNormalWall(0.0f, 1.0f, 0.0f);
				float probePenetrationWall = 0.0f;
				const glm::vec3 originalPosition = transform.position;
				for (float distance : { std::max(controller.skinWidth + 0.06f, 0.03f), std::max(radius * 0.35f, 0.075f) })
				{
					transform.position = originalPosition + facingDirection * distance;
					if (QueryCharacterSupport(scene, entity, probeNormalWall, probePenetrationWall) &&
						!IsWalkableSlope(probeNormalWall, controller.maxSlopeAngleDegrees) && probePenetrationWall > 0.0001f)
					{
						wallNormal = glm::normalize(probeNormalWall);
						hasWallContact = glm::length(wallNormal) > 0.0001f;
						break;
					}
				}
				transform.position = originalPosition;
			}

			controller.wallRunCooldownTimer = std::max(0.0f, controller.wallRunCooldownTimer - dt);
			const bool wallRunRequested = controller.wallRunRequested;
			const bool canWallRun = controller.enableWallRun && wallRunRequested && !controller.isGrounded && !controller.isDodging && !controller.isVaulting && !controller.isWallJumping &&
			controller.wallRunCooldownTimer <= 0.0f && controller.wallRunTimer <= 0.0f;
			if (canWallRun && hasWallContact)
			{
				const float wallAngle = glm::degrees(std::acos(glm::clamp(glm::dot(wallNormal, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f)));
				const bool validWall = wallAngle >= controller.wallRunMinWallAngleDegrees && wallAngle <= controller.wallRunMaxWallAngleDegrees;
				const glm::vec3 horizontalVelocity = glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z);
				const float horizontalSpeed = glm::length(horizontalVelocity);
				const bool validSpeed = horizontalSpeed >= std::max(controller.wallRunEntrySpeedThreshold, 0.0f);
				const bool validVerticalSpeed = rb.velocity.y >= controller.wallRunMinVerticalSpeed;
				if (validWall && validSpeed && validVerticalSpeed)
				{
					glm::vec3 wallTangent = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), wallNormal);
					if (glm::length(wallTangent) > 0.0001f)
						wallTangent = glm::normalize(wallTangent);
					if (glm::length(moveInput) > 0.1f)
					{
						glm::vec3 inputAlongWall = FlattenToPlane(moveInput, wallNormal);
						if (glm::length(inputAlongWall) > 0.0001f)
							wallTangent = glm::normalize(inputAlongWall);
					}
					if (glm::length(wallTangent) > 0.0001f)
					{
						controller.isWallRunning = true;
						controller.wallRunNormal = wallNormal;
						controller.wallRunDirection = wallTangent;
						controller.wallRunSide = (glm::dot(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), wallNormal), wallTangent) >= 0.0f) ? 1 : -1;
						controller.mirrorWallRunAnimation = controller.wallRunSide < 0;
						controller.wallRunEntrySpeed = std::max(controller.wallRunSpeed, 0.0f);
						controller.wallRunTimer = std::max(controller.wallRunMaxDuration, 0.0f);
						controller.wallRunEndMomentumTimer = 0.0f;
						controller.wallRunEndMomentumSpeed = 0.0f;
						controller.wallRunEndMomentumDirection = glm::vec3(0.0f);
						controller.wallRunAirborneFacingLock = false;
						controller.wallRunEndRotatePending = false;
						controller.wallRunEndRotateDirection = 0;
						controller.wallRunEndRotateFrameCounter = 0;
						controller.wallRunCommitTimer = std::max(controller.wallRunCommitDuration, 0.0f);
						controller.isGrounded = false;
						controller.wasGrounded = false;
						controller.jumpRequested = false;
						controller.wallRunRequested = false;
						controller.wallRunCooldownTimer = 0.0f;
						controller.wallRunContactGraceTimer = std::max(controller.wallRunContactGraceDuration, 0.0f);
						controller.jumpUngroundedTimer = std::max(controller.wallRunJumpUngroundedDuration, 0.0f);
						controller.wallJumpCooldownTimer = std::max(controller.wallJumpCooldown, 0.0f);
						controller.isWallJumping = false;
						controller.wallJumpFallSyncRequested = false;
						controller.wallJumpEstimatedAirTimeRemaining = 0.0f;
						controller.jumpedThisFrame = false;
						controller.wallJumpTriggeredThisFrame = false;
						controller.capsuleBottomAnimationLift = std::max(controller.capsuleBottomAnimationLift, controller.wallJumpCapsuleLift * 0.85f);
						const float wallClearance = std::max(controller.wallRunWallClearance, 0.0f);
						if (wallClearance > 0.0f)
							transform.position += wallNormal * wallClearance;
					}
				}
				else
				{
					controller.wallRunContactGraceTimer = 0.0f;
					controller.wallRunCommitTimer = 0.0f;
				}
			}

			if (controller.isWallRunning)
			{
				controller.wallRunTimer = std::max(0.0f, controller.wallRunTimer - dt);
				controller.wallRunCommitTimer = std::max(0.0f, controller.wallRunCommitTimer - dt);
				const float wallAngle = hasWallContact
					? glm::degrees(std::acos(glm::clamp(glm::dot(wallNormal, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f)))
					: 0.0f;
				const bool wallAngleValid = hasWallContact &&
					wallAngle >= controller.wallRunMinWallAngleDegrees &&
					wallAngle <= controller.wallRunMaxWallAngleDegrees;
				const float continuityDot = hasWallContact
					? glm::dot(glm::normalize(wallNormal), glm::normalize(controller.wallRunNormal))
					: 1.0f;
				const bool wallContinuityValid = wallAngleValid && continuityDot >= -0.6f;
				const bool inCommitWindow = controller.wallRunCommitTimer > 0.0f;
				if (wallContinuityValid)
				{
					controller.wallRunNormal = wallNormal;
					controller.wallRunContactGraceTimer = std::max(controller.wallRunContactGraceDuration, 0.0f);
				}
				else if (!inCommitWindow)
				{
					controller.wallRunContactGraceTimer = std::max(0.0f, controller.wallRunContactGraceTimer - dt);
				}
				else
				{
					controller.wallRunContactGraceTimer = std::max(controller.wallRunContactGraceTimer, controller.wallRunContactGraceDuration * 0.5f);
				}

				if (controller.wallRunTimer <= 0.0f)
				{
					glm::vec3 pushOff = FlattenToPlane(controller.wallRunNormal, glm::vec3(0.0f, 1.0f, 0.0f));
					if (glm::length(pushOff) <= 0.0001f)
						pushOff = -FlattenToPlane(controller.wallRunDirection, glm::vec3(0.0f, 1.0f, 0.0f));
					if (glm::length(pushOff) > 0.0001f)
						pushOff = glm::normalize(pushOff);

					glm::vec3 carry = FlattenToPlane(controller.wallRunDirection, glm::vec3(0.0f, 1.0f, 0.0f));
					if (glm::length(carry) > 0.0001f)
						carry = glm::normalize(carry);

					const float pushOffSpeed = std::max(controller.wallRunEndPushOffSpeed, 0.0f);
					const float carrySpeed = std::max(controller.wallRunJumpOffSpeed, 0.0f);
					glm::vec3 launchVelocity = pushOff * pushOffSpeed + carry * carrySpeed;
					if (glm::length(launchVelocity) > 0.0001f)
					{
						rb.velocity.x = launchVelocity.x;
						rb.velocity.z = launchVelocity.z;
						controller.wallRunEndMomentumDirection = glm::normalize(launchVelocity);
						controller.wallRunEndMomentumSpeed = glm::length(launchVelocity);
						controller.wallRunEndMomentumTimer = std::max(controller.wallRunEndMomentumDuration, 0.0f);
					}
					rb.velocity.y = std::max(rb.velocity.y, std::max(controller.wallRunEndPushOffUpwardSpeed, 0.0f));
					controller.wallRunAirborneFacingLock = true;
					controller.wallRunEndRotatePending = true;
					controller.wallRunEndRotateDirection = (controller.wallRunSide < 0) ? 1 : -1;
					controller.wallRunEndRotateFrameCounter = std::max(controller.wallRunEndRotateBeginFrame, 0);
					controller.isWallRunning = false;
					controller.wallRunTimer = 0.0f;
					controller.wallRunContactGraceTimer = 0.0f;
					controller.wallRunCommitTimer = 0.0f;
					controller.wallRunCooldownTimer = std::max(controller.wallRunCooldown, 0.0f);
				}
				else
				{
					glm::vec3 wallTangent = FlattenToPlane(controller.wallRunDirection, controller.wallRunNormal);
					if (glm::length(wallTangent) <= 0.0001f)
						wallTangent = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), controller.wallRunNormal);
					if (glm::length(moveInput) > 0.1f)
					{
						glm::vec3 inputAlongWall = FlattenToPlane(moveInput, controller.wallRunNormal);
						if (glm::length(inputAlongWall) > 0.0001f)
							wallTangent = inputAlongWall;
					}
					if (glm::length(wallTangent) > 0.0001f)
						controller.wallRunDirection = glm::normalize(wallTangent);

					rb.velocity.x = controller.wallRunDirection.x * controller.wallRunEntrySpeed;
					rb.velocity.z = controller.wallRunDirection.z * controller.wallRunEntrySpeed;
					rb.velocity.y = std::max(rb.velocity.y, -glm::length(gravity) * controller.wallRunGravityScale * dt);
					controller.currentSpeed = glm::length(glm::vec2(rb.velocity.x, rb.velocity.z));
				}
			}
			const bool recentWallRunJumpWindow = !controller.isGrounded && controller.wallRunCooldownTimer > 0.0f && glm::length(controller.wallRunNormal) > 0.0001f;
			const bool wallJumpSurfaceAvailable = hasWallContact || controller.isWallRunning || recentWallRunJumpWindow;
			if (controller.jumpRequested && controller.enableWallJump && wallJumpSurfaceAvailable && controller.wallJumpCooldownTimer <= 0.0f)
			{
				glm::vec3 jumpWallNormal = hasWallContact ? wallNormal : controller.wallRunNormal;
				if (glm::length(jumpWallNormal) > 0.0001f)
					jumpWallNormal = glm::normalize(jumpWallNormal);
				const float facingDot = glm::dot(glm::normalize(facingDirection), -jumpWallNormal);
				const bool bypassFacingCheck = controller.isWallRunning || recentWallRunJumpWindow;
				if (bypassFacingCheck || facingDot >= std::clamp(controller.wallJumpFacingDotThreshold, 0.0f, 0.95f))
				{
					glm::vec3 incoming = glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z);
					if (glm::length(incoming) < 0.1f)
						incoming = facingDirection;
					else
						incoming = glm::normalize(incoming);
					glm::vec3 reflected = glm::reflect(incoming, jumpWallNormal);
					reflected.y = 0.0f;
					if (glm::length(reflected) < 0.0001f || glm::dot(reflected, jumpWallNormal) < 0.1f)
						reflected = FlattenToPlane(jumpWallNormal, glm::vec3(0.0f, 1.0f, 0.0f));
					if (glm::length(reflected) > 0.0001f)
						reflected = glm::normalize(reflected);
					rb.velocity.x = reflected.x * std::max(controller.wallJumpHorizontalSpeed, 0.0f);
					rb.velocity.z = reflected.z * std::max(controller.wallJumpHorizontalSpeed, 0.0f);
					rb.velocity.y = std::max(rb.velocity.y, controller.wallJumpVerticalSpeed);
					const glm::vec3 towardWall(-jumpWallNormal.x, 0.0f, -jumpWallNormal.z);
					if (glm::length(towardWall) > 0.0001f)
						transform.rotation.y = std::atan2(towardWall.x, towardWall.z);
					controller.jumpedThisFrame = true;
					controller.wallJumpTriggeredThisFrame = true;
					controller.wallJumpFallSyncRequested = true;
					controller.wallJumpEstimatedAirTimeRemaining = 0.0f;
					controller.isWallJumping = true;
					controller.wallJumpTimer = std::max(controller.wallJumpStateDuration, 0.0f);
					controller.wallJumpCooldownTimer = std::max(controller.wallJumpCooldown, 0.0f);
					controller.jumpUngroundedTimer = std::max(controller.wallRunJumpUngroundedDuration, 0.0f);
					controller.jumpRequested = false;
					controller.wallRunRequested = false;
					controller.isWallRunning = false;
					controller.wallRunTimer = 0.0f;
					controller.wallRunContactGraceTimer = 0.0f;
					controller.wallRunCommitTimer = 0.0f;
					controller.wallRunCooldownTimer = 0.0f;
					controller.mirrorWallRunAnimation = false;
					controller.wallRunAirborneFacingLock = false;
					controller.wallRunEndRotatePending = false;
					controller.wallRunEndRotateDirection = 0;
					controller.wallRunEndRotateFrameCounter = 0;
				}
			}

			if (controller.jumpRequested && !controller.jumpHeld)
			{
				// If the button has been released before grounding, clear stale request.
				controller.jumpRequested = false;
			}
			if (!controller.isGrounded)
			{
				if (!controller.isWallRunning)
					rb.velocity += gravity * controller.gravityScale * dt;
				else
					rb.velocity += gravity * controller.gravityScale * controller.wallRunGravityScale * dt;
			}

			if (controller.isOnSteepSlope)
			{
				glm::vec3 slideDirection = FlattenToPlane(gravity, probeNormal);
				if (glm::length(slideDirection) > 0.0001f)
					rb.velocity += glm::normalize(slideDirection) * (glm::length(gravity) * controller.slideGravityScale * dt);
			}

			glm::vec3 horizontalDisplacement(rb.velocity.x * dt, 0.0f, rb.velocity.z * dt);
			if (entity->HasComponent<SparringRig>())
			{
				auto& recoil = entity->GetComponent<SparringRig>().burstRecoil;
				horizontalDisplacement += glm::vec3(recoil.x,0,recoil.z) * dt;
				recoil *= std::exp(-8.0f * dt);
			}
			if (glm::length(horizontalDisplacement) > 0.0001f)
			{
				glm::vec3 startPosition = transform.position;
				transform.position += horizontalDisplacement;

				glm::vec3 moveNormal(0.0f, 1.0f, 0.0f);
				float movePenetration = 0.0f;
				if (QueryCharacterSupport(scene, entity, moveNormal, movePenetration))
				{
					bool blockingHit = !IsWalkableSlope(moveNormal, controller.maxSlopeAngleDegrees);
					if (blockingHit)
					{
						transform.position = startPosition;
						if (!TryStepUp(scene, entity, controller, rb, horizontalDisplacement))
						{
							transform.position += horizontalDisplacement;
							ResolveCharacterOverlaps(scene, entity, controller, rb);
						}
					}
					else
					{
						ResolveCharacterOverlaps(scene, entity, controller, rb);
					}
				}
			}

			transform.position += glm::vec3(0.0f, rb.velocity.y * dt, 0.0f);
			ResolveCharacterOverlaps(scene, entity, controller, rb);

			if (controller.isGrounded)
				controller.groundVelocity = glm::vec3(controller.groundVelocity.x, 0.0f, controller.groundVelocity.z);
			else
			{
				glm::vec3 airVelocity = glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z);
				if (wasGroundedLastFrame)
				{
					const float stepOffBlend = std::clamp(dt * 8.0f, 0.0f, 1.0f);
					controller.groundVelocity = glm::mix(previousGroundVelocity, airVelocity, stepOffBlend);
				}
				else
				{
					controller.groundVelocity = airVelocity;
				}
			}

			if (controller.enableGroundSnap && !controller.jumpRequested && controller.jumpUngroundedTimer <= 0.0f && rb.velocity.y <= 0.0f)
			{
				glm::vec3 originalPosition = transform.position;
				transform.position -= glm::vec3(0.0f, controller.groundSnapDistance, 0.0f);
				glm::vec3 snapNormal(0.0f, 1.0f, 0.0f);
				float snapPenetration = 0.0f;
				glm::vec3 snapSupportVelocity(0.0f);
				if (QueryCharacterSupport(scene, entity, snapNormal, snapPenetration, &snapSupportVelocity) && IsWalkableSlope(snapNormal, controller.maxSlopeAngleDegrees))
				{
					transform.position += snapNormal * (snapPenetration + controller.skinWidth);
					controller.isGrounded = true;
					controller.groundNormal = glm::normalize(snapNormal);
					glm::vec3 snapSupportHorizontal(snapSupportVelocity.x, 0.0f, snapSupportVelocity.z);
					const float snapBlend = std::clamp(dt * 16.0f, 0.0f, 1.0f);
					controller.groundVelocity = glm::mix(controller.groundVelocity, snapSupportHorizontal, snapBlend);
					controller.isOnSteepSlope = false;
					rb.velocity.y = 0.0f;
				}
				else
				{
					transform.position = originalPosition;
				}
			}

			glm::vec3 facing = FlattenToPlane(glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z), glm::vec3(0.0f, 1.0f, 0.0f));
			bool animationInWallRunState = false;
			if (entity->HasComponent<AnimationStateMachineComponent>())
			{
				auto& sm = entity->GetComponent<AnimationStateMachineComponent>();
				if (sm.stateMachine && sm.stateMachine->IsValidStateIndex(sm.currentStateIndex))
					animationInWallRunState = sm.stateMachine->states[sm.currentStateIndex].name == "WallRun";
			}
			if (controller.wallRunEndRotatePending && !animationInWallRunState)
			{
				if (controller.wallRunEndRotateFrameCounter > 0)
				{
					--controller.wallRunEndRotateFrameCounter;
				}
				else
				{
					const float endTurnOffset = static_cast<float>(controller.wallRunEndRotateDirection) *
						glm::radians(controller.wallRunEndDirectionAngleDegrees);
					const float newYaw = transform.rotation.y + endTurnOffset;
					transform.rotation.y = std::atan2(std::sin(newYaw), std::cos(newYaw));
					controller.wallRunEndRotatePending = false;
					controller.wallRunEndRotateDirection = 0;
				}
			}
			const bool inWallRunKickoffWindow = animationInWallRunState || controller.isWallRunning ||
				(!controller.isGrounded &&
					(controller.wallRunEndMomentumTimer > 0.0f || controller.wallRunCooldownTimer > 0.0f));
			if (entity->HasComponent<ExplorerComponent>() && (entity->GetComponent<ExplorerComponent>().waiting || entity->GetComponent<ExplorerComponent>().inspectTime > 0.0f))
				facing = entity->GetComponent<ExplorerComponent>().lookDirection;
			const bool spacingPartner = entity->HasComponent<SparringPartnerComponent>();
			if (spacingPartner) facing = entity->GetComponent<SparringPartnerComponent>().facing;
			if ((controller.orientToMovement || spacingPartner) && !controller.isWallJumping && !inWallRunKickoffWindow && !controller.wallRunAirborneFacingLock && glm::length(facing) > 0.1f)
			{
				facing = glm::normalize(facing);
				// TransformComponent rotation uses radians; use +Z-forward yaw convention
				// so character-facing aligns better with imported humanoid animation sets.
				const float targetYaw = std::atan2(facing.x, facing.z);
				if (controller.enableSprintSlide)
				{
					const float yawDelta = std::atan2(std::sin(targetYaw - transform.rotation.y), std::cos(targetYaw - transform.rotation.y));
					const float maxTurn = std::max(controller.turnSpeed, 0.0f) * dt;
					transform.rotation.y += std::clamp(yawDelta, -maxTurn, maxTurn);
				}
				else
					transform.rotation.y = targetYaw;
			}

			if (!controller.isGrounded)
			{
				controller.isSprinting = false;
				if (controller.isSliding)
					controller.slideCooldownTimer = std::max(controller.slideCooldown, 0.0f);
				controller.isSliding = false;
				if (controller.isDodging)
					controller.dodgeCooldownTimer = std::max(controller.dodgeCooldown, 0.0f);
				controller.isDodging = false;
				if (controller.isVaulting)
					controller.vaultCooldownTimer = std::max(controller.vaultCooldown, 0.0f);
				controller.isVaulting = false;
			}
			UpdateControllerAnimationState(entity, controller);
		}
	}

	void PhysicsSystem::SolveJoints(Scene& scene, float dt)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<JointComponent>() || !entity->HasComponent<TransformComponent>())
				continue;

			auto& joint = entity->GetComponent<JointComponent>();
			if (!joint.enabled)
				continue;

			auto& transformA = entity->GetComponent<TransformComponent>();

			// Resolve the connected side: either another entity's rigidbody/transform
			// or a fixed point in world space (connectedEntityID == 0).
			std::shared_ptr<Entity> connected;
			if (joint.connectedEntityID != 0)
			{
				connected = TransformHierarchy::FindEntityByID(scene, joint.connectedEntityID);
				if (!connected || !connected->HasComponent<TransformComponent>())
					continue;
			}

			glm::vec3 anchorA = transformA.position + joint.anchor;
			glm::vec3 anchorB = connected
				? connected->GetComponent<TransformComponent>().position + joint.connectedAnchor
				: joint.connectedAnchor;

			glm::vec3 delta = anchorB - anchorA;
			float distance = glm::length(delta);
			if (distance < 1e-6f)
				continue;
			glm::vec3 dir = delta / distance;

			// Determine mobility of each side (kinematic/static bodies don't move).
			bool hasRbA = entity->HasComponent<RigidbodyComponent>();
			bool hasRbB = connected && connected->HasComponent<RigidbodyComponent>();

			bool movableA = hasRbA && !entity->GetComponent<RigidbodyComponent>().isKinematic;
			bool movableB = hasRbB && !connected->GetComponent<RigidbodyComponent>().isKinematic;

			// If neither side can move there's nothing to solve.
			if (!movableA && !movableB)
				continue;

			float invMassA = movableA ? 1.0f / std::max(entity->GetComponent<RigidbodyComponent>().mass, 0.0001f) : 0.0f;
			float invMassB = movableB ? 1.0f / std::max(connected->GetComponent<RigidbodyComponent>().mass, 0.0001f) : 0.0f;
			float invMassSum = invMassA + invMassB;
			if (invMassSum <= 0.0f)
				continue;

			switch (joint.type)
			{
				case JointType::Fixed:
				{
					// Pull both anchor points fully together (weighted by inverse mass).
					glm::vec3 correction = delta;
					if (movableA)
					{
						transformA.position += correction * (invMassA / invMassSum);
						if (hasRbA)
						{
							auto& rbA = entity->GetComponent<RigidbodyComponent>();
							rbA.velocity -= glm::dot(rbA.velocity, dir) * dir;
						}
					}
					if (movableB)
					{
						auto& transformB = connected->GetComponent<TransformComponent>();
						transformB.position -= correction * (invMassB / invMassSum);
						if (hasRbB)
						{
							auto& rbB = connected->GetComponent<RigidbodyComponent>();
							rbB.velocity -= glm::dot(rbB.velocity, dir) * dir;
						}
					}
					break;
				}

				case JointType::Spring:
				{
					// Hooke's law: F = -k * (distance - restLength) * dir, plus damping
					// along the spring axis to avoid perpetual oscillation.
					float stretch = distance - joint.restLength;
					glm::vec3 force = dir * (joint.stiffness * stretch);

					glm::vec3 velA = (hasRbA) ? entity->GetComponent<RigidbodyComponent>().velocity : glm::vec3(0.0f);
					glm::vec3 velB = (hasRbB) ? connected->GetComponent<RigidbodyComponent>().velocity : glm::vec3(0.0f);
					glm::vec3 relativeVel = velB - velA;
					force += dir * (joint.damping * glm::dot(relativeVel, dir));

					if (movableA)
					{
						auto& rbA = entity->GetComponent<RigidbodyComponent>();
						rbA.velocity += (force / std::max(rbA.mass, 0.0001f)) * dt;
					}
					if (movableB)
					{
						auto& rbB = connected->GetComponent<RigidbodyComponent>();
						rbB.velocity -= (force / std::max(rbB.mass, 0.0001f)) * dt;
					}
					break;
				}

				case JointType::Hinge:
				{
					// Ball-and-socket pivot: only correct once the body strays beyond
					// hingeDistance from the anchor, otherwise allow free swing.
					if (distance <= joint.hingeDistance)
						break;

					float excess = distance - joint.hingeDistance;
					glm::vec3 correction = dir * excess;

					if (movableA)
					{
						transformA.position += correction * (invMassA / invMassSum);
						if (hasRbA)
						{
							auto& rbA = entity->GetComponent<RigidbodyComponent>();
							float radialSpeed = glm::dot(rbA.velocity, dir);
							if (radialSpeed < 0.0f)
								rbA.velocity -= radialSpeed * dir;
						}
					}
					if (movableB)
					{
						auto& transformB = connected->GetComponent<TransformComponent>();
						transformB.position -= correction * (invMassB / invMassSum);
						if (hasRbB)
						{
							auto& rbB = connected->GetComponent<RigidbodyComponent>();
							float radialSpeed = glm::dot(rbB.velocity, -dir);
							if (radialSpeed < 0.0f)
								rbB.velocity -= radialSpeed * (-dir);
						}
					}
					break;
				}
			}
		}
	}

	// Shared response logic for a dynamic body resting/colliding against a static plane.
	static void ResolvePlaneContact(RigidbodyComponent& rb, glm::vec3& position, const glm::vec3& normal, float penetration)
	{
		// Use slop + partial correction to avoid over-correcting at rest, which
		// is a common source of visible micro-jitter on flat ground.
		const float penetrationSlop = 0.002f;
		const float correctionPercent = 0.8f;
		if (penetration > penetrationSlop)
		{
			float correction = (penetration - penetrationSlop) * correctionPercent;
			position += normal * correction;
		}

		float velAlongNormal = glm::dot(rb.velocity, normal);
		if (velAlongNormal < 0.0f)
		{
			float impactSpeed = -velAlongNormal;
			const float restImpactThreshold = 0.25f;
			const bool lowBounce = rb.bounciness < 0.05f;

			// For low-speed contacts on non-bouncy bodies, kill normal velocity
			// instead of reflecting; this keeps bodies stable on the plane.
			if (impactSpeed < restImpactThreshold && lowBounce)
			{
				rb.velocity -= velAlongNormal * normal;

				// Light tangential damping while grounded to remove tiny residual
				// shaking without making collisions feel sticky.
				glm::vec3 tangentVel = rb.velocity - glm::dot(rb.velocity, normal) * normal;
				rb.velocity -= tangentVel * 0.15f;
			}
			else
			{
				rb.velocity -= (1.0f + rb.bounciness) * velAlongNormal * normal;
			}

			if (glm::length(rb.velocity) < 0.05f && lowBounce)
				rb.velocity = glm::vec3(0.0f);
		}
		else if (std::abs(velAlongNormal) < 0.01f)
		{
			// Already separating or nearly at rest: remove normal drift.
			rb.velocity -= velAlongNormal * normal;
		}
	}

	void PhysicsSystem::DetectAndResolveCollisions(Scene& scene)
	{
		// Pair keys (packed entity IDs) seen this frame, used to diff against
		// last frame's active sets so Enter/Exit events fire exactly once.
		std::unordered_set<uint64_t> newCollisionPairs;
		std::unordered_set<uint64_t> newTriggerPairs;
		std::unordered_map<uint64_t, std::pair<std::shared_ptr<Entity>, std::shared_ptr<Entity>>> pairEntities;

		// --- Dynamic bodies vs plane colliders (spheres and boxes) ---
		for (const auto& dynamicEntity : scene.GetEntities())
		{
			if (!dynamicEntity || !dynamicEntity->HasComponent<RigidbodyComponent>() ||
				!dynamicEntity->HasComponent<TransformComponent>())
				continue;

			auto& rb = dynamicEntity->GetComponent<RigidbodyComponent>();
			auto& transform = dynamicEntity->GetComponent<TransformComponent>();
			glm::vec3 worldScale = ExtractWorldScale(scene, *dynamicEntity);

			// Skip kinematic objects (they don't collide with planes)
			if (rb.isKinematic)
				continue;

			bool hasSphere = dynamicEntity->HasComponent<BoundingSphereComponent>();
			bool hasBox = dynamicEntity->HasComponent<BoxColliderComponent>();
			bool hasCapsule = dynamicEntity->HasComponent<CapsuleColliderComponent>();

			if (!hasSphere && !hasBox && !hasCapsule)
				continue;

			// Check against all plane colliders
				for (const auto& planeEntity : scene.GetEntities())
				{
					if (!planeEntity || !planeEntity->HasComponent<PlaneColliderComponent>() ||
						!planeEntity->HasComponent<TransformComponent>())
						continue;

					// Collision layer filter
					if (!CollisionMatrix::CanCollide(
						static_cast<int>(dynamicEntity->GetLayer()),
						static_cast<int>(planeEntity->GetLayer())))
						continue;

					auto& plane = planeEntity->GetComponent<PlaneColliderComponent>();

				collisionChecks++;

				glm::vec3 normal;
				float penetration;
				bool hit = false;
				bool isTriggerPair = plane.isTrigger;

				// Prefer box collider, then capsule, then sphere when multiple are present
				if (hasBox)
				{
					auto& box = dynamicEntity->GetComponent<BoxColliderComponent>();
					glm::vec3 boxCenter = transform.position + box.center * worldScale;
					glm::vec3 boxHalfExtents = box.halfExtents * worldScale;
					isTriggerPair = isTriggerPair || box.isTrigger;
					hit = CheckBoxPlaneCollision(boxCenter, boxHalfExtents, plane.normal, plane.distance, normal, penetration);
				}
				else if (hasCapsule)
				{
					auto& capsule = dynamicEntity->GetComponent<CapsuleColliderComponent>();
					glm::vec3 segA = transform.position + capsule.pointA * worldScale;
					glm::vec3 segB = transform.position + capsule.pointB * worldScale;
					float capsuleRadius = capsule.radius * glm::compMax(glm::vec2(worldScale.x, worldScale.z));
					isTriggerPair = isTriggerPair || capsule.isTrigger;
					hit = CheckCapsulePlaneCollision(segA, segB, capsuleRadius, plane.normal, plane.distance, normal, penetration);
				}
				else if (hasSphere)
				{
					auto& bs = dynamicEntity->GetComponent<BoundingSphereComponent>();
					float sphereRadius = bs.radius * glm::compMax(worldScale);
					isTriggerPair = isTriggerPair || bs.isTrigger;
					hit = CheckSpherePlaneCollision(transform.position + bs.center * worldScale, sphereRadius, plane.normal, plane.distance, normal, penetration);
				}

				if (hit)
				{
					uint64_t key = MakePairKey(dynamicEntity->GetID(), planeEntity->GetID());
					pairEntities[key] = { dynamicEntity, planeEntity };

					if (isTriggerPair)
					{
						newTriggerPairs.insert(key);
					}
					else
					{
						newCollisionPairs.insert(key);
						collisionsDetected++;
						ResolvePlaneContact(rb, transform.position, normal, penetration);
					}
				}
			}
		}

		// --- Collect all dynamic-collider entities (sphere, box, or capsule) for pairwise checks ---
		// Priority when an entity has multiple collider components: box > capsule > sphere,
		// matching the priority used for the vs-plane pass above.
		std::vector<std::shared_ptr<Entity>> pairwiseEntities;
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<RigidbodyComponent>() || !entity->HasComponent<TransformComponent>())
				continue;

			if (entity->HasComponent<BoxColliderComponent>() ||
				entity->HasComponent<CapsuleColliderComponent>() ||
				entity->HasComponent<BoundingSphereComponent>())
			{
				pairwiseEntities.push_back(entity);
			}
		}

		// --- Broad-phase: bucket entities into a uniform spatial hash grid using
		// their world-space AABB, then only run narrow-phase checks on entities
		// that share (or neighbor) a grid cell. This avoids O(n^2) checks when
		// entities are spread out across the scene. ---
		broadphaseGrid.cellSize = broadphaseCellSize;
		broadphaseGrid.Clear();
		for (const auto& entity : pairwiseEntities)
		{
			glm::vec3 aabbMin, aabbMax;
			if (ComputeColliderAABB(scene, entity, aabbMin, aabbMax))
				broadphaseGrid.InsertAABB(entity, aabbMin, aabbMax);
		}

		auto dispatchNarrowPhase = [this, &scene, &newCollisionPairs, &newTriggerPairs, &pairEntities](const std::shared_ptr<Entity>& entityA, const std::shared_ptr<Entity>& entityB)
			{
				auto& rbA = entityA->GetComponent<RigidbodyComponent>();
				auto& rbB = entityB->GetComponent<RigidbodyComponent>();

				collisionChecks++;

				// Skip if both are kinematic
				if (rbA.isKinematic && rbB.isKinematic)
					return;

				// Collision layer filter
				if (!CollisionMatrix::CanCollide(
					static_cast<int>(entityA->GetLayer()),
					static_cast<int>(entityB->GetLayer())))
					return;

			auto& transformA = entityA->GetComponent<TransformComponent>();
			auto& transformB = entityB->GetComponent<TransformComponent>();
			glm::vec3 worldScaleA = ExtractWorldScale(scene, *entityA);
			glm::vec3 worldScaleB = ExtractWorldScale(scene, *entityB);

			bool boxA = entityA->HasComponent<BoxColliderComponent>();
			bool capsuleA = !boxA && entityA->HasComponent<CapsuleColliderComponent>();
			bool sphereA = !boxA && !capsuleA && entityA->HasComponent<BoundingSphereComponent>();

			bool boxB = entityB->HasComponent<BoxColliderComponent>();
			bool capsuleB = !boxB && entityB->HasComponent<CapsuleColliderComponent>();
			bool sphereB = !boxB && !capsuleB && entityB->HasComponent<BoundingSphereComponent>();

			bool triggerA = (boxA && entityA->GetComponent<BoxColliderComponent>().isTrigger) ||
				(capsuleA && entityA->GetComponent<CapsuleColliderComponent>().isTrigger) ||
				(sphereA && entityA->GetComponent<BoundingSphereComponent>().isTrigger);
			bool triggerB = (boxB && entityB->GetComponent<BoxColliderComponent>().isTrigger) ||
				(capsuleB && entityB->GetComponent<CapsuleColliderComponent>().isTrigger) ||
				(sphereB && entityB->GetComponent<BoundingSphereComponent>().isTrigger);
			bool isTriggerPair = triggerA || triggerB;

			glm::vec3 normal;
			float penetration;
			bool hit = false;

			if (boxA && boxB)
			{
				auto& a = entityA->GetComponent<BoxColliderComponent>();
				auto& b = entityB->GetComponent<BoxColliderComponent>();
				hit = CheckBoxBoxCollision(
					transformA.position + a.center * worldScaleA, a.halfExtents * worldScaleA,
					transformB.position + b.center * worldScaleB, b.halfExtents * worldScaleB,
					normal, penetration);
			}
			else if (boxA && sphereB)
			{
				auto& a = entityA->GetComponent<BoxColliderComponent>();
				auto& b = entityB->GetComponent<BoundingSphereComponent>();
				hit = CheckBoxSphereCollision(
					transformA.position + a.center * worldScaleA, a.halfExtents * worldScaleA,
					transformB.position + b.center * worldScaleB, b.radius * glm::compMax(worldScaleB),
					normal, penetration);
			}
			else if (sphereA && boxB)
			{
				auto& a = entityA->GetComponent<BoundingSphereComponent>();
				auto& b = entityB->GetComponent<BoxColliderComponent>();
				// Normal must point from A to B; box-sphere returns normal from box toward sphere,
				// so swap operands (box = A-role) then flip the resulting normal.
				hit = CheckBoxSphereCollision(
					transformB.position + b.center * worldScaleB, b.halfExtents * worldScaleB,
					transformA.position + a.center * worldScaleA, a.radius * glm::compMax(worldScaleA),
					normal, penetration);
				if (hit) normal = -normal;
			}
			else if (sphereA && sphereB)
			{
				auto& a = entityA->GetComponent<BoundingSphereComponent>();
				auto& b = entityB->GetComponent<BoundingSphereComponent>();
				hit = CheckSphereSphereCollision(
					transformA.position + a.center * worldScaleA, a.radius * glm::compMax(worldScaleA),
					transformB.position + b.center * worldScaleB, b.radius * glm::compMax(worldScaleB),
					normal, penetration);
			}
			else if (capsuleA && capsuleB)
			{
				auto& a = entityA->GetComponent<CapsuleColliderComponent>();
				auto& b = entityB->GetComponent<CapsuleColliderComponent>();
				hit = CheckCapsuleCapsuleCollision(
					transformA.position + a.pointA * worldScaleA, transformA.position + a.pointB * worldScaleA, a.radius * glm::compMax(glm::vec2(worldScaleA.x, worldScaleA.z)),
					transformB.position + b.pointA * worldScaleB, transformB.position + b.pointB * worldScaleB, b.radius * glm::compMax(glm::vec2(worldScaleB.x, worldScaleB.z)),
					normal, penetration);
			}
			else if (capsuleA && sphereB)
			{
				auto& a = entityA->GetComponent<CapsuleColliderComponent>();
				auto& b = entityB->GetComponent<BoundingSphereComponent>();
				hit = CheckCapsuleSphereCollision(
					transformA.position + a.pointA * worldScaleA, transformA.position + a.pointB * worldScaleA, a.radius * glm::compMax(glm::vec2(worldScaleA.x, worldScaleA.z)),
					transformB.position + b.center * worldScaleB, b.radius * glm::compMax(worldScaleB),
					normal, penetration);
			}
			else if (sphereA && capsuleB)
			{
				auto& a = entityA->GetComponent<BoundingSphereComponent>();
				auto& b = entityB->GetComponent<CapsuleColliderComponent>();
				hit = CheckCapsuleSphereCollision(
					transformB.position + b.pointA * worldScaleB, transformB.position + b.pointB * worldScaleB, b.radius * glm::compMax(glm::vec2(worldScaleB.x, worldScaleB.z)),
					transformA.position + a.center * worldScaleA, a.radius * glm::compMax(worldScaleA),
					normal, penetration);
				if (hit) normal = -normal;
			}
			else if (capsuleA && boxB)
			{
				auto& a = entityA->GetComponent<CapsuleColliderComponent>();
				auto& b = entityB->GetComponent<BoxColliderComponent>();
				hit = CheckCapsuleBoxCollision(
					transformA.position + a.pointA * worldScaleA, transformA.position + a.pointB * worldScaleA, a.radius * glm::compMax(glm::vec2(worldScaleA.x, worldScaleA.z)),
					transformB.position + b.center * worldScaleB, b.halfExtents * worldScaleB,
					normal, penetration);
			}
			else if (boxA && capsuleB)
			{
				auto& a = entityA->GetComponent<BoxColliderComponent>();
				auto& b = entityB->GetComponent<CapsuleColliderComponent>();
				hit = CheckCapsuleBoxCollision(
					transformB.position + b.pointA * worldScaleB, transformB.position + b.pointB * worldScaleB, b.radius * glm::compMax(glm::vec2(worldScaleB.x, worldScaleB.z)),
					transformA.position + a.center * worldScaleA, a.halfExtents * worldScaleA,
					normal, penetration);
				if (hit) normal = -normal;
			}

			if (hit)
			{
				uint64_t key = MakePairKey(entityA->GetID(), entityB->GetID());
				pairEntities[key] = { entityA, entityB };

				if (isTriggerPair)
				{
					newTriggerPairs.insert(key);
				}
				else
				{
					newCollisionPairs.insert(key);
					collisionsDetected++;
					ResolveSphereCollision(
						transformA.position, rbA.velocity, rbA.mass, rbA.bounciness, rbA.isKinematic,
						transformB.position, rbB.velocity, rbB.mass, rbB.bounciness, rbB.isKinematic,
						normal, penetration
					);
				}
			}
		};

		if (enableBroadphase)
		{
			broadphaseGrid.ForEachCandidatePair(dispatchNarrowPhase);
		}
		else
		{
			// Fallback: brute-force all pairs (useful for debugging/correctness comparisons)
			for (size_t i = 0; i < pairwiseEntities.size(); ++i)
			{
				for (size_t j = i + 1; j < pairwiseEntities.size(); ++j)
				{
					dispatchNarrowPhase(pairwiseEntities[i], pairwiseEntities[j]);
				}
			}
		}

		// --- Dynamic bodies vs mesh colliders ---
			// We iterate every entity with a RigidbodyComponent + a sphere or capsule
			// collider against every static MeshColliderComponent entity.
			for (const auto& dynamicEntity : scene.GetEntities())
			{
				if (!dynamicEntity || !dynamicEntity->HasComponent<RigidbodyComponent>() ||
					!dynamicEntity->HasComponent<TransformComponent>())
					continue;

				auto& rb        = dynamicEntity->GetComponent<RigidbodyComponent>();
				auto& transform = dynamicEntity->GetComponent<TransformComponent>();
				if (rb.isKinematic)
					continue;

				glm::vec3 worldScale = ExtractWorldScale(scene, *dynamicEntity);

				bool hasSphere  = dynamicEntity->HasComponent<BoundingSphereComponent>();
				bool hasCapsule = dynamicEntity->HasComponent<CapsuleColliderComponent>();
				if (!hasSphere && !hasCapsule)
					continue;

				for (const auto& meshEntity : scene.GetEntities())
				{
					if (!meshEntity || meshEntity.get() == dynamicEntity.get())
						continue;
					if (!meshEntity->HasComponent<MeshColliderComponent>() ||
						!meshEntity->HasComponent<TransformComponent>())
						continue;

					// Layer filter
					if (!CollisionMatrix::CanCollide(
						static_cast<int>(dynamicEntity->GetLayer()),
						static_cast<int>(meshEntity->GetLayer())))
						continue;

					auto& meshCol       = meshEntity->GetComponent<MeshColliderComponent>();
					auto& meshTransform = meshEntity->GetComponent<TransformComponent>();
					glm::vec3 meshScale = ExtractWorldScale(scene, *meshEntity);

					collisionChecks++;

					// Test against each triangle
					for (const auto& tri : meshCol.triangles)
					{
						// Transform triangle vertices into world space
						glm::vec3 tA = meshTransform.position + tri[0] * meshScale;
						glm::vec3 tB = meshTransform.position + tri[1] * meshScale;
						glm::vec3 tC = meshTransform.position + tri[2] * meshScale;

						glm::vec3 normal;
						float     penetration;
						bool      hit = false;

						if (hasCapsule)
						{
							auto& cap = dynamicEntity->GetComponent<CapsuleColliderComponent>();
							glm::vec3 segA = transform.position + cap.pointA * worldScale;
							glm::vec3 segB = transform.position + cap.pointB * worldScale;
							float     r    = cap.radius * glm::compMax(glm::vec2(worldScale.x, worldScale.z));
							hit = CheckCapsuleMeshCollision(segA, segB, r, tA, tB, tC, normal, penetration);
						}
						else
						{
							auto& bs = dynamicEntity->GetComponent<BoundingSphereComponent>();
							float r  = bs.radius * glm::compMax(worldScale);
							hit = CheckSphereMeshCollision(transform.position + bs.center * worldScale, r, tA, tB, tC, normal, penetration);
						}

						if (!hit)
							continue;

						uint64_t key = MakePairKey(dynamicEntity->GetID(), meshEntity->GetID());
						pairEntities[key] = { dynamicEntity, meshEntity };

						if (meshCol.isTrigger)
						{
							newTriggerPairs.insert(key);
						}
						else
						{
							newCollisionPairs.insert(key);
							collisionsDetected++;
							ResolvePlaneContact(rb, transform.position, normal, penetration);
						}
					}
				}
			}

			// --- Diff this frame's pairs against last frame's to fire Enter/Exit events ---
		for (const auto& key : newCollisionPairs)
		{
			if (activeCollisionPairs.find(key) == activeCollisionPairs.end())
			{
				auto& pair = pairEntities[key];
				FireEnterEvent(pair.first, pair.second, false);
			}
		}
		for (const auto& key : activeCollisionPairs)
		{
			if (newCollisionPairs.find(key) == newCollisionPairs.end())
			{
				auto it = pairEntities.find(key);
				if (it != pairEntities.end())
					FireExitEvent(it->second.first, it->second.second, false);
			}
		}

		for (const auto& key : newTriggerPairs)
		{
			if (activeTriggerPairs.find(key) == activeTriggerPairs.end())
			{
				auto& pair = pairEntities[key];
				FireEnterEvent(pair.first, pair.second, true);
			}
		}
		for (const auto& key : activeTriggerPairs)
		{
			if (newTriggerPairs.find(key) == newTriggerPairs.end())
			{
				auto it = pairEntities.find(key);
				if (it != pairEntities.end())
					FireExitEvent(it->second.first, it->second.second, true);
			}
		}

		activeCollisionPairs = std::move(newCollisionPairs);
		activeTriggerPairs = std::move(newTriggerPairs);
	}

	void PhysicsSystem::ProcessCombatHitboxes(Scene& scene)
	{
		std::vector<std::shared_ptr<Entity>> activeHitboxes;
		std::vector<std::shared_ptr<Entity>> hurtboxes;
		activeHitboxes.reserve(scene.GetEntities().size());
		hurtboxes.reserve(scene.GetEntities().size());

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<TransformComponent>())
				continue;

			if (entity->HasComponent<CombatHurtboxComponent>())
			{
				auto& hurtbox = entity->GetComponent<CombatHurtboxComponent>();
				hurtbox.wasHitThisStep = false;
				hurtbox.damageTakenThisStep = 0.0f;
				hurtbox.lastHitByEntityId = 0;
				hurtbox.lastHitActivationId = 0;
				hurtboxes.push_back(entity);
			}

			if (entity->HasComponent<CombatHitboxComponent>())
			{
				const auto& hitbox = entity->GetComponent<CombatHitboxComponent>();
				if (hitbox.active && hitbox.radius > 0.0f)
					activeHitboxes.push_back(entity);
			}
		}

		for (const auto& attacker : activeHitboxes)
		{
			auto& hitbox = attacker->GetComponent<CombatHitboxComponent>();
			if (attacker->HasComponent<SparringRig>() && attacker->GetComponent<SparringRig>().down) continue;
			const int slot = attacker->HasComponent<CombatAttackComponent>() ? attacker->GetComponent<CombatAttackComponent>().activeAttackIndex : -1;
			const auto strikes = CombatShapes::SampleStrikes(scene,*attacker,slot,hitbox.center,hitbox.radius);

			if (strikes.spheres[0].radius <= 0.0f)
				continue;

			for (const auto& target : hurtboxes)
			{
				if (!target || target.get() == attacker.get())
					continue;

				auto& hurtbox = target->GetComponent<CombatHurtboxComponent>();
				if (!hurtbox.canBeHit || hurtbox.radius <= 0.0f)
					continue;
				if (target->HasComponent<SparringRig>() && target->GetComponent<CombatStatsComponent>().isDead) continue;

				if (hitbox.team == hurtbox.team)
					continue;

				int region = -1;
				float nearest = std::numeric_limits<float>::max();
				CombatShapes::ForEachHurtSphere(scene,*target,hurtbox,[&](int j, const CombatShapes::Sphere& sphere) {
					for (int side = 0; side < strikes.count; ++side)
					{
						const auto& strike = strikes.spheres[side];
						const auto delta = sphere.center - strike.center;
						const float d = glm::dot(delta,delta), reach = sphere.radius + strike.radius;
						if (d <= reach * reach && d < nearest) { nearest = d; region = j; }
					}
				});
				if (nearest == std::numeric_limits<float>::max()) continue;
				if (attacker->HasComponent<PlayerStrikeBones>())
				{
					const auto& press = attacker->GetComponent<PlayerStrikeBones>().meleePress;
					if (press && press->target == target->GetID())
					{
						if (press->damageApplied) continue;
						press->damageApplied = true;
					}
				}

				const uint64_t directedPairKey = (static_cast<uint64_t>(attacker->GetID()) << 32) |
					static_cast<uint64_t>(target->GetID());
				if (hitbox.singleHitPerActivation)
				{
					auto it = lastCombatHitActivationByPair.find(directedPairKey);
					if (it != lastCombatHitActivationByPair.end() && it->second == hitbox.activationId)
						continue;
					lastCombatHitActivationByPair[directedPairKey] = hitbox.activationId;
				}

				hurtbox.wasHitThisStep = true;
				hurtbox.damageTakenThisStep += hitbox.damage;
				hurtbox.lastHitByEntityId = attacker->GetID();
				hurtbox.lastHitActivationId = hitbox.activationId;
				if (region >= 0)
				{
					auto& rig = target->GetComponent<SparringRig>();
					float posture = hitbox.damage;
					if (attacker->HasComponent<CombatAttackComponent>())
					{
						const auto& attack = attacker->GetComponent<CombatAttackComponent>();
						if (attack.activeAttackIndex >= 0 && attack.activeAttackIndex < static_cast<int>(attack.attacks.size())) posture = attack.attacks[attack.activeAttackIndex].postureDamage;
					}
					rig.pendingDamage += hitbox.damage; rig.pendingPosture += posture;
					rig.CountHit(); rig.lastAttacker = attacker->GetID(); rig.lastRegion = region;
					const auto origin = TransformHierarchy::GetWorldMatrix(scene,*attacker)[3];
					rig.Push(glm::vec3(TransformHierarchy::GetWorldMatrix(scene,*target)[3]-origin),posture >= 20.f ? 8.f : 4.f);
				}
				if (target->HasComponent<SparringPartnerComponent>())
				{
					auto& ai = target->GetComponent<SparringPartnerComponent>();
					if (ai.playerId == attacker->GetID() && ai.dodgeTrial.pending && ai.dodgeTrial.activation == hitbox.activationId) ai.dodgeTrial.hit = true;
				}
				if (attacker->HasComponent<SparringPartnerComponent>())
				{
					auto& ai = attacker->GetComponent<SparringPartnerComponent>();
					if (ai.playerId == target->GetID() && ai.counterTrial.pending && ai.counterTrial.activation == hitbox.activationId) ai.counterTrial.hit = true;
				}
			}
		}
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<SparringPartnerComponent>()) continue;
			auto& ai = entity->GetComponent<SparringPartnerComponent>();
			auto settle = [&](SparringPartnerComponent::Trial& trial, const std::shared_ptr<Entity>& source, bool dodge) {
				if (!trial.pending) return;
				if (!source || !source->HasComponent<CombatAttackComponent>() || !source->HasComponent<CombatHitboxComponent>()) { trial = {}; return; }
				const auto& box = source->GetComponent<CombatHitboxComponent>();
				const auto& attack = source->GetComponent<CombatAttackComponent>();
				if (box.active && box.activationId == trial.activation) trial.active = true;
				using Phase = CombatAttackComponent::AttackPhase;
				if (attack.phase == Phase::Recovery || (attack.phase == Phase::Idle && !attack.attackRequested)) ai.FinishTrial(trial,dodge);
			};
			if (ai.dodgeTrial.pending) settle(ai.dodgeTrial, scene.GetEntitySharedByID(ai.playerId), true);
			settle(ai.counterTrial, entity, false);
		}
	}

	void PhysicsSystem::UpdateSparringBursts(Scene& scene, float dt)
	{
		std::vector<uint32_t> expired;
		for (const auto& e : scene.GetEntities())
		{
			if (!e || !e->HasComponent<SparringEnergyBurst>()) continue;
			auto& burst = e->GetComponent<SparringEnergyBurst>();
			auto target = scene.GetEntitySharedByID(burst.target);
			if (!target || !target->HasComponent<CombatHurtboxComponent>() || (!target->HasComponent<SparringRig>() && !target->HasComponent<PlayerStrikeBones>())) { expired.push_back(e->GetID()); continue; }
			auto regionCenter = [&](int j) {
				return glm::vec3(TransformHierarchy::GetWorldMatrix(scene,*target) * glm::vec4(target->GetComponent<SparringRig>().Pose(*target)[j],1));
			};
			if (burst.fade >= 0)
			{
				burst.fade -= dt;
				UpdateBurstLight(e->GetComponent<LightComponent>(),burst,e->GetComponent<TransformComponent>().position);
				if (burst.fade <= 0) expired.push_back(e->GetID());
				else if (burst.glowParticle >= 0)
					e->GetComponent<ParticleEmitterComponent>().particles[burst.glowParticle].position = regionCenter(burst.impactRegion);
				continue;
			}
			auto& pos = e->GetComponent<TransformComponent>().position;
			Ray ray{pos,burst.direction};
			float distance = std::min(burst.remaining,dt * burst.speed);
			bool blocked = false;
			for (const auto& obstacle : scene.GetEntities())
			{
				if (!obstacle || obstacle->GetID() == burst.owner || obstacle == target || !obstacle->HasComponent<BoxColliderComponent>()) continue;
				if (BurstBoxContact(scene,*obstacle,ray,distance)) blocked = true;
			}
			auto& hurt = target->GetComponent<CombatHurtboxComponent>();
			int region = -1;
			bool bodyContact = false;
			if (hurt.canBeHit && hurt.team != burst.team)
				CombatShapes::ForEachHurtSphere(scene,*target,hurt,[&](int j, const CombatShapes::Sphere& sphere) {
					float t;
					if (RayIntersectsSphere(ray,sphere.center,sphere.radius+burst.radius,t) && (t < distance || (!blocked && t == distance))) { distance = t; region = j; bodyContact = true; }
				});
			pos += ray.direction * distance; burst.remaining -= distance;
			burst.impactFlash = bodyContact || blocked;
			UpdateBurstLight(e->GetComponent<LightComponent>(),burst,pos);
			if (bodyContact && region < 0 && !burst.press->damageApplied && !target->GetComponent<CombatStatsComponent>().isDead)
			{
				auto& incoming = target->GetComponent<PlayerStrikeBones>();
				incoming.incomingBurstDamage += burst.damage; incoming.incomingBurstPosture += burst.posture;
				incoming.incomingBurstOwner = burst.owner; burst.press->damageApplied = true;
			}
			if (region >= 0)
			{
				target->GetComponent<SparringRig>().Push(burst.direction,burst.posture >= 20.f ? 11.f : 6.f);
				if (!target->GetComponent<CombatStatsComponent>().isDead && !burst.press->damageApplied)
				{
					burst.press->damageApplied = true;
					hurt.wasHitThisStep = true; hurt.damageTakenThisStep += burst.damage;
					hurt.lastHitByEntityId = burst.owner; hurt.lastHitActivationId = burst.activation;
					auto& rig = target->GetComponent<SparringRig>();
					rig.pendingDamage += burst.damage; rig.pendingPosture += burst.posture;
					rig.CountHit(); rig.lastAttacker = burst.owner; rig.lastRegion = region;
				}
			}
			auto& fx = e->GetComponent<ParticleEmitterComponent>();
			const bool heavy = burst.posture >= 20;
			if (bodyContact || blocked || burst.remaining <= 0)
			{
				fx.emitting = false; burst.fade = heavy ? .28f : .22f;
				if (bodyContact || blocked)
				{
					if (region >= 0)
					{
						burst.impactRegion = region;
						// Reserve a slot even when the trail fills the pool. Keep it alive until cleanup.
						burst.glowParticle = static_cast<int>(fx.particles.size())-1;
						auto& glow = fx.particles.back(); glow = Particle{}; glow.alive = true;
						glow.position = regionCenter(region); glow.sizeStart = 2.6f*SparringRig::radius[region]; glow.sizeEnd = .08f;
						glow.colorStart = {.5f,1,1,.9f}; glow.colorEnd = {.12f,.8f,1,0}; glow.lifetime = burst.fade;
					}
					EmitPressure(fx,pos,burst.direction,heavy,PressureStage::Impact,burst.remaining);
					if (heavy) EmitSparringRing(fx,pos,burst.direction,true);
					fx.burstRequestCount = heavy ? 18 : 8; fx.spreadAngle = heavy ? 175.f : 50.f; fx.emitSpeed = 5;
				}
			}
			else if ((burst.ringTimer -= dt) <= 0)
			{
				EmitPressure(fx,pos,burst.direction,heavy,PressureStage::Wake,burst.remaining);
				if (heavy) EmitSparringRing(fx,pos,burst.direction,false);
				burst.ringTimer = .06f;
			}
		}
		for (auto id : expired) scene.DestroyEntity(id);
	}

	uint64_t PhysicsSystem::MakePairKey(uint32_t idA, uint32_t idB)
	{
		// Order-independent key: pack the smaller ID into the high bits.
		if (idA > idB)
			std::swap(idA, idB);
		return (static_cast<uint64_t>(idA) << 32) | static_cast<uint64_t>(idB);
	}

	void PhysicsSystem::FireEnterEvent(const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b, bool isTrigger)
	{
		if (!a || !b)
			return;

		if (a->HasComponent<CollisionEventsComponent>())
		{
			auto& events = a->GetComponent<CollisionEventsComponent>();
			auto& callback = isTrigger ? events.onTriggerEnter : events.onCollisionEnter;
			if (callback)
				callback(b);
			auto& scriptCallback = isTrigger ? events.onScriptTriggerEnter : events.onScriptCollisionEnter;
			if (scriptCallback)
				scriptCallback(b);
		}
		if (b->HasComponent<CollisionEventsComponent>())
		{
			auto& events = b->GetComponent<CollisionEventsComponent>();
			auto& callback = isTrigger ? events.onTriggerEnter : events.onCollisionEnter;
			if (callback)
				callback(a);
			auto& scriptCallback = isTrigger ? events.onScriptTriggerEnter : events.onScriptCollisionEnter;
			if (scriptCallback)
				scriptCallback(a);
		}
	}

	void PhysicsSystem::FireExitEvent(const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b, bool isTrigger)
	{
		if (!a || !b)
			return;

		if (a->HasComponent<CollisionEventsComponent>())
		{
			auto& events = a->GetComponent<CollisionEventsComponent>();
			auto& callback = isTrigger ? events.onTriggerExit : events.onCollisionExit;
			if (callback)
				callback(b);
			auto& scriptCallback = isTrigger ? events.onScriptTriggerExit : events.onScriptCollisionExit;
			if (scriptCallback)
				scriptCallback(b);
		}
		if (b->HasComponent<CollisionEventsComponent>())
		{
			auto& events = b->GetComponent<CollisionEventsComponent>();
			auto& callback = isTrigger ? events.onTriggerExit : events.onCollisionExit;
			if (callback)
				callback(a);
			auto& scriptCallback = isTrigger ? events.onScriptTriggerExit : events.onScriptCollisionExit;
			if (scriptCallback)
				scriptCallback(a);
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

	bool PhysicsSystem::CheckBoxPlaneCollision(
		const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
		const glm::vec3& planeNormal, float planeDistance,
		glm::vec3& outNormal, float& outPenetration)
	{
		// Project the box's half-extents onto the plane normal to find the
		// "radius" of the box along the normal direction (support mapping).
		float projectedRadius =
			boxHalfExtents.x * std::abs(planeNormal.x) +
			boxHalfExtents.y * std::abs(planeNormal.y) +
			boxHalfExtents.z * std::abs(planeNormal.z);

		float distToPlane = glm::dot(boxCenter, planeNormal) - planeDistance;

		if (distToPlane < projectedRadius)
		{
			outNormal = planeNormal;
			outPenetration = projectedRadius - distToPlane;
			return true;
		}

		return false;
	}

	bool PhysicsSystem::CheckBoxBoxCollision(
		const glm::vec3& centerA, const glm::vec3& halfExtentsA,
		const glm::vec3& centerB, const glm::vec3& halfExtentsB,
		glm::vec3& outNormal, float& outPenetration)
	{
		// Axis-aligned box overlap test (SAT on the 3 world axes)
		glm::vec3 delta = centerB - centerA;
		glm::vec3 overlap = (halfExtentsA + halfExtentsB) - glm::abs(delta);

		// If there's no overlap on any axis, the boxes aren't colliding
		if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
			return false;

		// Find axis of minimum penetration (least overlap = separating axis for MTV)
		if (overlap.x < overlap.y && overlap.x < overlap.z)
		{
			outNormal = glm::vec3(delta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
			outPenetration = overlap.x;
		}
		else if (overlap.y < overlap.z)
		{
			outNormal = glm::vec3(0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
			outPenetration = overlap.y;
		}
		else
		{
			outNormal = glm::vec3(0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f);
			outPenetration = overlap.z;
		}

		return true;
	}

	bool PhysicsSystem::CheckBoxSphereCollision(
		const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
		const glm::vec3& spherePos, float sphereRadius,
		glm::vec3& outNormal, float& outPenetration) const
	{
		// Find the closest point on the AABB to the sphere center
		glm::vec3 boxMin = boxCenter - boxHalfExtents;
		glm::vec3 boxMax = boxCenter + boxHalfExtents;
		glm::vec3 closestPoint = glm::clamp(spherePos, boxMin, boxMax);

		glm::vec3 delta = spherePos - closestPoint;
		float distSq = glm::dot(delta, delta);

		if (distSq < sphereRadius * sphereRadius)
		{
			float dist = std::sqrt(distSq);

			if (dist < 0.0001f)
			{
				// Sphere center is inside the box; push out along smallest penetration axis
				glm::vec3 toMin = spherePos - boxMin;
				glm::vec3 toMax = boxMax - spherePos;
				float minPen = toMin.x;
				outNormal = glm::vec3(-1.0f, 0.0f, 0.0f);
				if (toMax.x < minPen) { minPen = toMax.x; outNormal = glm::vec3(1.0f, 0.0f, 0.0f); }
				if (toMin.y < minPen) { minPen = toMin.y; outNormal = glm::vec3(0.0f, -1.0f, 0.0f); }
				if (toMax.y < minPen) { minPen = toMax.y; outNormal = glm::vec3(0.0f, 1.0f, 0.0f); }
				if (toMin.z < minPen) { minPen = toMin.z; outNormal = glm::vec3(0.0f, 0.0f, -1.0f); }
				if (toMax.z < minPen) { minPen = toMax.z; outNormal = glm::vec3(0.0f, 0.0f, 1.0f); }
				outPenetration = minPen + sphereRadius;
			}
			else
			{
				outNormal = delta / dist;
				outPenetration = sphereRadius - dist;
			}

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

				// ------------------------------------------------------------
				// Capsule collision detection
				// ------------------------------------------------------------

				glm::vec3 PhysicsSystem::ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& segA, const glm::vec3& segB)
				{
		glm::vec3 ab = segB - segA;
		float abLenSq = glm::dot(ab, ab);
		if (abLenSq < 0.0000001f)
			return segA;

		float t = glm::dot(point - segA, ab) / abLenSq;
		t = glm::clamp(t, 0.0f, 1.0f);
		return segA + ab * t;
	}

	void PhysicsSystem::ClosestPointsBetweenSegments(
		const glm::vec3& p1, const glm::vec3& q1,
		const glm::vec3& p2, const glm::vec3& q2,
		glm::vec3& outC1, glm::vec3& outC2)
	{
		glm::vec3 d1 = q1 - p1;
		glm::vec3 d2 = q2 - p2;
		glm::vec3 r = p1 - p2;

		float a = glm::dot(d1, d1);
		float e = glm::dot(d2, d2);
		float f = glm::dot(d2, r);

		float s, t;

		const float epsilon = 0.0000001f;
		if (a <= epsilon && e <= epsilon)
		{
			// Both segments degenerate to points
			outC1 = p1;
			outC2 = p2;
			return;
		}

		if (a <= epsilon)
		{
			// First segment is a point
			s = 0.0f;
			t = glm::clamp(f / e, 0.0f, 1.0f);
		}
		else
		{
			float c = glm::dot(d1, r);
			if (e <= epsilon)
			{
				// Second segment is a point
				t = 0.0f;
				s = glm::clamp(-c / a, 0.0f, 1.0f);
			}
			else
			{
				float b = glm::dot(d1, d2);
				float denom = a * e - b * b;

				if (denom != 0.0f)
					s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
				else
					s = 0.0f;

				t = (b * s + f) / e;

				if (t < 0.0f)
				{
					t = 0.0f;
					s = glm::clamp(-c / a, 0.0f, 1.0f);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = glm::clamp((b - c) / a, 0.0f, 1.0f);
				}
			}
		}

		outC1 = p1 + d1 * s;
		outC2 = p2 + d2 * t;
	}

	bool PhysicsSystem::CheckCapsulePlaneCollision(
		const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
		const glm::vec3& planeNormal, float planeDistance,
		glm::vec3& outNormal, float& outPenetration) const
	{
		// Find whichever end of the capsule segment is deepest into the plane
		float distA = glm::dot(segA, planeNormal) - planeDistance;
		float distB = glm::dot(segB, planeNormal) - planeDistance;
		float minDist = std::min(distA, distB);

		// Treat near-contact as support to avoid grounded/airborne oscillation,
		// but only produce positive penetration when actually interpenetrating.
		constexpr float kSupportTolerance = 0.03f;
		if (minDist <= (capsuleRadius + kSupportTolerance))
		{
			outNormal = planeNormal;
			outPenetration = std::max(0.0f, capsuleRadius - minDist);
			return true;
		}

		return false;
	}

	bool PhysicsSystem::CheckCapsuleSphereCollision(
		const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
		const glm::vec3& spherePos, float sphereRadius,
		glm::vec3& outNormal, float& outPenetration) const
	{
		glm::vec3 closest = ClosestPointOnSegment(spherePos, segA, segB);
		glm::vec3 delta = spherePos - closest;
		float distSq = glm::dot(delta, delta);
		float radiusSum = capsuleRadius + sphereRadius;

		if (distSq < radiusSum * radiusSum)
		{
			float dist = std::sqrt(distSq);
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

	bool PhysicsSystem::CheckCapsuleBoxCollision(
		const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
		const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
		glm::vec3& outNormal, float& outPenetration) const
	{
		// Approximate by finding the closest point on the capsule segment to the box,
		// then treating that point as a sphere center for a box-sphere test.
		glm::vec3 boxMin = boxCenter - boxHalfExtents;
		glm::vec3 boxMax = boxCenter + boxHalfExtents;

		// Clamp several samples along the segment to the box and pick the closest,
		// which is a good approximation of the true closest point on the segment to the AABB.
		glm::vec3 bestPoint = segA;
		float bestDistSq = std::numeric_limits<float>::max();
		const int sampleCount = 8;
		for (int i = 0; i <= sampleCount; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(sampleCount);
			glm::vec3 samplePoint = segA + (segB - segA) * t;
			glm::vec3 clamped = glm::clamp(samplePoint, boxMin, boxMax);
			glm::vec3 diff = samplePoint - clamped;
			float distSq = glm::dot(diff, diff);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestPoint = samplePoint;
			}
		}

		return CheckBoxSphereCollision(boxCenter, boxHalfExtents, bestPoint, capsuleRadius, outNormal, outPenetration);
	}

	bool PhysicsSystem::CheckCapsuleCapsuleCollision(
		const glm::vec3& segA0, const glm::vec3& segA1, float radiusA,
		const glm::vec3& segB0, const glm::vec3& segB1, float radiusB,
		glm::vec3& outNormal, float& outPenetration) const
	{
		glm::vec3 closestA, closestB;
		ClosestPointsBetweenSegments(segA0, segA1, segB0, segB1, closestA, closestB);

		glm::vec3 delta = closestB - closestA;
		float distSq = glm::dot(delta, delta);
		float radiusSum = radiusA + radiusB;

		if (distSq < radiusSum * radiusSum)
		{
			float dist = std::sqrt(distSq);
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

	// ------------------------------------------------------------
	// Broad-phase: uniform spatial hash grid
	// ------------------------------------------------------------

	long long PhysicsSystem::BroadphaseGrid::HashCell(int x, int y, int z)
	{
		// Combine three 21-bit-ish signed coordinates into a single 64-bit key.
		// Offset by a large constant so negative coordinates stay positive.
		const long long offset = 1 << 20;
		long long xi = static_cast<long long>(x) + offset;
		long long yi = static_cast<long long>(y) + offset;
		long long zi = static_cast<long long>(z) + offset;
		return (xi & 0x1FFFFF) | ((yi & 0x1FFFFF) << 21) | ((zi & 0x1FFFFF) << 42);
	}

	void PhysicsSystem::BroadphaseGrid::Clear()
	{
		cells.clear();
		smallEntries.clear();
		oversizedEntries.clear();
	}

	void PhysicsSystem::BroadphaseGrid::InsertAABB(const std::shared_ptr<Entity>& entity, const glm::vec3& aabbMin, const glm::vec3& aabbMax)
	{
		float inv = 1.0f / cellSize;
		int minX = static_cast<int>(std::floor(aabbMin.x * inv));
		int minY = static_cast<int>(std::floor(aabbMin.y * inv));
		int minZ = static_cast<int>(std::floor(aabbMin.z * inv));
		int maxX = static_cast<int>(std::floor(aabbMax.x * inv));
		int maxY = static_cast<int>(std::floor(aabbMax.y * inv));
		int maxZ = static_cast<int>(std::floor(aabbMax.z * inv));

		// Large floors/walls otherwise allocate thousands of grid entries per fixed step.
		const double cellCount = (static_cast<double>(maxX) - minX + 1.0) *
			(static_cast<double>(maxY) - minY + 1.0) * (static_cast<double>(maxZ) - minZ + 1.0);
		if (cellCount > 64.0)
		{
			oversizedEntries.push_back({ entity, aabbMin, aabbMax });
			return;
		}
		smallEntries.push_back({ entity, aabbMin, aabbMax });

		for (int x = minX; x <= maxX; ++x)
		{
			for (int y = minY; y <= maxY; ++y)
			{
				for (int z = minZ; z <= maxZ; ++z)
				{
					cells[HashCell(x, y, z)].push_back(entity);
				}
			}
		}
	}

	void PhysicsSystem::BroadphaseGrid::ForEachCandidatePair(const std::function<void(const std::shared_ptr<Entity>&, const std::shared_ptr<Entity>&)>& callback) const
	{
		// De-duplicate pairs since an entity spanning multiple cells can be
		// re-encountered against the same neighbor from different cells.
		std::unordered_map<long long, bool> seenPairs;
		seenPairs.reserve(64);

		auto emitPair = [&](const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b)
		{
			auto pa = reinterpret_cast<std::uintptr_t>(a.get());
			auto pb = reinterpret_cast<std::uintptr_t>(b.get());
			if (pa == pb)
				return;
			std::uintptr_t lo = std::min(pa, pb);
			std::uintptr_t hi = std::max(pa, pb);
			long long key = static_cast<long long>((lo * 2654435761u) ^ (hi * 2246822519u));
			if (!seenPairs.emplace(key, true).second)
				return;
			if (lo == pa)
				callback(a, b);
			else
				callback(b, a);
		};

		for (const auto& cellEntry : cells)
		{
			const auto& entities = cellEntry.second;
			for (size_t i = 0; i < entities.size(); ++i)
			{
				for (size_t j = i + 1; j < entities.size(); ++j)
				{
					emitPair(entities[i], entities[j]);
				}
			}
		}

		auto overlaps = [](const BoundsEntry& a, const BoundsEntry& b)
		{
			return a.min.x <= b.max.x && a.max.x >= b.min.x &&
				a.min.y <= b.max.y && a.max.y >= b.min.y &&
				a.min.z <= b.max.z && a.max.z >= b.min.z;
		};
		for (size_t i = 0; i < oversizedEntries.size(); ++i)
		{
			const auto& large = oversizedEntries[i];
			for (const auto& small : smallEntries)
				if (overlaps(large, small))
					emitPair(large.entity, small.entity);
			for (size_t j = i + 1; j < oversizedEntries.size(); ++j)
				if (overlaps(large, oversizedEntries[j]))
					emitPair(large.entity, oversizedEntries[j].entity);
		}
	}

	bool PhysicsSystem::ComputeColliderAABB(const Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outMin, glm::vec3& outMax) const
	{
		if (!entity || !entity->HasComponent<TransformComponent>())
			return false;

		auto& transform = entity->GetComponent<TransformComponent>();
		glm::vec3 worldScale = ExtractWorldScale(scene, *entity);

		if (entity->HasComponent<BoxColliderComponent>())
		{
			auto& box = entity->GetComponent<BoxColliderComponent>();
			glm::vec3 center = transform.position + box.center * worldScale;
			glm::vec3 halfExtents = box.halfExtents * worldScale;
			outMin = center - halfExtents;
			outMax = center + halfExtents;
			return true;
		}

		if (entity->HasComponent<CapsuleColliderComponent>())
		{
			auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
			glm::vec3 a = transform.position + capsule.pointA * worldScale;
			glm::vec3 b = transform.position + capsule.pointB * worldScale;
			float scaledRadius = capsule.radius * glm::compMax(glm::vec2(worldScale.x, worldScale.z));
			glm::vec3 r(scaledRadius);
			outMin = glm::min(a, b) - r;
			outMax = glm::max(a, b) + r;
			return true;
		}

		if (entity->HasComponent<BoundingSphereComponent>())
		{
			auto& sphere = entity->GetComponent<BoundingSphereComponent>();
			glm::vec3 center = transform.position + sphere.center * worldScale;
			float scaledRadius = sphere.radius * glm::compMax(worldScale);
			glm::vec3 r(scaledRadius);
			outMin = center - r;
			outMax = center + r;
			return true;
		}

		if (entity->HasComponent<MeshColliderComponent>())
		{
			auto& mesh = entity->GetComponent<MeshColliderComponent>();
			if (mesh.triangles.empty())
				return false;
			outMin = transform.position + mesh.localAABBMin * worldScale;
			outMax = transform.position + mesh.localAABBMax * worldScale;
			// Ensure min <= max after scale sign flip
			for (int i = 0; i < 3; ++i)
			{
				if (outMin[i] > outMax[i]) std::swap(outMin[i], outMax[i]);
			}
			return true;
		}

		return false;
	}

	// ------------------------------------------------------------
	// Mesh collision helpers
	// ------------------------------------------------------------

	glm::vec3 PhysicsSystem::ClosestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
	{
		// Christer Ericson "Real-Time Collision Detection" §5.1.5
		glm::vec3 ab = b - a;
		glm::vec3 ac = c - a;
		glm::vec3 ap = p - a;

		float d1 = glm::dot(ab, ap);
		float d2 = glm::dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f)
			return a;

		glm::vec3 bp = p - b;
		float d3 = glm::dot(ab, bp);
		float d4 = glm::dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3)
			return b;

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			float v = d1 / (d1 - d3);
			return a + v * ab;
		}

		glm::vec3 cp = p - c;
		float d5 = glm::dot(ab, cp);
		float d6 = glm::dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6)
			return c;

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			float w = d2 / (d2 - d6);
			return a + w * ac;
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + w * (c - b);
		}

		float denom = 1.0f / (va + vb + vc);
		float v = vb * denom;
		float w = vc * denom;
		return a + v * ab + w * ac;
	}

	bool PhysicsSystem::CheckSphereMeshCollision(
		const glm::vec3& sphereCenter, float sphereRadius,
		const glm::vec3& triA, const glm::vec3& triB, const glm::vec3& triC,
		glm::vec3& outNormal, float& outPenetration) const
	{
		glm::vec3 closest = ClosestPointOnTriangle(sphereCenter, triA, triB, triC);
		glm::vec3 delta   = sphereCenter - closest;
		float     dist2   = glm::dot(delta, delta);

		if (dist2 >= sphereRadius * sphereRadius)
			return false;

		float dist = std::sqrt(dist2);
		if (dist < 1e-6f)
		{
			// Sphere center is on the triangle — use triangle normal
			glm::vec3 edge0 = triB - triA;
			glm::vec3 edge1 = triC - triA;
			glm::vec3 n     = glm::cross(edge0, edge1);
			float     nLen  = glm::length(n);
			outNormal       = (nLen > 1e-6f) ? n / nLen : glm::vec3(0.0f, 1.0f, 0.0f);
			outPenetration  = sphereRadius;
		}
		else
		{
			outNormal      = delta / dist;
			outPenetration = sphereRadius - dist;
		}
		return true;
	}

	bool PhysicsSystem::CheckCapsuleMeshCollision(
		const glm::vec3& segA, const glm::vec3& segB, float capsuleRadius,
		const glm::vec3& triA, const glm::vec3& triB, const glm::vec3& triC,
		glm::vec3& outNormal, float& outPenetration) const
	{
		// Find the point on the capsule segment closest to the triangle, then
		// treat it as a sphere of radius capsuleRadius.
		glm::vec3 triClosest = ClosestPointOnTriangle((segA + segB) * 0.5f, triA, triB, triC);
		glm::vec3 segClosest = ClosestPointOnSegment(triClosest, segA, segB);
		return CheckSphereMeshCollision(segClosest, capsuleRadius, triA, triB, triC, outNormal, outPenetration);
	}
}
