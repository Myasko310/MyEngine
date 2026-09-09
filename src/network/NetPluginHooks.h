#pragma once

#include <functional>
#include <vector>

#include "network/NetTypes.h"

class Entity;

namespace MyEngine::Net
{
	using NetInterestFilter = std::function<bool(const Entity&, const ReplicatedEntityState&)>;
	using NetStateMutator = std::function<void(const Entity&, ReplicatedEntityState&)>;

	class NetPluginRegistry
	{
	public:
		static void RegisterInterestFilter(NetInterestFilter filter);
		static void RegisterStateMutator(NetStateMutator mutator);
		static void Clear();

		static bool ShouldReplicateEntity(const Entity& entity, const ReplicatedEntityState& baseState);
		static void ApplyStateMutators(const Entity& entity, ReplicatedEntityState& state);

	private:
		static std::vector<NetInterestFilter>& InterestFilters();
		static std::vector<NetStateMutator>& StateMutators();
	};
}
