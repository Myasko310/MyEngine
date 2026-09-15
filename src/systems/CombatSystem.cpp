#include "systems/CombatSystem.h"

#include "components/BossAIComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/TransformComponent.h"
#include "core/Input.h"
#include "core/InputActions.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace MyEngine
{
	namespace
	{
		static constexpr float kAttackFrameRate = 60.0f;

		static glm::vec3 GetForwardFromTransform(const TransformComponent& transform)
		{
			const float yawRadians = glm::radians(transform.rotation.y);
			return glm::normalize(glm::vec3(std::cos(yawRadians), 0.0f, std::sin(yawRadians)));
		}

		static void EnsureFallbackAttacks(CombatAttackComponent& attack)
		{
			if (!attack.attacks.empty())
				return;

			CombatAttackDefinition light;
			light.name = "LightSlash";
			light.startupFrames = 12;
			light.activeFrames = 5;
			light.recoveryFrames = 20;
			light.damage = 10.0f;
			light.postureDamage = 8.0f;
			light.hitboxCenter = glm::vec3(0.0f, 1.0f, 1.0f);
			light.hitboxRadius = 0.45f;
			light.cooldownSeconds = 0.15f;

			CombatAttackDefinition heavy;
			heavy.name = "HeavySlash";
			heavy.startupFrames = 20;
			heavy.activeFrames = 7;
			heavy.recoveryFrames = 28;
			heavy.damage = 18.0f;
			heavy.postureDamage = 20.0f;
			heavy.hitboxCenter = glm::vec3(0.0f, 1.1f, 1.2f);
			heavy.hitboxRadius = 0.52f;
			heavy.cooldownSeconds = 0.28f;

			attack.attacks.push_back(light);
			attack.attacks.push_back(heavy);
		}

		static bool TryLoadAttackSetFromJson(CombatAttackComponent& attack)
		{
			if (attack.attackSetPath.empty())
			{
				attack.lastLoadError = "Attack set path is empty.";
				return false;
			}

			std::ifstream ifs(attack.attackSetPath);
			if (!ifs.is_open())
			{
				attack.lastLoadError = "Failed to open attack set JSON: " + attack.attackSetPath;
				return false;
			}

			rapidjson::IStreamWrapper stream(ifs);
			rapidjson::Document doc;
			doc.ParseStream(stream);
			if (doc.HasParseError() || !doc.IsObject())
			{
				attack.lastLoadError = "Invalid JSON in attack set: " + attack.attackSetPath;
				return false;
			}

			if (!doc.HasMember("attacks") || !doc["attacks"].IsArray())
			{
				attack.lastLoadError = "Attack set JSON must contain an 'attacks' array.";
				return false;
			}

			std::vector<CombatAttackDefinition> parsed;
			const auto& attacksArray = doc["attacks"];
			parsed.reserve(attacksArray.Size());

			for (rapidjson::SizeType i = 0; i < attacksArray.Size(); ++i)
			{
				const auto& src = attacksArray[i];
				if (!src.IsObject())
					continue;

				CombatAttackDefinition def;
				if (src.HasMember("name") && src["name"].IsString())
					def.name = src["name"].GetString();
				if (src.HasMember("startupFrames") && src["startupFrames"].IsInt())
					def.startupFrames = std::max(0, src["startupFrames"].GetInt());
				if (src.HasMember("activeFrames") && src["activeFrames"].IsInt())
					def.activeFrames = std::max(1, src["activeFrames"].GetInt());
				if (src.HasMember("recoveryFrames") && src["recoveryFrames"].IsInt())
					def.recoveryFrames = std::max(0, src["recoveryFrames"].GetInt());
				if (src.HasMember("damage") && src["damage"].IsNumber())
					def.damage = static_cast<float>(src["damage"].GetDouble());
				if (src.HasMember("postureDamage") && src["postureDamage"].IsNumber())
					def.postureDamage = static_cast<float>(src["postureDamage"].GetDouble());
				if (src.HasMember("hitboxRadius") && src["hitboxRadius"].IsNumber())
					def.hitboxRadius = std::max(0.01f, static_cast<float>(src["hitboxRadius"].GetDouble()));
				if (src.HasMember("cooldownSeconds") && src["cooldownSeconds"].IsNumber())
					def.cooldownSeconds = std::max(0.0f, static_cast<float>(src["cooldownSeconds"].GetDouble()));

				if (src.HasMember("hitboxCenter") && src["hitboxCenter"].IsArray() && src["hitboxCenter"].Size() >= 3)
				{
					def.hitboxCenter.x = static_cast<float>(src["hitboxCenter"][0].GetDouble());
					def.hitboxCenter.y = static_cast<float>(src["hitboxCenter"][1].GetDouble());
					def.hitboxCenter.z = static_cast<float>(src["hitboxCenter"][2].GetDouble());
				}

				parsed.push_back(def);
			}

			if (parsed.empty())
			{
				attack.lastLoadError = "No valid attack definitions found in JSON.";
				return false;
			}

			attack.attacks = std::move(parsed);
			attack.loadedFromJson = true;
			attack.lastLoadError.clear();
			return true;
		}

		static std::shared_ptr<Entity> FindTargetEntity(Scene& scene, const BossAIComponent& ai)
		{
			if (ai.targetEntityID != 0)
			{
				auto target = scene.GetEntitySharedByID(ai.targetEntityID);
				if (target && target->HasComponent<TransformComponent>())
					return target;
			}

			for (const auto& candidate : scene.GetEntities())
			{
				if (!candidate || !candidate->HasComponent<TransformComponent>())
					continue;
				if (candidate->GetName() == "Player" || candidate->GetTag() == "Player")
					return candidate;
			}

			return nullptr;
		}

		static const char* ToReactionName(CombatStatsComponent::HitReactionState state)
		{
			switch (state)
			{
			case CombatStatsComponent::HitReactionState::Hitstun: return "Hitstun";
			case CombatStatsComponent::HitReactionState::Staggered: return "Staggered";
			case CombatStatsComponent::HitReactionState::KnockedDown: return "KnockedDown";
			default: return "None";
			}
		}
	}

	void CombatSystem::OnUpdate(Scene& scene, float deltaTime)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<CombatAttackComponent>())
				continue;

			auto& attack = entity->GetComponent<CombatAttackComponent>();
			EnsureFallbackAttacks(attack);

			if (attack.autoLoadFromJson && (attack.requestReloadFromJson || attack.loadedFromJson == false))
			{
				attack.requestReloadFromJson = false;
				if (!TryLoadAttackSetFromJson(attack))
					attack.loadedFromJson = false;
			}
		}

		UpdateBossAI(scene, deltaTime);
		UpdateAttackPhases(scene, deltaTime);
		ResolveCombatHits(scene, deltaTime);
	}

	void CombatSystem::UpdateBossAI(Scene& scene, float deltaTime)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity ||
				!entity->HasComponent<BossAIComponent>() ||
				!entity->HasComponent<TransformComponent>() ||
				!entity->HasComponent<CombatAttackComponent>())
			{
				continue;
			}

			auto& ai = entity->GetComponent<BossAIComponent>();
			if (!ai.enabled)
				continue;

			auto target = FindTargetEntity(scene, ai);
			if (!target)
				continue;

			auto& transform = entity->GetComponent<TransformComponent>();
			auto& attack = entity->GetComponent<CombatAttackComponent>();
			auto& targetTransform = target->GetComponent<TransformComponent>();

			glm::vec3 toTarget = targetTransform.position - transform.position;
			const float distance = glm::length(toTarget);
			ai.distanceToTarget = distance;

			glm::vec3 toTargetDir = (distance > 0.0001f) ? (toTarget / distance) : glm::vec3(0.0f, 0.0f, 1.0f);
			glm::vec3 forward = GetForwardFromTransform(transform);
			ai.facingDotToTarget = glm::dot(forward, toTargetDir);

			if (target->HasComponent<CombatStatsComponent>())
			{
				const auto& targetStats = target->GetComponent<CombatStatsComponent>();
				if (targetStats.reactionState == CombatStatsComponent::HitReactionState::Staggered)
					ai.punishWindowTimer = std::max(ai.punishWindowTimer, ai.punishWindowSeconds);
			}

			ai.decisionTimer -= deltaTime;
			ai.stateTimer += deltaTime;
			ai.punishWindowTimer = std::max(0.0f, ai.punishWindowTimer - deltaTime);
			ai.desiredMoveDirection = glm::vec3(0.0f);

			if (ai.decisionTimer <= 0.0f)
			{
				ai.decisionTimer = ai.decisionInterval;

				if (attack.isAttacking)
				{
					ai.state = BossAIComponent::AIState::Recover;
				}
				else if (ai.punishWindowTimer > 0.0f && distance <= ai.punishRange)
				{
					ai.state = BossAIComponent::AIState::Punish;
				}
				else if (distance > ai.attackRange)
				{
					ai.state = BossAIComponent::AIState::Approach;
				}
				else if (distance > ai.desiredRange)
				{
					ai.state = BossAIComponent::AIState::Attack;
				}
				else
				{
					ai.state = BossAIComponent::AIState::Strafe;
				}
			}

			switch (ai.state)
			{
			case BossAIComponent::AIState::Approach:
				ai.desiredMoveDirection = glm::vec3(toTargetDir.x, 0.0f, toTargetDir.z);
				if (glm::length(ai.desiredMoveDirection) > 0.0001f)
					transform.position += glm::normalize(ai.desiredMoveDirection) * ai.approachSpeed * deltaTime;
				break;
			case BossAIComponent::AIState::Strafe:
			{
				glm::vec3 up(0.0f, 1.0f, 0.0f);
				glm::vec3 strafe = glm::cross(up, toTargetDir);
				if (glm::length(strafe) > 0.0001f)
				{
					ai.desiredMoveDirection = glm::normalize(strafe);
					transform.position += ai.desiredMoveDirection * ai.strafeSpeed * deltaTime;
				}
				break;
			}
			case BossAIComponent::AIState::Punish:
				if (!attack.isAttacking && attack.cooldownTimer <= 0.0f)
				{
					attack.selectedAttackIndex = static_cast<int>(std::min<size_t>(1, attack.attacks.size() - 1));
					attack.attackRequested = true;
				}
				break;
			case BossAIComponent::AIState::Attack:
				if (!attack.isAttacking && attack.cooldownTimer <= 0.0f)
				{
					attack.selectedAttackIndex = 0;
					attack.attackRequested = true;
				}
				break;
			default:
				break;
			}

			if (distance > 0.01f)
			{
				const float yaw = glm::degrees(std::atan2(toTargetDir.z, toTargetDir.x));
				transform.rotation.y = yaw;
			}
		}
	}

	void CombatSystem::UpdateAttackPhases(Scene& scene, float deltaTime)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity ||
				!entity->HasComponent<CombatAttackComponent>() ||
				!entity->HasComponent<CombatHitboxComponent>())
			{
				continue;
			}

			auto& attack = entity->GetComponent<CombatAttackComponent>();
			auto& hitbox = entity->GetComponent<CombatHitboxComponent>();

			EnsureFallbackAttacks(attack);
			if (attack.attacks.empty())
				continue;

			const bool playerRequestedAttack =
				(entity->GetName() == "Player" || entity->GetTag() == "Player") &&
				(InputActions::IsActionPressed("Attack") || Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT));

			if (playerRequestedAttack)
				attack.attackRequested = true;

			attack.cooldownTimer = std::max(0.0f, attack.cooldownTimer - deltaTime);
			hitbox.active = false;

			if (attack.phase == CombatAttackComponent::AttackPhase::Idle)
			{
				attack.isAttacking = false;
				attack.phaseFrame = 0;

				if (attack.attackRequested && attack.cooldownTimer <= 0.0f)
				{
					attack.attackRequested = false;
					attack.activeAttackIndex = std::clamp(attack.selectedAttackIndex, 0, static_cast<int>(attack.attacks.size()) - 1);
					attack.phase = CombatAttackComponent::AttackPhase::Startup;
					attack.phaseFrame = 0;
					attack.isAttacking = true;
				}

				continue;
			}

			const CombatAttackDefinition& def = attack.attacks[std::clamp(attack.activeAttackIndex, 0, static_cast<int>(attack.attacks.size()) - 1)];
			const int frameAdvance = std::max(1, static_cast<int>(std::round(deltaTime * kAttackFrameRate)));
			attack.phaseFrame += frameAdvance;

			if (attack.phase == CombatAttackComponent::AttackPhase::Startup)
			{
				if (attack.phaseFrame >= std::max(0, def.startupFrames))
				{
					attack.phase = CombatAttackComponent::AttackPhase::Active;
					attack.phaseFrame = 0;
					++attack.attackActivationCounter;

					hitbox.activationId = attack.attackActivationCounter;
					hitbox.damage = def.damage;
					hitbox.center = def.hitboxCenter;
					hitbox.radius = def.hitboxRadius;
				}
			}
			else if (attack.phase == CombatAttackComponent::AttackPhase::Active)
			{
				hitbox.active = true;
				hitbox.damage = def.damage;
				hitbox.center = def.hitboxCenter;
				hitbox.radius = def.hitboxRadius;

				if (attack.phaseFrame >= std::max(1, def.activeFrames))
				{
					attack.phase = CombatAttackComponent::AttackPhase::Recovery;
					attack.phaseFrame = 0;
					hitbox.active = false;
				}
			}
			else if (attack.phase == CombatAttackComponent::AttackPhase::Recovery)
			{
				if (attack.phaseFrame >= std::max(0, def.recoveryFrames))
				{
					attack.phase = CombatAttackComponent::AttackPhase::Idle;
					attack.phaseFrame = 0;
					attack.activeAttackIndex = -1;
					attack.isAttacking = false;
					attack.cooldownTimer = std::max(0.0f, def.cooldownSeconds);
				}
			}
		}
	}

	void CombatSystem::ResolveCombatHits(Scene& scene, float deltaTime)
	{
		(void)deltaTime;

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity ||
				!entity->HasComponent<CombatStatsComponent>() ||
				!entity->HasComponent<CombatHurtboxComponent>())
			{
				continue;
			}

			auto& stats = entity->GetComponent<CombatStatsComponent>();
			auto& hurtbox = entity->GetComponent<CombatHurtboxComponent>();

			stats.lastDamageTaken = 0.0f;
			stats.lastPostureDamageTaken = 0.0f;

			if (stats.isDead)
				continue;

			if (hurtbox.wasHitThisStep)
			{
				float postureDamage = hurtbox.damageTakenThisStep;
				if (auto attacker = scene.GetEntitySharedByID(hurtbox.lastHitByEntityId))
				{
					if (attacker->HasComponent<CombatAttackComponent>())
					{
						const auto& attack = attacker->GetComponent<CombatAttackComponent>();
						if (attack.activeAttackIndex >= 0 && attack.activeAttackIndex < static_cast<int>(attack.attacks.size()))
							postureDamage = attack.attacks[attack.activeAttackIndex].postureDamage;
					}
				}

				if (stats.guarding)
					postureDamage *= stats.guardPostureMultiplier;

				stats.health = std::max(0.0f, stats.health - hurtbox.damageTakenThisStep);
				stats.posture = std::clamp(stats.posture + postureDamage, 0.0f, stats.maxPosture);
				stats.lastDamageTaken = hurtbox.damageTakenThisStep;
				stats.lastPostureDamageTaken = postureDamage;
				stats.lastHitByEntityId = hurtbox.lastHitByEntityId;

				if (stats.health <= 0.0f)
				{
					stats.isDead = true;
					stats.reactionState = CombatStatsComponent::HitReactionState::KnockedDown;
					stats.reactionTimer = 9999.0f;
				}
				else if (stats.posture >= stats.maxPosture)
				{
					stats.reactionState = CombatStatsComponent::HitReactionState::Staggered;
					stats.reactionTimer = 1.0f;
				}
				else
				{
					stats.reactionState = CombatStatsComponent::HitReactionState::Hitstun;
					stats.reactionTimer = 0.2f;
				}
			}
			else
			{
				stats.posture = std::max(0.0f, stats.posture - stats.postureRecoveryPerSecond * deltaTime);
			}

			if (stats.reactionTimer > 0.0f)
			{
				stats.reactionTimer = std::max(0.0f, stats.reactionTimer - deltaTime);
				if (stats.reactionTimer <= 0.0f && stats.reactionState != CombatStatsComponent::HitReactionState::KnockedDown)
					stats.reactionState = CombatStatsComponent::HitReactionState::None;
			}

			(void)ToReactionName(stats.reactionState);
		}
	}
}
