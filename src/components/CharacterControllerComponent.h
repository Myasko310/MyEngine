#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace MyEngine
{
	struct CharacterControllerComponent : public Component
	{
		glm::vec3 moveInput = glm::vec3(0.0f);
		glm::vec3 groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 groundVelocity = glm::vec3(0.0f);

		float moveSpeed = 4.5f;
		float airControl = 0.35f;
		float jumpSpeed = 6.0f;
		float gravityScale = 1.0f;
		float maxSlopeAngleDegrees = 50.0f;
		float groundSnapDistance = 0.15f;
		float skinWidth = 0.02f;
		float maxStepHeight = 0.35f;
		float acceleration = 32.0f;
		float airAcceleration = 10.0f;
		float braking = 24.0f;
		float slideGravityScale = 1.25f;
		float jumpUngroundedDuration = 0.30f;
		bool enableSprintSlide = false;
		float sprintMultiplier = 1.5f;
		float slideSpeedMultiplier = 1.12f;
		float slideDuration = 0.75f;
		float slideCooldown = 0.5f;
		float slideInputBufferDuration = 0.18f;
		float slideStartMomentumBoost = 1.08f;
		float slideGroundFrictionScale = 0.82f;
		bool enableDodge = true;
		float dodgeSpeedMultiplier = 2.4f;
		float dodgeDuration = 0.22f;
		float dodgeCooldown = 0.55f;
		bool enableVault = true;
		float vaultMinHeight = 0.6f;
		float vaultMaxHeight = 1.6f;
		float vaultForwardDistance = 1.4f;
		float vaultDuration = 0.34f;
		float vaultCooldown = 0.45f;
		float vaultArcHeight = 0.35f;
		float vaultOverheadArcScale = 0.10f;
		float vaultOverheadMarginScale = 0.5f;
		float vaultArchArcScale = 0.08f;
		float vaultArchHeightScale = 0.035f;
		bool enableWallJump = true;
		float wallJumpHorizontalSpeed = 5.5f;
		float wallJumpVerticalSpeed = 6.2f;
		float wallJumpCooldown = 0.25f;
		float wallJumpFacingDotThreshold = 0.25f;
		float wallJumpStateDuration = 0.26f;
		bool enableWallRun = true;
		float wallRunSpeed = 5.0f;
		float wallRunGravityScale = 0.15f;
		float wallRunMaxDuration = 0.75f;
		float wallRunMinWallAngleDegrees = 65.0f;
		float wallRunMaxWallAngleDegrees = 110.0f;
		float wallRunJumpOffSpeed = 5.0f;
		float wallRunCooldown = 0.35f;
		float wallRunJumpUngroundedDuration = 0.18f;
		float wallRunEntrySpeedThreshold = 2.2f;
		float wallRunMinVerticalSpeed = -0.5f;
		float wallRunWallClearance = 0.06f;
		float wallRunContactGraceDuration = 0.10f;
		float wallRunCommitDuration = 0.22f;
		float wallRunEndPushOffSpeed = 2.8f;
		float wallRunEndPushOffUpwardSpeed = 1.6f;
		float wallRunEndMomentumDuration = 0.24f;
		float wallRunEndTransitionDuration = 1.50f;
		float wallRunEndDirectionAngleDegrees = 90.0f;
		int wallRunEndRotateBeginFrame = 0;
		float turnSpeed = 12.0f;
		bool sprintRequested = false;
		bool slideRequested = false;
		bool dodgeRequested = false;
		bool wallRunRequested = false;
		bool vaultRequested = false;
		bool crouchHeld = false;
		bool sprintLatched = false;
		bool isSprinting = false;
		bool isSliding = false;
		bool isDodging = false;
		bool isVaulting = false;
		bool isWallJumping = false;
		bool isWallRunning = false;
		bool wallRunHeldLatch = false;
		bool mirrorWallRunAnimation = false;
		bool wallRunEndRotatePending = false;
		bool wallRunAirborneFacingLock = false;
		int wallRunSide = 0;
		int wallRunEndRotateDirection = 0;
		int wallRunEndRotateFrameCounter = 0;
		glm::vec3 wallRunNormal = glm::vec3(0.0f);
		glm::vec3 wallRunDirection = glm::vec3(0.0f);
		glm::vec3 wallRunEndMomentumDirection = glm::vec3(0.0f);
		float wallRunTimer = 0.0f;
		float wallRunCooldownTimer = 0.0f;
		float wallRunContactGraceTimer = 0.0f;
		float wallRunCommitTimer = 0.0f;
		float wallRunEntrySpeed = 0.0f;
		float wallRunEndMomentumTimer = 0.0f;
		float wallRunEndMomentumSpeed = 0.0f;
		float slideTimer = 0.0f;
		float slideCooldownTimer = 0.0f;
		float slideInputBufferTimer = 0.0f;
		float dodgeTimer = 0.0f;
		float dodgeCooldownTimer = 0.0f;
		float vaultTimer = 0.0f;
		float vaultCooldownTimer = 0.0f;
		float wallJumpTimer = 0.0f;
		float wallJumpCooldownTimer = 0.0f;
		float vaultElapsed = 0.0f;
		float vaultHeight = 0.0f;
		float vaultMotionDuration = 0.0f;
		float slideEntrySpeed = 0.0f;
		float dodgeEntrySpeed = 0.0f;
		glm::vec3 slideDirection = glm::vec3(0.0f);
		glm::vec3 dodgeDirection = glm::vec3(0.0f);
		glm::vec3 vaultStartPosition = glm::vec3(0.0f);
		glm::vec3 vaultEndPosition = glm::vec3(0.0f);
		std::vector<glm::vec4> vaultPath;
		glm::vec3 previousPhysicsPosition = glm::vec3(0.0f);
		bool hasPhysicsPosition = false;
		bool hasMovementInputTick = false;
		unsigned int lastMovementInputTick = 0;

		std::string animationSpeedParameter;
		std::string animationGroundedParameter;
		std::string animationJumpTriggerParameter;
		float currentSpeed = 0.0f;
		float jumpUngroundedTimer = 0.0f;
		bool jumpedThisFrame = false;
		bool wallJumpTriggeredThisFrame = false;
		bool wallJumpFallSyncRequested = false;
		float wallJumpEstimatedAirTimeRemaining = 0.0f;
		float capsuleBottomAnimationLift = 0.0f;
		float maxCapsuleBottomAnimationLift = 0.40f;
		float jumpCapsuleLift = 0.30f;
		float wallJumpCapsuleLift = 0.26f;
		float fallCapsuleLift = 0.12f;

		bool jumpRequested = false;
		bool jumpHeld = false;
		bool attack1Requested = false;
		bool attack2Requested = false;
		bool attack3Requested = false;
		bool isGrounded = false;
		bool wasGrounded = false;
		bool isOnSteepSlope = false;
		bool enableGroundSnap = true;
		bool orientToMovement = true;

		void ClearFrameInput()
		{
			moveInput = glm::vec3(0.0f);
			jumpRequested = false;
			jumpHeld = false;
			sprintRequested = false;
			slideRequested = false;
			dodgeRequested = false;
			wallRunRequested = false;
			vaultRequested = false;
			crouchHeld = false;
			attack1Requested = false;
			attack2Requested = false;
			attack3Requested = false;
			jumpedThisFrame = false;
		}
	};
}
