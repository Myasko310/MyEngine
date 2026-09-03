#include "core/AnimationEventBus.h"

#include <algorithm>
#include <utility>

namespace MyEngine
{
	namespace
	{
		int g_NextToken = 1;
		std::vector<std::pair<int, AnimationEventBus::Handler>> g_Handlers;
		std::vector<AnimationEventMessage> g_RecentEvents;
		std::vector<AnimationEventActionMessage> g_QueuedActions;
	}

	void AnimationEventBus::BeginFrame()
	{
		g_RecentEvents.clear();
		g_QueuedActions.clear();
	}

	int AnimationEventBus::Subscribe(Handler handler)
	{
		if (!handler)
			return 0;
		const int token = g_NextToken++;
		g_Handlers.emplace_back(token, std::move(handler));
		return token;
	}

	void AnimationEventBus::Unsubscribe(int token)
	{
		if (token <= 0)
			return;
		g_Handlers.erase(
			std::remove_if(g_Handlers.begin(), g_Handlers.end(), [token](const auto& pair)
			{
				return pair.first == token;
			}),
			g_Handlers.end());
	}

	void AnimationEventBus::Publish(const AnimationEventMessage& message)
	{
		g_RecentEvents.push_back(message);
		for (const auto& [token, handler] : g_Handlers)
		{
			(void)token;
			if (handler)
				handler(message);
		}
	}

	const std::vector<AnimationEventMessage>& AnimationEventBus::GetRecentEvents()
	{
		return g_RecentEvents;
	}

	void AnimationEventBus::QueueAction(const AnimationEventActionMessage& action)
	{
		g_QueuedActions.push_back(action);
	}

	std::vector<AnimationEventActionMessage> AnimationEventBus::ConsumeQueuedActions()
	{
		auto actions = g_QueuedActions;
		g_QueuedActions.clear();
		return actions;
	}
}
