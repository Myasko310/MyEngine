#include "PhysicsSystem.h"
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
#include "components/CollisionEventsComponent.h"
#include "components/JointComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "core/CollisionMatrix.h"
#include "core/Input.h"
#include "core/InputActions.h"

#include <iostream>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <cmath>
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
		OnUpdate(scene, deltaTime, nullptr, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	void PhysicsSystem::OnUpdate(Scene& scene, float deltaTime, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight)
	{
		CollectCharacterControllerInput(scene, window, cameraForward, cameraRight);

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

	void PhysicsSystem::CollectCharacterControllerInput(Scene& scene, GLFWwindow* window, const glm::vec3& cameraForward, const glm::vec3& cameraRight)
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

		float moveForward = InputActions::GetAxis("MoveForward");
		float moveRight = InputActions::GetAxis("MoveRight");

		glm::vec3 moveInput = flattenedForward * moveForward + flattenedRight * moveRight;
		if (glm::length(moveInput) > 1.0f)
			moveInput = glm::normalize(moveInput);

		bool jumpPressed = InputActions::IsActionPressed("Jump");

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<CharacterControllerComponent>())
				continue;

			auto& controller = entity->GetComponent<CharacterControllerComponent>();
			controller.moveInput = moveInput;
			controller.jumpRequested = jumpPressed;
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
		glm::vec3 worldScale = ExtractWorldScale(scene, *entity);
		glm::vec3 segA = transform.position + capsule.pointA * worldScale;
		glm::vec3 segB = transform.position + capsule.pointB * worldScale;
		float radius = capsule.radius * glm::compMax(glm::vec2(worldScale.x, worldScale.z));

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

	bool PhysicsSystem::SweepCharacterPairs(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const
	{
		if (!entity || !entity->HasComponent<TransformComponent>() || !entity->HasComponent<CapsuleColliderComponent>())
			return false;

		auto& transform = entity->GetComponent<TransformComponent>();
		auto& capsule = entity->GetComponent<CapsuleColliderComponent>();
		glm::vec3 worldScale = ExtractWorldScale(scene, *entity);
		glm::vec3 segA = transform.position + capsule.pointA * worldScale;
		glm::vec3 segB = transform.position + capsule.pointB * worldScale;
		float radius = capsule.radius * glm::compMax(glm::vec2(worldScale.x, worldScale.z));

		bool foundHit = false;
		outPenetration = 0.0f;
		for (const auto& other : scene.GetEntities())
		{
			if (!other || other.get() == entity.get() || !other->HasComponent<TransformComponent>())
				continue;

			glm::vec3 otherScale = ExtractWorldScale(scene, *other);
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
				hit = CheckCapsuleCapsuleCollision(
					segA, segB, radius,
					other->GetComponent<TransformComponent>().position + otherCapsule.pointA * otherScale,
					other->GetComponent<TransformComponent>().position + otherCapsule.pointB * otherScale,
					otherCapsule.radius * glm::compMax(glm::vec2(otherScale.x, otherScale.z)),
					normal, penetration);
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

			if (!hit)
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

	bool PhysicsSystem::QueryCharacterSupport(Scene& scene, const std::shared_ptr<Entity>& entity, glm::vec3& outNormal, float& outPenetration) const
	{
		glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);
		float planePenetration = 0.0f;
		bool hitPlane = SweepCharacterPlanes(scene, entity, planeNormal, planePenetration);

		glm::vec3 pairNormal(0.0f, 1.0f, 0.0f);
		float pairPenetration = 0.0f;
		bool hitPair = SweepCharacterPairs(scene, entity, pairNormal, pairPenetration);

		if (!hitPlane && !hitPair)
			return false;

		outNormal = planeNormal;
		outPenetration = planePenetration;
		if (hitPair && (!hitPlane || pairPenetration > planePenetration ||
			(std::abs(pairPenetration - planePenetration) < 0.0001f && glm::dot(pairNormal, glm::vec3(0.0f, 1.0f, 0.0f)) > glm::dot(planeNormal, glm::vec3(0.0f, 1.0f, 0.0f)))))
		{
			outNormal = pairNormal;
			outPenetration = pairPenetration;
		}

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

			resolvedAny = true;
			transform.position += hitNormal * (hitPenetration + controller.skinWidth);
			float intoSurface = glm::dot(rb.velocity, hitNormal);
			if (intoSurface < 0.0f)
				rb.velocity -= intoSurface * hitNormal;

			if (IsWalkableSlope(hitNormal, controller.maxSlopeAngleDegrees))
			{
				controller.isGrounded = true;
				controller.groundNormal = glm::normalize(hitNormal);
				controller.isOnSteepSlope = false;
				if (rb.velocity.y < 0.0f)
					rb.velocity.y = 0.0f;
			}
			else
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

	void PhysicsSystem::UpdateControllerAnimationState(const std::shared_ptr<Entity>& entity, CharacterControllerComponent& controller) const
	{
		if (!entity || !entity->HasComponent<AnimationStateMachineComponent>())
			return;

		auto& sm = entity->GetComponent<AnimationStateMachineComponent>();
		if (!sm.stateMachine)
			return;

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
			if (parameter.type == AnimationStateMachineParameterType::Trigger && controller.jumpedThisFrame)
				value.triggerValue = true;
		});

		// Update Crouch/Slide parameter from action bindings
		{
			const bool crouchOrSlide = InputActions::IsAction("Crouch") || InputActions::IsAction("Slide");
			int crouchIdx = sm.stateMachine->FindParameterIndex("IsCrouching");
			if (crouchIdx >= 0 && static_cast<size_t>(crouchIdx) < sm.parameterValues.size())
			{
				sm.parameterValues[crouchIdx].boolValue = crouchOrSlide;
			}
		}

		// Update Fight/Attack parameter from action bindings
		{
			const bool isAttacking = InputActions::IsAction("Fight") || InputActions::IsAction("Attack");
			int fightIdx = sm.stateMachine->FindParameterIndex("IsFighting");
			if (fightIdx >= 0 && static_cast<size_t>(fightIdx) < sm.parameterValues.size())
			{
				sm.parameterValues[fightIdx].boolValue = isAttacking;
			}
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
			rb.isKinematic = true;

			controller.wasGrounded = controller.isGrounded;
			controller.isGrounded = false;
			controller.isOnSteepSlope = false;
			controller.groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
			controller.groundVelocity = glm::vec3(0.0f);
			controller.jumpedThisFrame = false;

			glm::vec3 probeNormal(0.0f, 1.0f, 0.0f);
			float probePenetration = 0.0f;
			bool hasSupport = QueryCharacterSupport(scene, entity, probeNormal, probePenetration);
			if (hasSupport && IsWalkableSlope(probeNormal, controller.maxSlopeAngleDegrees))
			{
				controller.isGrounded = true;
				controller.wasGrounded = true;
				controller.groundNormal = glm::normalize(probeNormal);
				if (rb.velocity.y < 0.0f)
					rb.velocity.y = 0.0f;
			}
			else if (hasSupport)
			{
				controller.isOnSteepSlope = true;
			}

			glm::vec3 moveInput = controller.moveInput;
			if (controller.isGrounded)
				moveInput = FlattenToPlane(moveInput, controller.groundNormal);
			if (glm::length(moveInput) > 1.0f)
				moveInput = glm::normalize(moveInput);

			glm::vec3 horizontalVelocity(rb.velocity.x, 0.0f, rb.velocity.z);
			glm::vec3 targetHorizontalVelocity = moveInput * controller.moveSpeed;
			if (glm::length(moveInput) > 0.0001f)
			{
				float accel = controller.isGrounded
					? controller.acceleration
					: controller.airAcceleration * std::max(controller.airControl, 0.0f);
				horizontalVelocity = MoveTowards(horizontalVelocity, targetHorizontalVelocity, accel * dt);
			}
			else
			{
				horizontalVelocity = MoveTowards(horizontalVelocity, glm::vec3(0.0f), controller.braking * dt);
			}
			rb.velocity.x = horizontalVelocity.x;
			rb.velocity.z = horizontalVelocity.z;
			controller.currentSpeed = glm::length(glm::vec2(rb.velocity.x, rb.velocity.z));

			if (controller.jumpRequested && controller.isGrounded)
			{
				rb.velocity.y = controller.jumpSpeed;
				controller.isGrounded = false;
				controller.wasGrounded = false;
				controller.jumpedThisFrame = true;
			}
			else if (!controller.isGrounded)
			{
				rb.velocity += gravity * controller.gravityScale * dt;
			}

			if (controller.isOnSteepSlope)
			{
				glm::vec3 slideDirection = FlattenToPlane(gravity, probeNormal);
				if (glm::length(slideDirection) > 0.0001f)
					rb.velocity += glm::normalize(slideDirection) * (glm::length(gravity) * controller.slideGravityScale * dt);
			}

			glm::vec3 horizontalDisplacement(rb.velocity.x * dt, 0.0f, rb.velocity.z * dt);
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

			controller.groundVelocity = glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z);

			if (controller.enableGroundSnap && !controller.jumpRequested && rb.velocity.y <= 0.0f)
			{
				glm::vec3 originalPosition = transform.position;
				transform.position -= glm::vec3(0.0f, controller.groundSnapDistance, 0.0f);
				glm::vec3 snapNormal(0.0f, 1.0f, 0.0f);
				float snapPenetration = 0.0f;
				if (QueryCharacterSupport(scene, entity, snapNormal, snapPenetration) && IsWalkableSlope(snapNormal, controller.maxSlopeAngleDegrees))
				{
					transform.position += snapNormal * (snapPenetration + controller.skinWidth);
					controller.isGrounded = true;
					controller.groundNormal = glm::normalize(snapNormal);
					controller.isOnSteepSlope = false;
					rb.velocity.y = 0.0f;
				}
				else
				{
					transform.position = originalPosition;
				}
			}

			glm::vec3 facing = FlattenToPlane(glm::vec3(rb.velocity.x, 0.0f, rb.velocity.z), glm::vec3(0.0f, 1.0f, 0.0f));
			if (controller.orientToMovement && glm::length(facing) > 0.1f)
			{
				facing = glm::normalize(facing);
				// TransformComponent rotation uses radians; use +Z-forward yaw convention
				// so character-facing aligns better with imported humanoid animation sets.
				transform.rotation.y = std::atan2(facing.x, facing.z);
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

		if (minDist < capsuleRadius)
		{
			outNormal = planeNormal;
			outPenetration = capsuleRadius - minDist;
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

		for (const auto& cellEntry : cells)
		{
			const auto& entities = cellEntry.second;
			for (size_t i = 0; i < entities.size(); ++i)
			{
				for (size_t j = i + 1; j < entities.size(); ++j)
				{
					const auto& a = entities[i];
					const auto& b = entities[j];

					// Build a stable pair key from the entities' addresses.
					auto pa = reinterpret_cast<std::uintptr_t>(a.get());
					auto pb = reinterpret_cast<std::uintptr_t>(b.get());
					if (pa == pb)
						continue;

					std::uintptr_t lo = std::min(pa, pb);
					std::uintptr_t hi = std::max(pa, pb);
					// Simple mixing hash of the two pointers to form a dedup key.
					long long key = static_cast<long long>((lo * 2654435761u) ^ (hi * 2246822519u));

					if (seenPairs.find(key) != seenPairs.end())
						continue;
					seenPairs[key] = true;

					if (lo == pa)
						callback(a, b);
					else
						callback(b, a);
				}
			}
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
