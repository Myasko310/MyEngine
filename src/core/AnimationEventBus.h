#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MyEngine
{
	struct AnimationEventMessage
	{
		std::uint32_t entityID = 0;
		std::string entityName;
		std::string eventName;
		float eventTimeSeconds = 0.0f;
	};

	struct AnimationEventActionMessage
	{
		std::uint32_t entityID = 0;
		std::string eventName;
		bool triggerAudio = false;
		std::string audioClipPath;
		float audioVolume = 1.0f;
		float audioPitch = 1.0f;
		bool triggerParticleBurst = false;
		int particleBurstCount = 0;
		bool triggerScriptCallback = false;
		std::string scriptCallbackName;
	};

	class AnimationEventBus
	{
	public:
		using Handler = std::function<void(const AnimationEventMessage&)>;

		static void BeginFrame();
		static int Subscribe(Handler handler);
		static void Unsubscribe(int token);
		static void Publish(const AnimationEventMessage& message);
		static const std::vector<AnimationEventMessage>& GetRecentEvents();
		static void QueueAction(const AnimationEventActionMessage& action);
		static std::vector<AnimationEventActionMessage> ConsumeQueuedActions();
	};
}
