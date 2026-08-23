#pragma once

#include <memory>
#include <string>
#include <vector>

#include "rendering/AnimationClip.h"

namespace MyEngine
{
	enum class AnimationStateMachineParameterType
	{
		Bool = 0,
		Float = 1,
		Trigger = 2
	};

	enum class AnimationStateMachineConditionOperator
	{
		IfTrue = 0,
		IfFalse = 1,
		Greater = 2,
		Less = 3,
		Trigger = 4
	};

	struct AnimationStateMachineParameter
	{
		std::string name;
		AnimationStateMachineParameterType type = AnimationStateMachineParameterType::Bool;
		float defaultFloatValue = 0.0f;
		bool defaultBoolValue = false;
	};

	struct AnimationStateMachineCondition
	{
		std::string parameterName;
		AnimationStateMachineConditionOperator op = AnimationStateMachineConditionOperator::IfTrue;
		float threshold = 0.0f;
	};

	struct AnimationStateMachineTransition
	{
		int targetStateIndex = -1;
		float blendDuration = 0.2f;
		bool requiresExitTime = false;
		float exitTimeNormalized = 1.0f;
		bool resetTimeOnEnter = true;
		std::vector<AnimationStateMachineCondition> conditions;
	};

	struct AnimationStateMachineState
	{
		std::string name;
		std::string clipName;
		bool loop = true;
		float playbackSpeed = 1.0f;
		std::vector<AnimationStateMachineTransition> transitions;
	};

	class AnimationStateMachine
	{
	public:
		AnimationStateMachine() = default;
		explicit AnimationStateMachine(std::string path)
			: m_Path(std::move(path))
		{
		}

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		int FindStateIndex(const std::string& stateName) const;
		int FindParameterIndex(const std::string& parameterName) const;
		int ResolveClipIndex(const std::vector<AnimationClip>& clips, const AnimationStateMachineState& state) const;
		bool IsValidStateIndex(int stateIndex) const;

		std::string name;
		int defaultStateIndex = 0;
		std::vector<AnimationStateMachineParameter> parameters;
		std::vector<AnimationStateMachineState> states;

	private:
		std::string m_Path;
	};
}
