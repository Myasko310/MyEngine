#include "network/NetReplicationSystem.h"

#include <algorithm>
#include <cmath>

#include "components/AnimationComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "network/NetPluginHooks.h"

namespace
{
	bool NearlyEqual(float a, float b, float eps)
	{
		return std::fabs(a - b) <= eps;
	}

	bool Vec3NearlyEqual(const glm::vec3& a, const glm::vec3& b, float eps)
	{
		return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps) && NearlyEqual(a.z, b.z, eps);
	}

	bool TryGetEntityState(const MyEngine::Net::WorldSnapshot& snapshot, std::uint32_t entityID, MyEngine::Net::ReplicatedEntityState& outState)
	{
		for (const auto& state : snapshot.entities)
		{
			if (state.entityID == entityID)
			{
				outState = state;
				return true;
			}
		}
		return false;
	}

	MyEngine::Net::ReplicatedEntityState LerpState(
		const MyEngine::Net::ReplicatedEntityState& a,
		const MyEngine::Net::ReplicatedEntityState& b,
		float t)
	{
		MyEngine::Net::ReplicatedEntityState result;
		result.entityID = a.entityID;
		result.position = glm::mix(a.position, b.position, t);
		result.rotation = glm::mix(a.rotation, b.rotation, t);
		result.velocity = glm::mix(a.velocity, b.velocity, t);
		const bool useRightState = t >= 0.5f;
		result.isGrounded = useRightState ? b.isGrounded : a.isGrounded;
		result.animationPlaying = useRightState ? b.animationPlaying : a.animationPlaying;
		result.audioPlaying = useRightState ? b.audioPlaying : a.audioPlaying;
		result.activeAnimationClipIndex = useRightState ? b.activeAnimationClipIndex : a.activeAnimationClipIndex;
		result.animationTimeSeconds = glm::mix(a.animationTimeSeconds, b.animationTimeSeconds, t);
		result.audioEventName = useRightState ? b.audioEventName : a.audioEventName;
		return result;
	}
}

namespace MyEngine::Net
{
	WorldSnapshot NetReplicationSystem::BuildSnapshot(const Scene& scene, NetTick tick)
	{
		return BuildSnapshot(scene, tick, ReplicationInterestSettings{});
	}

	WorldSnapshot NetReplicationSystem::BuildSnapshot(const Scene& scene, NetTick tick, const ReplicationInterestSettings& interestSettings)
	{
		WorldSnapshot snapshot;
		snapshot.tick = tick;

		const bool useInterestFilter = interestSettings.enabled && interestSettings.radius > 0.0f;
		const float radiusSquared = interestSettings.radius * interestSettings.radius;

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<TransformComponent>())
				continue;

			const auto& transform = entity->GetComponent<TransformComponent>();
			if (useInterestFilter)
			{
				const glm::vec3 toEntity = transform.position - interestSettings.origin;
				if (glm::dot(toEntity, toEntity) > radiusSquared)
					continue;
			}

			ReplicatedEntityState state;
			state.entityID = entity->GetID();
			state.position = transform.position;
			state.rotation = transform.rotation;
			if (entity->HasComponent<RigidbodyComponent>())
				state.velocity = entity->GetComponent<RigidbodyComponent>().velocity;
			if (entity->HasComponent<CharacterControllerComponent>())
				state.isGrounded = entity->GetComponent<CharacterControllerComponent>().isGrounded;
			if (entity->HasComponent<AnimationComponent>())
			{
				const auto& anim = entity->GetComponent<AnimationComponent>();
				state.animationPlaying = anim.playing;
				state.activeAnimationClipIndex = anim.activeClipIndex;
				state.animationTimeSeconds = anim.time;
			}
			if (entity->HasComponent<AudioSourceComponent>())
			{
				const auto& audio = entity->GetComponent<AudioSourceComponent>();
				state.audioPlaying = audio.isPlaying;
				state.audioEventName = audio.eventName;
			}

			if (!NetPluginRegistry::ShouldReplicateEntity(*entity, state))
				continue;
			NetPluginRegistry::ApplyStateMutators(*entity, state);

			snapshot.entities.push_back(state);
		}

		return snapshot;
	}

	WorldSnapshot NetReplicationSystem::BuildDeltaSnapshot(const WorldSnapshot& baseline, const WorldSnapshot& current)
	{
		WorldSnapshot delta;
		delta.tick = current.tick;

		for (const auto& state : current.entities)
		{
			ReplicatedEntityState baselineState;
			if (!TryGetEntityState(baseline, state.entityID, baselineState))
			{
				delta.entities.push_back(state);
				continue;
			}

			const bool changed = !Vec3NearlyEqual(state.position, baselineState.position, 0.0001f) ||
				!Vec3NearlyEqual(state.rotation, baselineState.rotation, 0.0001f) ||
				!Vec3NearlyEqual(state.velocity, baselineState.velocity, 0.0001f) ||
				(state.isGrounded != baselineState.isGrounded) ||
				(state.animationPlaying != baselineState.animationPlaying) ||
				(state.audioPlaying != baselineState.audioPlaying) ||
				(state.activeAnimationClipIndex != baselineState.activeAnimationClipIndex) ||
				!NearlyEqual(state.animationTimeSeconds, baselineState.animationTimeSeconds, 0.0001f) ||
				(state.audioEventName != baselineState.audioEventName);
			if (changed)
				delta.entities.push_back(state);
		}

		return delta;
	}

	bool NetReplicationSystem::ApplySnapshot(Scene& scene, const WorldSnapshot& snapshot)
	{
		bool changedAnyEntity = false;
		for (const auto& state : snapshot.entities)
		{
			std::shared_ptr<Entity> entity = scene.GetEntitySharedByID(state.entityID);
			if (!entity)
				entity = scene.CreateEntityWithID(state.entityID);
			if (!entity)
				continue;

			auto& transform = entity->HasComponent<TransformComponent>()
				? entity->GetComponent<TransformComponent>()
				: entity->AddComponent<TransformComponent>();

			if (!Vec3NearlyEqual(transform.position, state.position, 0.0001f) ||
				!Vec3NearlyEqual(transform.rotation, state.rotation, 0.0001f))
			{
				transform.position = state.position;
				transform.rotation = state.rotation;
				changedAnyEntity = true;
			}

			if (entity->HasComponent<RigidbodyComponent>())
			{
				auto& rb = entity->GetComponent<RigidbodyComponent>();
				if (!Vec3NearlyEqual(rb.velocity, state.velocity, 0.0001f))
				{
					rb.velocity = state.velocity;
					changedAnyEntity = true;
				}
			}

			if (entity->HasComponent<CharacterControllerComponent>())
			{
				auto& controller = entity->GetComponent<CharacterControllerComponent>();
				if (controller.isGrounded != state.isGrounded)
				{
					controller.isGrounded = state.isGrounded;
					changedAnyEntity = true;
				}
			}

			if (entity->HasComponent<AnimationComponent>())
			{
				auto& anim = entity->GetComponent<AnimationComponent>();
				if (anim.playing != state.animationPlaying ||
					anim.activeClipIndex != state.activeAnimationClipIndex ||
					!NearlyEqual(anim.time, state.animationTimeSeconds, 0.0001f))
				{
					anim.playing = state.animationPlaying;
					anim.activeClipIndex = state.activeAnimationClipIndex;
					anim.time = state.animationTimeSeconds;
					changedAnyEntity = true;
				}
			}

			if (entity->HasComponent<AudioSourceComponent>())
			{
				auto& audio = entity->GetComponent<AudioSourceComponent>();
				if (audio.isPlaying != state.audioPlaying || audio.eventName != state.audioEventName)
				{
					audio.isPlaying = state.audioPlaying;
					audio.eventName = state.audioEventName;
					changedAnyEntity = true;
				}
			}
		}
		return changedAnyEntity;
	}

	bool NetReplicationSystem::ApplyDeltaSnapshot(Scene& scene, const WorldSnapshot& baseline, const WorldSnapshot& deltaSnapshot)
	{
		WorldSnapshot merged = baseline;
		merged.tick = deltaSnapshot.tick;
		for (const auto& state : deltaSnapshot.entities)
		{
			bool replaced = false;
			for (auto& mergedState : merged.entities)
			{
				if (mergedState.entityID == state.entityID)
				{
					mergedState = state;
					replaced = true;
					break;
				}
			}
			if (!replaced)
				merged.entities.push_back(state);
		}
		return ApplySnapshot(scene, merged);
	}

	void ClientReconciliationState::RecordPredictedInput(const InputCommand& input)
	{
		m_PendingInputs.push_back(input);
	}

	void ClientReconciliationState::Acknowledge(NetTick serverProcessedTick)
	{
		while (!m_PendingInputs.empty() && m_PendingInputs.front().tick <= serverProcessedTick)
			m_PendingInputs.pop_front();
	}

	std::size_t ClientReconciliationState::PendingInputCount() const
	{
		return m_PendingInputs.size();
	}

	void ClientReconciliationState::ReplayPredictedInputs(TransformComponent& transform, float moveSpeed, float fixedDeltaTime) const
	{
		if (fixedDeltaTime <= 0.0f || moveSpeed <= 0.0f)
			return;

		for (const auto& input : m_PendingInputs)
		{
			glm::vec3 move(input.moveAxis.x, 0.0f, input.moveAxis.y);
			const float moveLength = glm::length(move);
			if (moveLength > 1.0f)
				move /= moveLength;

			transform.position += move * moveSpeed * fixedDeltaTime;
		}
	}

	bool ClientReconciliationState::ReconcileEntity(
		TransformComponent& transform,
		const ReplicatedEntityState& authoritativeState,
		float positionErrorThreshold,
		float rotationErrorThreshold) const
	{
		const bool positionDiffers = !Vec3NearlyEqual(transform.position, authoritativeState.position, positionErrorThreshold);
		const bool rotationDiffers = !Vec3NearlyEqual(transform.rotation, authoritativeState.rotation, rotationErrorThreshold);
		if (!positionDiffers && !rotationDiffers)
			return false;

		transform.position = authoritativeState.position;
		transform.rotation = authoritativeState.rotation;
		return true;
	}

	SnapshotInterpolationBuffer::SnapshotInterpolationBuffer(std::size_t maxSnapshots)
		: m_MaxSnapshots(maxSnapshots)
	{
	}

	void SnapshotInterpolationBuffer::PushSnapshot(const WorldSnapshot& snapshot)
	{
		if (!m_Snapshots.empty() && snapshot.tick < m_Snapshots.back().tick)
		{
			auto it = std::lower_bound(
				m_Snapshots.begin(),
				m_Snapshots.end(),
				snapshot.tick,
				[](const WorldSnapshot& lhs, NetTick tick)
				{
					return lhs.tick < tick;
				});
			m_Snapshots.insert(it, snapshot);
		}
		else
		{
			m_Snapshots.push_back(snapshot);
		}

		while (m_Snapshots.size() > m_MaxSnapshots)
			m_Snapshots.pop_front();
	}

	bool SnapshotInterpolationBuffer::Sample(NetTick targetTick, ReplicatedEntityState& outState, std::uint32_t entityID) const
	{
		if (m_Snapshots.empty())
			return false;

		if (m_Snapshots.size() == 1)
			return TryGetEntityState(m_Snapshots.front(), entityID, outState);

		const WorldSnapshot* left = nullptr;
		const WorldSnapshot* right = nullptr;

		for (const auto& snapshot : m_Snapshots)
		{
			if (snapshot.tick <= targetTick)
				left = &snapshot;
			if (snapshot.tick >= targetTick)
			{
				right = &snapshot;
				break;
			}
		}

		if (!left)
			left = &m_Snapshots.front();
		if (!right)
			right = &m_Snapshots.back();

		ReplicatedEntityState leftState;
		if (!TryGetEntityState(*left, entityID, leftState))
			return false;

		if (left == right || left->tick == right->tick)
		{
			outState = leftState;
			return true;
		}

		ReplicatedEntityState rightState;
		if (!TryGetEntityState(*right, entityID, rightState))
		{
			outState = leftState;
			return true;
		}

		const float denom = static_cast<float>(right->tick - left->tick);
		if (denom <= 0.0001f)
		{
			outState = leftState;
			return true;
		}
		const float t = std::clamp((static_cast<float>(targetTick - left->tick) / denom), 0.0f, 1.0f);
		outState = LerpState(leftState, rightState, t);
		return true;
	}

	std::size_t SnapshotInterpolationBuffer::Size() const
	{
		return m_Snapshots.size();
	}
}
