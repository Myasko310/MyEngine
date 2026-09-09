#pragma once

#include <cstddef>
#include <deque>

#include "network/NetTypes.h"

class Scene;
struct TransformComponent;

namespace MyEngine::Net
{
	struct ReplicationInterestSettings
	{
		bool enabled = false;
		float radius = 25.0f;
		glm::vec3 origin = glm::vec3(0.0f);
	};

	class NetReplicationSystem
	{
	public:
		static WorldSnapshot BuildSnapshot(const Scene& scene, NetTick tick);
		static WorldSnapshot BuildSnapshot(const Scene& scene, NetTick tick, const ReplicationInterestSettings& interestSettings);
		static WorldSnapshot BuildDeltaSnapshot(const WorldSnapshot& baseline, const WorldSnapshot& current);
		static bool ApplySnapshot(Scene& scene, const WorldSnapshot& snapshot);
		static bool ApplyDeltaSnapshot(Scene& scene, const WorldSnapshot& baseline, const WorldSnapshot& deltaSnapshot);
	};

	class ClientReconciliationState
	{
	public:
		void RecordPredictedInput(const InputCommand& input);
		void Acknowledge(NetTick serverProcessedTick);
		std::size_t PendingInputCount() const;
		void ReplayPredictedInputs(TransformComponent& transform, float moveSpeed, float fixedDeltaTime) const;

		bool ReconcileEntity(
			TransformComponent& transform,
			const ReplicatedEntityState& authoritativeState,
			float positionErrorThreshold = 0.001f,
			float rotationErrorThreshold = 0.001f) const;

	private:
		std::deque<InputCommand> m_PendingInputs;
	};

	class SnapshotInterpolationBuffer
	{
	public:
		explicit SnapshotInterpolationBuffer(std::size_t maxSnapshots = 32);
		void PushSnapshot(const WorldSnapshot& snapshot);
		bool Sample(NetTick targetTick, ReplicatedEntityState& outState, std::uint32_t entityID) const;
		std::size_t Size() const;

	private:
		std::size_t m_MaxSnapshots = 32;
		std::deque<WorldSnapshot> m_Snapshots;
	};
}
