#include "systems/BossBattleSystem.h"
#include "components/BossBattleComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/TransformComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "core/PerformanceMetrics.h"
#include <iostream>
#include <cmath>

namespace MyEngine
{
	BossBattleSystem::BossBattleSystem()
		: enableDebugLogging(false)
	{
	}

	void BossBattleSystem::OnUpdate(Scene& scene, float deltaTime)
	{
		PROFILE_SCOPE("BossBattle");

		// Health syncing from CombatStatsComponent
		UpdateBossHealth(scene, deltaTime);

		// Apply telegraph visual feedback
		ApplyTelegraphFeedback(scene);

		// Special abilities (legacy system, complementary to AI)
		UpdateSpecialAbilities(scene, deltaTime);
	}

	void BossBattleSystem::UpdateBossHealth(Scene& scene, float deltaTime)
	{
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			// Check if entity has combat stats component
			if (entity->HasComponent<CombatStatsComponent>())
			{
				auto& stats = entity->GetComponent<CombatStatsComponent>();
				boss.currentHealth = static_cast<uint32_t>(stats.health);
				boss.maxHealth = static_cast<uint32_t>(stats.maxHealth);

				if (stats.health <= 0.0f && !boss.isDead)
				{
					boss.isDead = true;
					if (enableDebugLogging)
						std::cout << "Boss '" << boss.bossName << "' has been defeated!" << std::endl;
				}
			}
		}
	}

	void BossBattleSystem::ApplyTelegraphFeedback(Scene& scene)
	{
		// Telegraph visual feedback during attack tells
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			// Only apply feedback during telegraph phase
			if (boss.aiState != BossBattleComponent::AIState::Telegraphing)
				continue;

			// Calculate telegraph intensity (0.0 = just started, 1.0 = about to hit)
			float telegraphProgress = 1.0f - (boss.telegraphTimer / boss.telegraphDuration);

			if (enableDebugLogging && telegraphProgress > 0.9f)
			{
				std::cout << "Boss '" << boss.bossName << "' attack incoming: " << boss.currentAttackPatternName << std::endl;
			}

			// TODO: Queue visual feedback
			// - Screen shake intensity based on attack type
			// - Red screen tint or UI warning based on telegraphProgress
			// - Audio warning cue
		}
	}

	void BossBattleSystem::UpdateBossAttacks(Scene& scene, float deltaTime)
	{
		// NOTE: Attack handling is now managed by BossBattleAI system
		// This method is kept for legacy compatibility but should be 
		// replaced by wiring BossBattleAI into the main scene update

		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			if (boss.isDead)
				continue;

			// Trigger animation state based on current AI state
			if (entity->HasComponent<AnimationStateMachineComponent>())
			{
				auto& anim = entity->GetComponent<AnimationStateMachineComponent>();

				switch (boss.aiState)
				{
				case BossBattleComponent::AIState::Telegraphing:
					// Transition to telegraph animation (usually idle with visual effect)
					break;

				case BossBattleComponent::AIState::Attacking:
					// Transition to attack animation
					if (!boss.currentAttackPatternName.empty())
					{
						// TODO: Trigger animation by pattern name
						// e.g., "Boss_SlashCombo", "Boss_Spin", "Boss_BlastCharge"
					}
					break;

				default:
					// Idle or other states
					break;
				}
			}
		}
	}

	void BossBattleSystem::UpdateSpecialAbilities(Scene& scene, float deltaTime)
	{
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			if (boss.isDead)
				continue;

			// Update special ability cooldown
			if (boss.specialAbilityCooldownRemaining > 0.0f)
			{
				boss.specialAbilityCooldownRemaining -= deltaTime;
			}

			// Trigger special ability in phase 2+
			if (boss.specialAbilityCooldownRemaining <= 0.0f && boss.phase >= 2)
			{
				boss.specialAbilityType = (boss.phase == 2) ? 1 : 2; // Summon in P2, AoE in P3
				boss.specialAbilityCooldownRemaining = boss.specialAbilityCooldown;

				TriggerSpecialAbility(scene, entity->GetID(), boss.specialAbilityType);

				if (enableDebugLogging)
					std::cout << "Boss '" << boss.bossName << "' uses special ability type " << boss.specialAbilityType << std::endl;
			}
		}
	}

	void BossBattleSystem::TriggerSpecialAbility(Scene& scene, uint32_t bossID, uint32_t abilityType)
	{
		auto boss = scene.GetEntityByID(bossID);
		if (!boss || !boss->HasComponent<TransformComponent>())
			return;

		auto& transform = boss->GetComponent<TransformComponent>();

		switch (abilityType)
		{
		case 1: // Summon minions
		{
			if (enableDebugLogging)
				std::cout << "  -> Summoning minions around boss..." << std::endl;
			// Could spawn minion entities here
			break;
		}
		case 2: // Area attack
		{
			if (enableDebugLogging)
				std::cout << "  -> Executing area attack at " << transform.position.x << ", " << transform.position.z << std::endl;
			// Could apply damage to all nearby entities
			break;
		}
		case 3: // Heal (rarely used)
		{
			if (enableDebugLogging)
				std::cout << "  -> Boss heals itself!" << std::endl;
			break;
		}
		default:
			break;
		}
	}
}
