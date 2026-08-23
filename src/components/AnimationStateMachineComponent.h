#pragma once

#include <memory>
#include <string>
#include <vector>

#include "animation/AnimationStateMachine.h"

struct AnimationStateMachineParameterValue
{
	float floatValue = 0.0f;
	bool boolValue = false;
	bool triggerValue = false;
};

struct AnimationStateMachineComponent
{
	std::shared_ptr<MyEngine::AnimationStateMachine> stateMachine = nullptr;
	std::string assetPath;
	std::vector<AnimationStateMachineParameterValue> parameterValues;
	int currentStateIndex = -1;
	int pendingStateIndex = -1;
	float currentStateTime = 0.0f;
	bool autoInitialize = true;
	bool debugPauseTransitions = false;

	void ResetRuntimeState()
	{
		currentStateIndex = -1;
		pendingStateIndex = -1;
		currentStateTime = 0.0f;
	}
	};
