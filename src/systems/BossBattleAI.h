#pragma once

#include "ecs/System.h"
#include "core/AttackPattern.h"
#include "components/BossBattleComponent.h"
#include <memory>

class Scene;

namespace MyEngine
{
	// ============================================================
	// BossBattleAI
	// ============================================================
	// Handles boss attack selection, state machine transitions,
	// and AI decision-making for dynamic, engaging combat.
	// Integrates with BossBattleComponent and AttackPatternLibrary.
	// ============================================================
	class BossBattleAI : public System
	{
	public:
		BossBattleAI();
		~BossBattleAI() override = default;

		void OnUpdate(Scene& scene, float deltaTime) override;

	private:
		std::shared_ptr<AttackPatternLibrary> patternLibrary_;

		// Core AI update steps
		void UpdateAIState(Scene& scene, BossBattleComponent& boss, float deltaTime);
		void UpdatePlayerDistance(Scene& scene, BossBattleComponent& boss);
		void HandlePhaseTransitions(BossBattleComponent& boss);

		// State machine handlers
		void UpdateIdleState(Scene& scene, BossBattleComponent& boss, float deltaTime);
		void UpdateTelegraphState(Scene& scene, BossBattleComponent& boss, float deltaTime);
		void UpdateAttackingState(Scene& scene, BossBattleComponent& boss, float deltaTime);
		void UpdateCooldownState(BossBattleComponent& boss, float deltaTime);
		void UpdateHitStunState(BossBattleComponent& boss, float deltaTime);
		void UpdatePhaseTransitionState(BossBattleComponent& boss, float deltaTime);

		// Attack execution
		void SelectAndStartAttack(Scene& scene, BossBattleComponent& boss);
		void ApplyAttackDamage(Scene& scene, BossBattleComponent& boss, const AttackPattern& pattern);
		void ApplyAttackEffects(Scene& scene, BossBattleComponent& boss, const AttackPattern& pattern);

		// Helper utilities
		float GetNormalizedBossHealth(const BossBattleComponent& boss) const;
		void TransitionAIState(BossBattleComponent& boss, BossBattleComponent::AIState newState);

		// Debug
		void LogAIDebug(const BossBattleComponent& boss, const std::string& message) const;
	};
}
