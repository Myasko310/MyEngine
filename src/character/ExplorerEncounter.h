#pragma once

#include "ecs/Scene.h"
#include "character/CombatShapes.h"
#include "character/SparringEnergyBurst.h"
#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/CharacterCustomizationComponent.h"
#include "components/ExplorerComponent.h"
#include "components/SparringPartnerComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/SkeletonComponent.h"
#include "components/TransformComponent.h"

namespace MyEngine::ExplorerEncounter
{
	template<typename T>
	inline void CopyIfPresent(const Entity& source, Entity& destination)
	{
		if (source.HasComponent<T>())
			destination.AddComponent<T>(source.GetComponent<T>());
	}

	inline std::shared_ptr<Entity> SpawnCopy(Scene& scene, const std::shared_ptr<Entity>& player, const char* name, float offset)
	{
		if (!player || !player->HasComponent<CharacterControllerComponent>() || !player->HasComponent<TransformComponent>() ||
			!player->HasComponent<RigidbodyComponent>() || !player->HasComponent<CapsuleColliderComponent>())
			return nullptr;

		auto copy = scene.CreateEntity(name);
		copy->SetLayer(player->GetLayer());
		auto& transform = copy->AddComponent<TransformComponent>(player->GetComponent<TransformComponent>());
		transform.position.x += offset;
		auto& controller = copy->AddComponent<CharacterControllerComponent>(player->GetComponent<CharacterControllerComponent>());
		controller.ClearFrameInput();
		controller.hasPhysicsPosition = false;
		auto& rb = copy->AddComponent<RigidbodyComponent>(player->GetComponent<RigidbodyComponent>());
		rb.velocity = rb.acceleration = glm::vec3(0.0f);
		CopyIfPresent<CapsuleColliderComponent>(*player, *copy);
		CopyIfPresent<MeshComponent>(*player, *copy);
		CopyIfPresent<MeshRendererComponent>(*player, *copy);
		CopyIfPresent<SkeletonComponent>(*player, *copy);
		CopyIfPresent<AnimationComponent>(*player, *copy);
		CopyIfPresent<AnimationStateMachineComponent>(*player, *copy);
		CopyIfPresent<CharacterCustomizationComponent>(*player, *copy);
		return copy;
	}

	inline std::shared_ptr<Entity> Spawn(Scene& scene, const std::shared_ptr<Entity>& player)
	{
		auto copy = SpawnCopy(scene, player, "Explorer (Test)", 2.5f);
		if (copy)
		{
			auto& ai = copy->AddComponent<ExplorerComponent>();
			ai.playerId = player->GetID();
			ai.origin = ai.checkpoint = copy->GetComponent<TransformComponent>().position;
		}
		return copy;
	}

	inline std::shared_ptr<Entity> SpawnSparringPartner(Scene& scene, const std::shared_ptr<Entity>& player)
	{
		auto copy = SpawnCopy(scene, player, "Sparring Partner (Test)", -4.0f);
		if (!copy) return nullptr;
		auto& ai = copy->AddComponent<SparringPartnerComponent>();
		copy->GetComponent<CharacterControllerComponent>().dodgeCooldown *= .7f;
		ai.playerId = player->GetID();
		ai.addedAttack = !player->HasComponent<CombatAttackComponent>();
		ai.addedHitbox = !player->HasComponent<CombatHitboxComponent>();
		ai.addedHurtbox = !player->HasComponent<CombatHurtboxComponent>();
		ai.addedStats = !player->HasComponent<CombatStatsComponent>();
		ai.addedStrikeBones = !player->HasComponent<PlayerStrikeBones>();
		if (ai.addedStrikeBones) player->AddComponent<PlayerStrikeBones>().Bind(*player);
		if (ai.addedAttack) player->AddComponent<CombatAttackComponent>();
		if (ai.addedHitbox) player->AddComponent<CombatHitboxComponent>();
		if (ai.addedHurtbox) player->AddComponent<CombatHurtboxComponent>().center.y = 1.0f;
		if (ai.addedStats) player->AddComponent<CombatStatsComponent>();
		copy->AddComponent<CombatAttackComponent>();
		copy->AddComponent<CombatHitboxComponent>().team = player->GetComponent<CombatHurtboxComponent>().team == 0 ? 1 : 0;
		auto& hurt = copy->AddComponent<CombatHurtboxComponent>();
		hurt.team = player->GetComponent<CombatHitboxComponent>().team == 0 ? 1 : 0;
		hurt.center.y = 1.0f;
		copy->AddComponent<CombatStatsComponent>();
		copy->AddComponent<SparringRig>().Bind(*copy);
		return copy;
	}

	inline void RemoveTrainingComponents(Scene& scene, const SparringPartnerComponent& ai)
	{
		std::vector<uint32_t> bursts;
		for (const auto& e : scene.GetEntities())
			if (e && e->HasComponent<SparringEnergyBurst>() && (e->GetComponent<SparringEnergyBurst>().owner == ai.playerId || e->GetComponent<SparringEnergyBurst>().target == ai.playerId)) bursts.push_back(e->GetID());
		for (auto id : bursts) scene.DestroyEntity(id);
		auto player = scene.GetEntitySharedByID(ai.playerId);
		if (!player) return;
		if (ai.addedAttack) player->RemoveComponent<CombatAttackComponent>();
		if (ai.addedHitbox) player->RemoveComponent<CombatHitboxComponent>();
		if (ai.addedHurtbox) player->RemoveComponent<CombatHurtboxComponent>();
		if (ai.addedStats) player->RemoveComponent<CombatStatsComponent>();
		if (ai.addedStrikeBones) player->RemoveComponent<PlayerStrikeBones>();
	}
}
