#include "systems/BossBattleAI.h"
#include "components/BossBattleComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>

namespace MyEngine
{
	BossBattleAI::BossBattleAI()
		: patternLibrary_(std::make_shared<AttackPatternLibrary>())
	{
		patternLibrary_->Initialize();
	}

	void BossBattleAI::OnUpdate(Scene& scene, float deltaTime)
	{
		// Find and process all boss entities
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			// Update combat context
			UpdatePlayerDistance(scene, boss);

			// Phase transition check (before state machine)
			HandlePhaseTransitions(boss);

			// Main AI state machine
			UpdateAIState(scene, boss, deltaTime);
		}
	}

	void BossBattleAI::UpdateAIState(Scene& scene, BossBattleComponent& boss, float deltaTime)
	{
		switch (boss.aiState)
		{
		case BossBattleComponent::AIState::Idle:
			UpdateIdleState(scene, boss, deltaTime);
			break;

		case BossBattleComponent::AIState::Telegraphing:
			UpdateTelegraphState(scene, boss, deltaTime);
			break;

		case BossBattleComponent::AIState::Attacking:
			UpdateAttackingState(scene, boss, deltaTime);
			break;

		case BossBattleComponent::AIState::OnCooldown:
			UpdateCooldownState(boss, deltaTime);
			break;

		case BossBattleComponent::AIState::HitStun:
			UpdateHitStunState(boss, deltaTime);
			break;

		case BossBattleComponent::AIState::PhaseTransition:
			UpdatePhaseTransitionState(boss, deltaTime);
			break;
		}
	}

	void BossBattleAI::UpdateIdleState(Scene& scene, BossBattleComponent& boss, float deltaTime)
	{
		// If on cooldown still, don't attack
		if (boss.attackCooldownRemaining > 0.0f)
		{
			boss.attackCooldownRemaining -= deltaTime;
			return;
		}

		// Ready to attack! Select a pattern and telegraph
		SelectAndStartAttack(scene, boss);
	}

	void BossBattleAI::UpdateTelegraphState(Scene& scene, BossBattleComponent& boss, float deltaTime)
	{
		// Count down telegraph timer
		boss.telegraphTimer -= deltaTime;

		if (boss.telegraphTimer <= 0.0f)
		{
			// Telegraph complete, transition to attacking
			boss.telegraphTimer = 0.0f;
			boss.isInTelegraphPhase = false;
			boss.attackTimer = boss.attackDuration;
			boss.isExecutingAttack = true;
			boss.hasAttackHitThisFrame = false;  // Reset hit flag
			TransitionAIState(boss, BossBattleComponent::AIState::Attacking);
			LogAIDebug(boss, "Telegraph complete, transitioning to attack execution");
		}
	}

	void BossBattleAI::UpdateAttackingState(Scene& scene, BossBattleComponent& boss, float deltaTime)
	{
		// Count down attack timer
		boss.attackTimer -= deltaTime;

		// Apply damage once during attack window
		if (!boss.hasAttackHitThisFrame && boss.attackTimer < boss.attackDuration * 0.7f)
		{
			const AttackPattern* pattern = patternLibrary_->GetPattern(boss.currentAttackPatternID);
			if (pattern)
			{
				ApplyAttackDamage(scene, boss, *pattern);
				ApplyAttackEffects(scene, boss, *pattern);
				boss.hasAttackHitThisFrame = true;
				LogAIDebug(boss, "Attack hit applied: " + pattern->name);
			}
		}

		// Check if attack finished
		if (boss.attackTimer <= 0.0f)
		{
			boss.attackTimer = 0.0f;
			boss.isExecutingAttack = false;

			// Transition to cooldown
			const AttackPattern* pattern = patternLibrary_->GetPattern(boss.currentAttackPatternID);
			if (pattern)
			{
				boss.attackCooldownRemaining = pattern->cooldownAfter;
			}
			else
			{
				boss.attackCooldownRemaining = 0.5f;  // Fallback cooldown
			}

			TransitionAIState(boss, BossBattleComponent::AIState::OnCooldown);
			LogAIDebug(boss, "Attack finished, entering cooldown");
		}
	}

	void BossBattleAI::UpdateCooldownState(BossBattleComponent& boss, float deltaTime)
	{
		// Count down cooldown
		boss.attackCooldownRemaining -= deltaTime;

		if (boss.attackCooldownRemaining <= 0.0f)
		{
			boss.attackCooldownRemaining = 0.0f;
			boss.isOnCooldown = false;
			TransitionAIState(boss, BossBattleComponent::AIState::Idle);
			LogAIDebug(boss, "Cooldown finished, returning to idle");
		}
	}

	void BossBattleAI::UpdateHitStunState(BossBattleComponent& boss, float deltaTime)
	{
		// Stub: Should be called when boss takes damage from player
		// For now, immediately return to idle
		TransitionAIState(boss, BossBattleComponent::AIState::Idle);
	}

	void BossBattleAI::UpdatePhaseTransitionState(BossBattleComponent& boss, float deltaTime)
	{
		// Phase transition animation/pause
		boss.phaseTransitionTimer -= deltaTime;

		if (boss.phaseTransitionTimer <= 0.0f)
		{
			boss.phaseTransitionTimer = 0.0f;
			boss.isTransitioningPhase = false;
			TransitionAIState(boss, BossBattleComponent::AIState::Idle);
			LogAIDebug(boss, "Phase transition complete");
		}
	}

	void BossBattleAI::UpdatePlayerDistance(Scene& scene, BossBattleComponent& boss)
	{
		// Find boss entity
		Entity* bossEntity = nullptr;
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			if (&entity->GetComponent<BossBattleComponent>() == &boss)
			{
				bossEntity = entity.get();
				break;
			}
		}

		if (!bossEntity || !bossEntity->HasComponent<TransformComponent>())
			return;

		auto& bossTrans = bossEntity->GetComponent<TransformComponent>();

		// Find player entity (assuming first entity with CameraComponent or tag "Player")
		// For now, just use a default distance
		boss.lastPlayerDistance = 5.0f;  // TODO: Find actual player entity
	}

	void BossBattleAI::HandlePhaseTransitions(BossBattleComponent& boss)
	{
		float normalizedHealth = GetNormalizedBossHealth(boss);

		// Phase 1 -> 2
		if (boss.phase == 1 && normalizedHealth <= boss.phaseTransitionHealth1)
		{
			boss.phase = 2;
			boss.isTransitioningPhase = true;
			boss.phaseTransitionTimer = 1.0f;  // 1 second phase transition pause
			TransitionAIState(boss, BossBattleComponent::AIState::PhaseTransition);
			LogAIDebug(boss, "Transitioned to Phase 2!");
		}

		// Phase 2 -> 3
		if (boss.phase == 2 && normalizedHealth <= boss.phaseTransitionHealth2)
		{
			boss.phase = 3;
			boss.isTransitioningPhase = true;
			boss.phaseTransitionTimer = 1.5f;  // 1.5 second transition for final phase
			TransitionAIState(boss, BossBattleComponent::AIState::PhaseTransition);
			LogAIDebug(boss, "Transitioned to Phase 3! Boss is entering desperate mode!");
		}
	}

	void BossBattleAI::SelectAndStartAttack(Scene& scene, BossBattleComponent& boss)
	{
		float normalizedHealth = GetNormalizedBossHealth(boss);

		// Select attack from library
		const AttackPattern* selectedPattern = patternLibrary_->SelectRandomAttack(
			boss.phase,
			normalizedHealth,
			boss.lastPlayerDistance
		);

		if (!selectedPattern)
		{
			LogAIDebug(boss, "No valid attack pattern found!");
			return;
		}

		// Start telegraph phase
		boss.currentAttackPatternID = selectedPattern->patternID;
		boss.currentAttackPatternName = selectedPattern->name;
		boss.telegraphDuration = selectedPattern->telegraphDuration;
		boss.telegraphTimer = selectedPattern->telegraphDuration;
		boss.attackDuration = selectedPattern->attackDuration;
		boss.isInTelegraphPhase = true;
		boss.hasAttackHitThisFrame = false;

		TransitionAIState(boss, BossBattleComponent::AIState::Telegraphing);
		LogAIDebug(boss, "Selected attack: " + selectedPattern->name);
	}

	void BossBattleAI::ApplyAttackDamage(Scene& scene, BossBattleComponent& boss, const AttackPattern& pattern)
	{
		// Find player entity and apply damage
		// TODO: Find actual player entity and apply damage via CombatStatsComponent
		// For now, this is a placeholder

		// Example logic (when player entity is found):
		// auto* playerCombat = playerEntity->GetComponent<CombatStatsComponent>();
		// if (playerCombat)
		// {
		//     playerCombat->health -= pattern.damageDealt;
		//     playerCombat->posture += pattern.postureBreakDamage;
		//     playerCombat->lastDamageTaken = pattern.damageDealt;
		// }
	}

	void BossBattleAI::ApplyAttackEffects(Scene& scene, BossBattleComponent& boss, const AttackPattern& pattern)
	{
		// Trigger sound effects, particle effects, etc.
		// TODO: Queue sound playback: pattern.soundEffectName
		// TODO: Create particles: pattern.particleEffectName
		// TODO: Play animation: pattern.animationStateName
	}

	float BossBattleAI::GetNormalizedBossHealth(const BossBattleComponent& boss) const
	{
		if (boss.maxHealth == 0)
			return 1.0f;

		return static_cast<float>(boss.currentHealth) / static_cast<float>(boss.maxHealth);
	}

	void BossBattleAI::TransitionAIState(BossBattleComponent& boss, BossBattleComponent::AIState newState)
	{
		boss.previousAIState = boss.aiState;
		boss.aiState = newState;
	}

	void BossBattleAI::LogAIDebug(const BossBattleComponent& boss, const std::string& message) const
	{
		if (boss.enableDebugLogging)
		{
			std::cout << "[BossBattleAI] " << boss.bossName << " (" << boss.bossID << "): " << message << std::endl;
		}
	}
}
