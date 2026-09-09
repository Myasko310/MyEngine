#include "network/NetPluginHooks.h"

namespace MyEngine::Net
{
	std::vector<NetInterestFilter>& NetPluginRegistry::InterestFilters()
	{
		static std::vector<NetInterestFilter> filters;
		return filters;
	}

	std::vector<NetStateMutator>& NetPluginRegistry::StateMutators()
	{
		static std::vector<NetStateMutator> mutators;
		return mutators;
	}

	void NetPluginRegistry::RegisterInterestFilter(NetInterestFilter filter)
	{
		if (filter)
			InterestFilters().push_back(std::move(filter));
	}

	void NetPluginRegistry::RegisterStateMutator(NetStateMutator mutator)
	{
		if (mutator)
			StateMutators().push_back(std::move(mutator));
	}

	void NetPluginRegistry::Clear()
	{
		InterestFilters().clear();
		StateMutators().clear();
	}

	bool NetPluginRegistry::ShouldReplicateEntity(const Entity& entity, const ReplicatedEntityState& baseState)
	{
		for (const auto& filter : InterestFilters())
		{
			if (!filter(entity, baseState))
				return false;
		}
		return true;
	}

	void NetPluginRegistry::ApplyStateMutators(const Entity& entity, ReplicatedEntityState& state)
	{
		for (const auto& mutator : StateMutators())
			mutator(entity, state);
	}
}
