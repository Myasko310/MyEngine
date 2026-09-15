#pragma once

#include "ecs/System.h"

namespace MyEngine
{
	class CombatSystem : public System
	{
	public:
		void OnUpdate(Scene& scene, float deltaTime) override;

	private:
		void UpdateBossAI(Scene& scene, float deltaTime);
		void UpdateAttackPhases(Scene& scene, float deltaTime);
		void ResolveCombatHits(Scene& scene, float deltaTime);
	};
}
