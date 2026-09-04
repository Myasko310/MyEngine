#include "animation/AnimationStateMachine.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace
{
	const char* ToString(MyEngine::AnimationStateMachineParameterType type)
	{
		switch (type)
		{
		case MyEngine::AnimationStateMachineParameterType::Bool: return "Bool";
		case MyEngine::AnimationStateMachineParameterType::Float: return "Float";
		case MyEngine::AnimationStateMachineParameterType::Trigger: return "Trigger";
		default: return "Bool";
		}
	}

	const char* ToString(MyEngine::AnimationStateMachineConditionOperator op)
	{
		switch (op)
		{
		case MyEngine::AnimationStateMachineConditionOperator::IfTrue: return "IfTrue";
		case MyEngine::AnimationStateMachineConditionOperator::IfFalse: return "IfFalse";
		case MyEngine::AnimationStateMachineConditionOperator::Greater: return "Greater";
		case MyEngine::AnimationStateMachineConditionOperator::Less: return "Less";
		case MyEngine::AnimationStateMachineConditionOperator::Trigger: return "Trigger";
		default: return "IfTrue";
		}
	}

	MyEngine::AnimationStateMachineParameterType ParseParameterType(const rapidjson::Value& value)
	{
		if (!value.IsString())
			return MyEngine::AnimationStateMachineParameterType::Bool;

		std::string text = value.GetString();
		if (text == "Float")
			return MyEngine::AnimationStateMachineParameterType::Float;
		if (text == "Trigger")
			return MyEngine::AnimationStateMachineParameterType::Trigger;
		return MyEngine::AnimationStateMachineParameterType::Bool;
	}

	MyEngine::AnimationStateMachineConditionOperator ParseConditionOperator(const rapidjson::Value& value)
	{
		if (!value.IsString())
			return MyEngine::AnimationStateMachineConditionOperator::IfTrue;

		std::string text = value.GetString();
		if (text == "IfFalse")
			return MyEngine::AnimationStateMachineConditionOperator::IfFalse;
		if (text == "Greater")
			return MyEngine::AnimationStateMachineConditionOperator::Greater;
		if (text == "Less")
			return MyEngine::AnimationStateMachineConditionOperator::Less;
		if (text == "Trigger")
			return MyEngine::AnimationStateMachineConditionOperator::Trigger;
		return MyEngine::AnimationStateMachineConditionOperator::IfTrue;
	}
}

namespace MyEngine
{
	bool AnimationStateMachine::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return false;

		std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		rapidjson::Document doc;
		doc.Parse(json.c_str());
		if (doc.HasParseError() || !doc.IsObject())
			return false;

		m_Path = path;
		name = doc.HasMember("name") && doc["name"].IsString()
			? doc["name"].GetString()
			: std::string();
		defaultStateIndex = doc.HasMember("defaultStateIndex") && doc["defaultStateIndex"].IsInt()
			? doc["defaultStateIndex"].GetInt()
			: 0;
		parameters.clear();
		states.clear();

		if (doc.HasMember("parameters") && doc["parameters"].IsArray())
		{
			for (const auto& paramValue : doc["parameters"].GetArray())
			{
				if (!paramValue.IsObject())
					continue;

				AnimationStateMachineParameter param;
				if (paramValue.HasMember("name") && paramValue["name"].IsString())
					param.name = paramValue["name"].GetString();
				if (paramValue.HasMember("type"))
					param.type = ParseParameterType(paramValue["type"]);
				if (paramValue.HasMember("defaultFloatValue") && paramValue["defaultFloatValue"].IsNumber())
					param.defaultFloatValue = paramValue["defaultFloatValue"].GetFloat();
				if (paramValue.HasMember("defaultBoolValue") && paramValue["defaultBoolValue"].IsBool())
					param.defaultBoolValue = paramValue["defaultBoolValue"].GetBool();
				parameters.push_back(std::move(param));
			}
		}

		if (doc.HasMember("states") && doc["states"].IsArray())
		{
			for (const auto& stateValue : doc["states"].GetArray())
			{
				if (!stateValue.IsObject())
					continue;

				AnimationStateMachineState state;
				if (stateValue.HasMember("name") && stateValue["name"].IsString())
					state.name = stateValue["name"].GetString();
				if (stateValue.HasMember("clipName") && stateValue["clipName"].IsString())
					state.clipName = stateValue["clipName"].GetString();
				if (stateValue.HasMember("loop") && stateValue["loop"].IsBool())
					state.loop = stateValue["loop"].GetBool();
				if (stateValue.HasMember("playbackSpeed") && stateValue["playbackSpeed"].IsNumber())
					state.playbackSpeed = stateValue["playbackSpeed"].GetFloat();

				if (stateValue.HasMember("transitions") && stateValue["transitions"].IsArray())
				{
					for (const auto& transitionValue : stateValue["transitions"].GetArray())
					{
						if (!transitionValue.IsObject())
							continue;

						AnimationStateMachineTransition transition;
						if (transitionValue.HasMember("targetStateIndex") && transitionValue["targetStateIndex"].IsInt())
							transition.targetStateIndex = transitionValue["targetStateIndex"].GetInt();
						if (transitionValue.HasMember("blendDuration") && transitionValue["blendDuration"].IsNumber())
							transition.blendDuration = transitionValue["blendDuration"].GetFloat();
						if (transitionValue.HasMember("requiresExitTime") && transitionValue["requiresExitTime"].IsBool())
							transition.requiresExitTime = transitionValue["requiresExitTime"].GetBool();
						if (transitionValue.HasMember("exitTimeNormalized") && transitionValue["exitTimeNormalized"].IsNumber())
							transition.exitTimeNormalized = transitionValue["exitTimeNormalized"].GetFloat();
						if (transitionValue.HasMember("resetTimeOnEnter") && transitionValue["resetTimeOnEnter"].IsBool())
							transition.resetTimeOnEnter = transitionValue["resetTimeOnEnter"].GetBool();

						if (transitionValue.HasMember("conditions") && transitionValue["conditions"].IsArray())
						{
							for (const auto& conditionValue : transitionValue["conditions"].GetArray())
							{
								if (!conditionValue.IsObject())
									continue;

								AnimationStateMachineCondition condition;
								if (conditionValue.HasMember("parameterName") && conditionValue["parameterName"].IsString())
									condition.parameterName = conditionValue["parameterName"].GetString();
								if (conditionValue.HasMember("op"))
									condition.op = ParseConditionOperator(conditionValue["op"]);
								if (conditionValue.HasMember("threshold") && conditionValue["threshold"].IsNumber())
									condition.threshold = conditionValue["threshold"].GetFloat();
								transition.conditions.push_back(std::move(condition));
							}
						}

						state.transitions.push_back(std::move(transition));
					}
				}

				states.push_back(std::move(state));
			}
		}

		if (states.empty())
			defaultStateIndex = -1;
		else
			defaultStateIndex = std::clamp(defaultStateIndex, 0, static_cast<int>(states.size()) - 1);

		return true;
	}

	bool AnimationStateMachine::SaveToFile(const std::string& path) const
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("name");
		writer.String(name.c_str());
		writer.Key("defaultStateIndex");
		writer.Int(defaultStateIndex);

		writer.Key("parameters");
		writer.StartArray();
		for (const auto& param : parameters)
		{
			writer.StartObject();
			writer.Key("name"); writer.String(param.name.c_str());
			writer.Key("type"); writer.String(ToString(param.type));
			writer.Key("defaultFloatValue"); writer.Double(param.defaultFloatValue);
			writer.Key("defaultBoolValue"); writer.Bool(param.defaultBoolValue);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("states");
		writer.StartArray();
		for (const auto& state : states)
		{
			writer.StartObject();
			writer.Key("name"); writer.String(state.name.c_str());
			writer.Key("clipName"); writer.String(state.clipName.c_str());
			writer.Key("loop"); writer.Bool(state.loop);
			writer.Key("playbackSpeed"); writer.Double(state.playbackSpeed);
			writer.Key("transitions");
			writer.StartArray();
			for (const auto& transition : state.transitions)
			{
				writer.StartObject();
				writer.Key("targetStateIndex"); writer.Int(transition.targetStateIndex);
				writer.Key("blendDuration"); writer.Double(transition.blendDuration);
				writer.Key("requiresExitTime"); writer.Bool(transition.requiresExitTime);
				writer.Key("exitTimeNormalized"); writer.Double(transition.exitTimeNormalized);
				writer.Key("resetTimeOnEnter"); writer.Bool(transition.resetTimeOnEnter);
				writer.Key("conditions");
				writer.StartArray();
				for (const auto& condition : transition.conditions)
				{
					writer.StartObject();
					writer.Key("parameterName"); writer.String(condition.parameterName.c_str());
					writer.Key("op"); writer.String(ToString(condition.op));
					writer.Key("threshold"); writer.Double(condition.threshold);
					writer.EndObject();
				}
				writer.EndArray();
				writer.EndObject();
			}
			writer.EndArray();
			writer.EndObject();
		}
		writer.EndArray();
		writer.EndObject();

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;

		out << buffer.GetString();
		return static_cast<bool>(out);
	}

	int AnimationStateMachine::FindStateIndex(const std::string& stateName) const
	{
		for (int i = 0; i < static_cast<int>(states.size()); ++i)
		{
			if (states[i].name == stateName)
				return i;
		}
		return -1;
	}

	int AnimationStateMachine::FindParameterIndex(const std::string& parameterName) const
	{
		for (int i = 0; i < static_cast<int>(parameters.size()); ++i)
		{
			if (parameters[i].name == parameterName)
				return i;
		}
		return -1;
	}

	int AnimationStateMachine::ResolveClipIndex(const std::vector<AnimationClip>& clips, const AnimationStateMachineState& state) const
	{
		if (state.clipName.empty())
			return -1;

		auto equalsIgnoreCase = [](const std::string& a, const std::string& b)
		{
			if (a.size() != b.size())
				return false;
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
					return false;
			}
			return true;
		};

		for (int i = 0; i < static_cast<int>(clips.size()); ++i)
		{
			if (clips[i].name == state.clipName)
				return i;
		}
		for (int i = 0; i < static_cast<int>(clips.size()); ++i)
		{
			if (equalsIgnoreCase(clips[i].name, state.clipName))
				return i;
		}
		return -1;
	}

	bool AnimationStateMachine::IsValidStateIndex(int stateIndex) const
	{
		return stateIndex >= 0 && stateIndex < static_cast<int>(states.size());
	}
}
