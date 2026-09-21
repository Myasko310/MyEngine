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

		UpdateBossHealth(scene, deltaTime);
		UpdateBossPhases(scene);
		UpdateBossAttacks(scene, deltaTime);
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

	void BossBattleSystem::UpdateBossPhases(Scene& scene)
	{
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();
			float healthPercent = boss.maxHealth > 0 ? static_cast<float>(boss.currentHealth) / boss.maxHealth : 1.0f;

			uint32_t newPhase = 1;
			if (healthPercent <= boss.phaseTransitionHealth2)
				newPhase = 3;
			else if (healthPercent <= boss.phaseTransitionHealth1)
				newPhase = 2;

			if (newPhase != boss.phase)
			{
				boss.phase = newPhase;
				if (enableDebugLogging)
					std::cout << "Boss '" << boss.bossName << "' entered phase " << newPhase << std::endl;

				// Trigger phase-transition animation
				if (entity->HasComponent<AnimationStateMachineComponent>())
				{
					auto& anim = entity->GetComponent<AnimationStateMachineComponent>();
					// Could transition to "BossPhase2" or "BossPhase3" state here
				}
			}
		}
	}

	void BossBattleSystem::UpdateBossAttacks(Scene& scene, float deltaTime)
	{
		for (auto& entity : scene.GetEntities())
		{
			if (!entity->HasComponent<BossBattleComponent>())
				continue;

			auto& boss = entity->GetComponent<BossBattleComponent>();

			if (boss.isDead)
				continue;

			// Update attack cooldown
			if (boss.attackCooldownRemaining > 0.0f)
			{
				boss.attackCooldownRemaining -= deltaTime;
				if (boss.attackCooldownRemaining <= 0.0f)
					boss.isAttacking = false;
			}

			// Trigger attack based on phase
			if (boss.attackCooldownRemaining <= 0.0f)
			{
				boss.isAttacking = true;
				boss.attackCooldownRemaining = boss.attackCooldown / boss.phase; // Faster attacks in later phases

				if (enableDebugLogging)
					std::cout << "Boss '" << boss.bossName << "' attacks!" << std::endl;

				// Trigger attack animation
				if (entity->HasComponent<AnimationStateMachineComponent>())
				{
					auto& anim = entity->GetComponent<AnimationStateMachineComponent>();
					// Could transition to "Attack" state here
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
