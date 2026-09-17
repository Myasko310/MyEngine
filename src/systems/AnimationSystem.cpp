#include "systems/AnimationSystem.h"

#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/SkeletonComponent.h"
#include "components/TransformComponent.h"
#include "components/CharacterControllerComponent.h"
#include "core/AnimationEventBus.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_set>

namespace MyEngine
{
	static void EnsureStateMachineParameterDefaults(AnimationStateMachineComponent& stateMachineComponent)
	{
		if (!stateMachineComponent.stateMachine)
			return;

		auto& runtimeValues = stateMachineComponent.parameterValues;
		const auto& parameters = stateMachineComponent.stateMachine->parameters;
		const size_t previousSize = runtimeValues.size();
		if (previousSize != parameters.size())
			runtimeValues.resize(parameters.size());

		for (size_t i = previousSize; i < parameters.size(); ++i)
		{
			runtimeValues[i].floatValue = parameters[i].defaultFloatValue;
			runtimeValues[i].boolValue = parameters[i].defaultBoolValue;
			runtimeValues[i].triggerValue = false;
		}
	}

	static bool EvaluateTransitionCondition(
		const AnimationStateMachine& stateMachine,
		AnimationStateMachineComponent& stateMachineComponent,
		const AnimationStateMachineCondition& condition,
		std::string* outReason = nullptr)
	{
		int parameterIndex = stateMachine.FindParameterIndex(condition.parameterName);
		if (parameterIndex < 0 || parameterIndex >= static_cast<int>(stateMachineComponent.parameterValues.size()))
		{
			if (outReason)
				*outReason = "Missing parameter '" + condition.parameterName + "'";
			return false;
		}

		const auto& parameter = stateMachine.parameters[parameterIndex];
		auto& value = stateMachineComponent.parameterValues[parameterIndex];
		switch (condition.op)
		{
		case AnimationStateMachineConditionOperator::IfTrue:
			if (!value.boolValue && outReason)
				*outReason = condition.parameterName + " must be true";
			return value.boolValue;
		case AnimationStateMachineConditionOperator::IfFalse:
			if (value.boolValue && outReason)
				*outReason = condition.parameterName + " must be false";
			return !value.boolValue;
		case AnimationStateMachineConditionOperator::Greater:
			if (!(value.floatValue > condition.threshold) && outReason)
				*outReason = condition.parameterName + " must be > " + std::to_string(condition.threshold);
			return value.floatValue > condition.threshold;
		case AnimationStateMachineConditionOperator::Less:
			if (!(value.floatValue < condition.threshold) && outReason)
				*outReason = condition.parameterName + " must be < " + std::to_string(condition.threshold);
			return value.floatValue < condition.threshold;
		case AnimationStateMachineConditionOperator::Trigger:
			if (parameter.type != AnimationStateMachineParameterType::Trigger)
			{
				if (outReason)
					*outReason = condition.parameterName + " is not a trigger parameter";
				return false;
			}
			if (!value.triggerValue && outReason)
				*outReason = condition.parameterName + " trigger not armed";
			return value.triggerValue;
		default:
			if (outReason)
				*outReason = "Unknown condition";
			return false;
		}
	}

	static void ComputeTrimmedTimeRangeSeconds(
		const AnimationStateMachineState& state,
		const AnimationClip* clip,
		float& outStartSeconds,
		float& outEndSeconds)
	{
		outStartSeconds = 0.0f;
		outEndSeconds = 0.0f;
		if (!clip)
			return;

		const float durationSeconds = clip->GetDurationSeconds();
		if (durationSeconds <= 0.0001f)
			return;

		float startNormalized = std::clamp(state.trimStartNormalized, 0.0f, 1.0f);
		float endNormalized = std::clamp(state.trimEndNormalized, 0.0f, 1.0f);
		if (endNormalized < startNormalized)
			std::swap(startNormalized, endNormalized);

		outStartSeconds = startNormalized * durationSeconds;
		outEndSeconds = endNormalized * durationSeconds;

		// Avoid ultra-short trim windows that make clips appear to replay/flicker
		// many times per second when looping.
		constexpr float kMinTrimWindowSeconds = 1.0f / 30.0f;
		if (outEndSeconds - outStartSeconds < kMinTrimWindowSeconds)
		{
			outEndSeconds = std::min(durationSeconds, outStartSeconds + kMinTrimWindowSeconds);
			if (outEndSeconds - outStartSeconds < kMinTrimWindowSeconds)
				outStartSeconds = std::max(0.0f, outEndSeconds - kMinTrimWindowSeconds);
		}
	}

	static bool ShouldTakeTransition(
		const AnimationStateMachine& stateMachine,
		AnimationStateMachineComponent& stateMachineComponent,
		const AnimationStateMachineTransition& transition,
		const AnimationComponent& anim,
		const AnimationStateMachineState& currentState,
		const AnimationClip* currentClip,
		std::string* outReason = nullptr)
	{
		if (!stateMachine.IsValidStateIndex(transition.targetStateIndex))
		{
			if (outReason)
				*outReason = "Target state index is invalid";
			return false;
		}

		if (transition.requiresExitTime && currentClip)
		{
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = 0.0f;
			ComputeTrimmedTimeRangeSeconds(currentState, currentClip, trimStartSeconds, trimEndSeconds);
			const float trimmedDurationSeconds = trimEndSeconds - trimStartSeconds;
			if (trimmedDurationSeconds > 0.0001f)
			{
				float normalizedTime = std::clamp((anim.time - trimStartSeconds) / trimmedDurationSeconds, 0.0f, 1.0f);
				if (normalizedTime < std::clamp(transition.exitTimeNormalized, 0.0f, 1.0f))
				{
					if (outReason)
						*outReason = "Waiting for exit time " + std::to_string(transition.exitTimeNormalized);
					return false;
				}
			}
		}

		for (const auto& condition : transition.conditions)
		{
			std::string conditionReason;
			if (!EvaluateTransitionCondition(stateMachine, stateMachineComponent, condition, &conditionReason))
			{
				if (outReason)
					*outReason = conditionReason;
				return false;
			}
		}

		(void)currentState;
		return true;
	}

	static void ConsumeTriggeredParameters(
		const AnimationStateMachine& stateMachine,
		AnimationStateMachineComponent& stateMachineComponent,
		const AnimationStateMachineTransition& transition)
	{
		for (const auto& condition : transition.conditions)
		{
			if (condition.op != AnimationStateMachineConditionOperator::Trigger)
				continue;

			int parameterIndex = stateMachine.FindParameterIndex(condition.parameterName);
			if (parameterIndex < 0 || parameterIndex >= static_cast<int>(stateMachineComponent.parameterValues.size()))
				continue;

			stateMachineComponent.parameterValues[parameterIndex].triggerValue = false;
		}
	}

	static float GetEffectiveStatePlaybackSpeed(const AnimationStateMachineState& state)
	{
		// Any near-zero playback speed can make trimmed playback appear stuck or
		// repeatedly replay tiny windows, so enforce a small positive runtime floor.
		(void)state;
		return std::max(0.01f, state.playbackSpeed);
	}

	static void UpdateAnimationStateMachine(AnimationComponent& anim, AnimationStateMachineComponent& stateMachineComponent)
	{
		if (!stateMachineComponent.stateMachine || !anim.clips || anim.clips->empty())
			return;

		EnsureStateMachineParameterDefaults(stateMachineComponent);
		AnimationStateMachine& stateMachine = *stateMachineComponent.stateMachine;
		stateMachineComponent.debugTransitionMessages.clear();
		stateMachineComponent.debugLastBlockedReason.clear();
		stateMachineComponent.debugLastBlockedTransitionIndex = -1;
		stateMachineComponent.debugPendingStateName.clear();
		if (stateMachine.states.empty())
			return;

		if (!stateMachine.IsValidStateIndex(stateMachineComponent.currentStateIndex))
		{
			stateMachineComponent.currentStateIndex = stateMachine.defaultStateIndex;
			stateMachineComponent.currentStateTime = 0.0f;
		}
		if (!stateMachine.IsValidStateIndex(stateMachineComponent.currentStateIndex))
			return;

		const AnimationStateMachineState& currentState = stateMachine.states[stateMachineComponent.currentStateIndex];
		stateMachineComponent.debugCurrentStateName = currentState.name;
		int resolvedClipIndex = stateMachine.ResolveClipIndex(*anim.clips, currentState);
		if (resolvedClipIndex >= 0 && resolvedClipIndex != anim.activeClipIndex)
		{
			anim.TransitionTo(resolvedClipIndex, 0.15f);
			anim.looping = currentState.loop;
			anim.playbackSpeed = GetEffectiveStatePlaybackSpeed(currentState);
			stateMachineComponent.currentStateTime = 0.0f;
			const AnimationClip* currentStateClip = (resolvedClipIndex >= 0 && resolvedClipIndex < static_cast<int>(anim.clips->size()))
				? &(*anim.clips)[resolvedClipIndex]
				: nullptr;
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = 0.0f;
			ComputeTrimmedTimeRangeSeconds(currentState, currentStateClip, trimStartSeconds, trimEndSeconds);
			anim.time = trimStartSeconds;
		}
		else
		{
			anim.looping = currentState.loop;
			anim.playbackSpeed = GetEffectiveStatePlaybackSpeed(currentState);

			// If state trim/speed was edited while this state kept the same clip,
			// realign playback to the new trim window at state start instead of
			// carrying stale clip time from the previous authored range.
			const AnimationClip* currentStateClip = (resolvedClipIndex >= 0 && resolvedClipIndex < static_cast<int>(anim.clips->size()))
				? &(*anim.clips)[resolvedClipIndex]
				: nullptr;
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = 0.0f;
			ComputeTrimmedTimeRangeSeconds(currentState, currentStateClip, trimStartSeconds, trimEndSeconds);
			if (anim.time < trimStartSeconds - 0.0001f || anim.time > trimEndSeconds + 0.0001f)
				anim.time = trimStartSeconds;
		}

		if (stateMachineComponent.debugPauseTransitions)
		{
			stateMachineComponent.debugTransitionMessages.push_back("Transitions paused for debugging.");
			return;
		}

		const AnimationClip* currentClip = (resolvedClipIndex >= 0 && resolvedClipIndex < static_cast<int>(anim.clips->size()))
			? &(*anim.clips)[resolvedClipIndex]
			: nullptr;
		bool tookTransition = false;
		for (size_t transitionIndex = 0; transitionIndex < currentState.transitions.size(); ++transitionIndex)
		{
			const auto& transition = currentState.transitions[transitionIndex];
			std::string transitionReason;
			bool canTransition = ShouldTakeTransition(stateMachine, stateMachineComponent, transition, anim, currentState, currentClip, &transitionReason);
			std::string targetName = stateMachine.IsValidStateIndex(transition.targetStateIndex)
				? stateMachine.states[transition.targetStateIndex].name
				: ("State " + std::to_string(transition.targetStateIndex));
			stateMachineComponent.debugTransitionMessages.push_back(
				"[" + std::to_string(static_cast<int>(transitionIndex)) + "] -> " + targetName + ": " +
				(canTransition ? std::string("ready") : transitionReason));

			if (!canTransition)
			{
				if (stateMachineComponent.debugLastBlockedTransitionIndex < 0)
				{
					stateMachineComponent.debugLastBlockedTransitionIndex = static_cast<int>(transitionIndex);
					stateMachineComponent.debugLastBlockedReason = transitionReason;
				}
				continue;
			}

			const AnimationStateMachineState& nextState = stateMachine.states[transition.targetStateIndex];
			const int nextClipIndex = stateMachine.ResolveClipIndex(*anim.clips, nextState);
			if (nextClipIndex < 0)
			{
				const std::string missingClipReason = "Clip '" + nextState.clipName + "' was not found for state '" + nextState.name + "'";
				stateMachineComponent.debugTransitionMessages.back() =
					"[" + std::to_string(static_cast<int>(transitionIndex)) + "] -> " + targetName + ": " + missingClipReason;
				if (stateMachineComponent.debugLastBlockedTransitionIndex < 0)
				{
					stateMachineComponent.debugLastBlockedTransitionIndex = static_cast<int>(transitionIndex);
					stateMachineComponent.debugLastBlockedReason = missingClipReason;
				}
				continue;
			}

			stateMachineComponent.pendingStateIndex = transition.targetStateIndex;
			stateMachineComponent.currentStateIndex = transition.targetStateIndex;
			stateMachineComponent.currentStateTime = transition.resetTimeOnEnter ? 0.0f : anim.time;
			stateMachineComponent.debugPendingStateName = nextState.name;
			anim.looping = nextState.loop;
			anim.playbackSpeed = GetEffectiveStatePlaybackSpeed(nextState);
			const bool sameClipTransition = (nextClipIndex == anim.activeClipIndex);
			if (!sameClipTransition)
			{
				anim.TransitionTo(nextClipIndex, transition.blendDuration);
			}
			else
			{
				// Same-clip state transitions (common when authoring trim/speed variants)
				// should not cross-fade from the clip onto itself; it causes stale time
				// carryover and visible early-frame replay/freeze artifacts.
				anim.blending = false;
				anim.previousClipIndex = -1;
				anim.blendElapsed = 0.0f;
				anim.blendDuration = 0.0f;
			}
			const AnimationClip* nextClip = (nextClipIndex >= 0 && nextClipIndex < static_cast<int>(anim.clips->size()))
				? &(*anim.clips)[nextClipIndex]
				: nullptr;
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = 0.0f;
			ComputeTrimmedTimeRangeSeconds(nextState, nextClip, trimStartSeconds, trimEndSeconds);
			if (!transition.resetTimeOnEnter)
				anim.time = std::clamp(stateMachineComponent.currentStateTime, trimStartSeconds, trimEndSeconds);
			else
				anim.time = trimStartSeconds;

			stateMachineComponent.debugSelectedTransitionIndex = static_cast<int>(transitionIndex);
			ConsumeTriggeredParameters(stateMachine, stateMachineComponent, transition);
			tookTransition = true;
			break;
		}

		// Safety fallback: if a non-looping state reaches its trim end and no
		// transition fired (e.g. after editing trim/speed), return to default
		// state so playback never hard-freezes.
		if (!tookTransition && currentClip && !currentState.loop)
		{
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = 0.0f;
			ComputeTrimmedTimeRangeSeconds(currentState, currentClip, trimStartSeconds, trimEndSeconds);
			if (anim.time >= (trimEndSeconds - 0.0001f) && stateMachine.IsValidStateIndex(stateMachine.defaultStateIndex))
			{
				const auto& fallbackState = stateMachine.states[stateMachine.defaultStateIndex];
				const int fallbackClipIndex = stateMachine.ResolveClipIndex(*anim.clips, fallbackState);
				if (fallbackClipIndex >= 0)
				{
					stateMachineComponent.currentStateIndex = stateMachine.defaultStateIndex;
					stateMachineComponent.pendingStateIndex = -1;
					stateMachineComponent.currentStateTime = 0.0f;
					stateMachineComponent.debugPendingStateName = fallbackState.name;
					anim.looping = fallbackState.loop;
					anim.playbackSpeed = GetEffectiveStatePlaybackSpeed(fallbackState);
					anim.TransitionTo(fallbackClipIndex, 0.1f);
					anim.time = 0.0f;
				}
			}
		}
	}
	// Samples a single bone's local transform (translate * rotate * scale)
	// from a clip's track at the given time, falling back to the bind-pose
	// local transform if the clip doesn't animate this bone.
	static glm::mat4 SampleLocalTransform(
		const Bone& bone,
		const AnimationClip* clip,
		float timeTicks,
		bool removeRootMotionTranslation,
		bool preserveRootMotionY)
	{
		if (!clip)
			return bone.localBindTransform;

		// Use normalized lookup to handle case/spacing/special-character mismatches
		const BoneAnimationTrack* track = clip->FindTrackNormalized(bone.name);
		if (!track)
		{
			// Silently fall back to bind pose for bones without animation
			return bone.localBindTransform;
		}

		glm::vec3 pos = track->SamplePosition(timeTicks);
		if (removeRootMotionTranslation)
		{
			pos.x = 0.0f;
			pos.z = 0.0f;
			if (!preserveRootMotionY)
				pos.y = 0.0f;
		}
		glm::quat rot = track->SampleRotation(timeTicks);
		glm::vec3 scale = track->SampleScale(timeTicks);

		glm::mat4 sampled = glm::translate(glm::mat4(1.0f), pos) *
			glm::mat4_cast(rot) *
			glm::scale(glm::mat4(1.0f), scale);

		return bone.skippedNodeLocalPrefix * sampled;
	}

	// Decomposes a local transform matrix back into translate/rotate/scale so
	// two poses (from potentially different clips) can be blended component-
	// wise (lerp position/scale, slerp rotation) rather than blending raw
	// matrices, which would produce incorrect interpolation.
	static void DecomposeTRS(const glm::mat4& m, glm::vec3& outPos, glm::quat& outRot, glm::vec3& outScale)
	{
		outPos = glm::vec3(m[3]);

		glm::vec3 col0(m[0]), col1(m[1]), col2(m[2]);
		outScale = glm::vec3(glm::length(col0), glm::length(col1), glm::length(col2));

		glm::mat3 rotMat(
			outScale.x > 0.0001f ? col0 / outScale.x : col0,
			outScale.y > 0.0001f ? col1 / outScale.y : col1,
			outScale.z > 0.0001f ? col2 / outScale.z : col2
		);
		outRot = glm::normalize(glm::quat_cast(rotMat));
	}

	// Blends two local transforms component-wise by `t` (0 = fromMatrix, 1 = toMatrix).
	static glm::mat4 BlendLocalTransforms(const glm::mat4& fromMatrix, const glm::mat4& toMatrix, float t)
	{
		glm::vec3 fromPos, fromScale, toPos, toScale;
		glm::quat fromRot, toRot;
		DecomposeTRS(fromMatrix, fromPos, fromRot, fromScale);
		DecomposeTRS(toMatrix, toPos, toRot, toScale);

		glm::vec3 pos = glm::mix(fromPos, toPos, t);
		glm::vec3 scale = glm::mix(fromScale, toScale, t);
		glm::quat rot = glm::normalize(glm::slerp(fromRot, toRot, t));

		return glm::translate(glm::mat4(1.0f), pos) *
			glm::mat4_cast(rot) *
			glm::scale(glm::mat4(1.0f), scale);
	}

	static int ResolveRootMotionBoneIndex(const Skeleton& skeleton, const std::string& preferredBoneName)
	{
		const auto& bones = skeleton.GetBones();
		if (bones.empty())
			return -1;

		if (!preferredBoneName.empty())
		{
			int index = skeleton.GetBoneIndex(preferredBoneName);
			if (index >= 0)
				return index;
		}

		return 0;
	}

	static glm::vec3 SampleRootMotionPosition(const Skeleton& skeleton, int rootBoneIndex, const AnimationClip* clip, float timeTicks)
	{
		if (!clip || rootBoneIndex < 0)
			return glm::vec3(0.0f);

		const auto& bones = skeleton.GetBones();
		if (rootBoneIndex >= static_cast<int>(bones.size()))
			return glm::vec3(0.0f);

		const BoneAnimationTrack* track = clip->FindTrackNormalized(bones[rootBoneIndex].name);
		if (!track)
			return glm::vec3(0.0f);

		return track->SamplePosition(timeTicks);
	}

	static glm::vec3 ComputeRootMotionDelta(
		const Skeleton& skeleton,
		int rootBoneIndex,
		const AnimationClip& clip,
		float previousTimeSeconds,
		float currentTimeSeconds,
		bool looping)
	{
		if (rootBoneIndex < 0)
			return glm::vec3(0.0f);

		const float durationSeconds = clip.GetDurationSeconds();
		if (durationSeconds <= 0.0001f)
			return glm::vec3(0.0f);

		const float previousTicks = previousTimeSeconds * clip.ticksPerSecond;
		const float currentTicks = currentTimeSeconds * clip.ticksPerSecond;

		const glm::vec3 previousPos = SampleRootMotionPosition(skeleton, rootBoneIndex, &clip, previousTicks);
		const glm::vec3 currentPos = SampleRootMotionPosition(skeleton, rootBoneIndex, &clip, currentTicks);

		if (looping && currentTimeSeconds < previousTimeSeconds)
		{
			const glm::vec3 endPos = SampleRootMotionPosition(skeleton, rootBoneIndex, &clip, clip.durationTicks);
			const glm::vec3 startPos = SampleRootMotionPosition(skeleton, rootBoneIndex, &clip, 0.0f);
			return (endPos - previousPos) + (currentPos - startPos);
		}

		return currentPos - previousPos;
	}

	// Recursively computes each bone's final skinning matrix:
	//   finalMatrix[bone] = parentAccumulatedTransform * localAnimatedTransform * offsetMatrix
	// `localAnimatedTransform` comes from the sampled clip track for this bone
	// (falling back to its bind-pose local transform if the clip doesn't
	// animate it), and `offsetMatrix` converts from bind-pose mesh space into
	// the bone's own space so multiplying by the vertex position skins it correctly.
	static void ComputeBoneMatrices(
		const Skeleton& skeleton,
		const AnimationClip* clip,
		float timeTicks,
		int rootMotionBoneIndex,
		std::vector<glm::mat4>& outMatrices)
	{
		const auto& bones = skeleton.GetBones();
		std::vector<glm::mat4> accumulated(bones.size(), glm::mat4(1.0f));

		outMatrices.assign(bones.size(), glm::mat4(1.0f));

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const Bone& bone = bones[i];

			const bool removeRootTranslation = static_cast<int>(i) == rootMotionBoneIndex;
			glm::mat4 localTransform = SampleLocalTransform(bone, clip, timeTicks, removeRootTranslation, true);

			glm::mat4 parentTransform = bone.parentIndex >= 0
				? accumulated[bone.parentIndex]
				: glm::mat4(1.0f);

			accumulated[i] = parentTransform * localTransform;
			outMatrices[i] = accumulated[i] * bone.offsetMatrix;
		}
	}

	// Same as ComputeBoneMatrices, but blends each bone's local transform
	// between `fromClip`@fromTimeTicks and `toClip`@toTimeTicks using weight
	// `blendT` (0 = fully fromClip, 1 = fully toClip) before accumulating
	// parent transforms. Used during cross-fade transitions.
	static void ComputeBlendedBoneMatrices(
		const Skeleton& skeleton,
		const AnimationClip* fromClip,
		float fromTimeTicks,
		const AnimationClip* toClip,
		float toTimeTicks,
		float blendT,
		int rootMotionBoneIndex,
		std::vector<glm::mat4>& outMatrices)
	{
		const auto& bones = skeleton.GetBones();
		std::vector<glm::mat4> accumulated(bones.size(), glm::mat4(1.0f));

		outMatrices.assign(bones.size(), glm::mat4(1.0f));

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const Bone& bone = bones[i];
			const bool removeRootTranslation = static_cast<int>(i) == rootMotionBoneIndex;

			glm::mat4 fromLocal = SampleLocalTransform(bone, fromClip, fromTimeTicks, removeRootTranslation, true);
			glm::mat4 toLocal = SampleLocalTransform(bone, toClip, toTimeTicks, removeRootTranslation, true);
			glm::mat4 localTransform = BlendLocalTransforms(fromLocal, toLocal, blendT);

			glm::mat4 parentTransform = bone.parentIndex >= 0
				? accumulated[bone.parentIndex]
				: glm::mat4(1.0f);

			accumulated[i] = parentTransform * localTransform;
			outMatrices[i] = accumulated[i] * bone.offsetMatrix;
		}
	}

	void AnimationSystem::Update(Scene& scene, float deltaTime)
	{
		AnimationEventBus::BeginFrame();
		auto collectAnimationEvents = [](AnimationComponent& anim, float previousTime, float currentTime, float durationSeconds)
		{
			anim.triggeredEventsThisFrame.clear();
			if (anim.events.empty() || durationSeconds <= 0.0001f)
				return;

			auto inRangeNoWrap = [](float t, float start, float end)
			{
				return t >= start && t <= end;
			};

			for (const auto& evt : anim.events)
			{
				if (!evt.enabled || evt.name.empty())
					continue;
				float t = std::clamp(evt.timeSeconds, 0.0f, durationSeconds);
				bool triggered = false;
				if (currentTime >= previousTime)
				{
					triggered = inRangeNoWrap(t, previousTime, currentTime);
				}
				else
				{
					triggered = inRangeNoWrap(t, previousTime, durationSeconds) || inRangeNoWrap(t, 0.0f, currentTime);
				}
				if (triggered)
					anim.triggeredEventsThisFrame.push_back(evt.name);
			}
		};

		for (const auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;

			if (!entity->HasComponent<AnimationComponent>() || !entity->HasComponent<SkeletonComponent>())
				continue;

			auto& anim = entity->GetComponent<AnimationComponent>();
			auto& skel = entity->GetComponent<SkeletonComponent>();
			AnimationStateMachineComponent* stateMachineComponent = entity->HasComponent<AnimationStateMachineComponent>()
				? &entity->GetComponent<AnimationStateMachineComponent>()
				: nullptr;

			if (stateMachineComponent)
			{
				if (!stateMachineComponent->suppressStateMachineEvaluation)
					UpdateAnimationStateMachine(anim, *stateMachineComponent);
				stateMachineComponent->currentStateTime = anim.time;
			}

			if (!skel.skeleton || !anim.clips || anim.clips->empty())
				continue;

			if (anim.activeClipIndex < 0 || anim.activeClipIndex >= static_cast<int>(anim.clips->size()))
				continue;

			const AnimationClip& clip = (*anim.clips)[anim.activeClipIndex];
			const float previousAnimationTime = anim.time;

			if (anim.playing)
				anim.time += deltaTime * anim.playbackSpeed;

			int rootMotionBoneIndex = -1;
			if (anim.enableRootMotion)
				rootMotionBoneIndex = ResolveRootMotionBoneIndex(*skel.skeleton, anim.rootMotionBoneName);

			float durationSeconds = clip.GetDurationSeconds();
			float trimStartSeconds = 0.0f;
			float trimEndSeconds = durationSeconds;
			if (stateMachineComponent && stateMachineComponent->stateMachine &&
				stateMachineComponent->stateMachine->IsValidStateIndex(stateMachineComponent->currentStateIndex))
			{
				const auto& activeState = stateMachineComponent->stateMachine->states[stateMachineComponent->currentStateIndex];
				ComputeTrimmedTimeRangeSeconds(activeState, &clip, trimStartSeconds, trimEndSeconds);
			}
			if (durationSeconds > 0.0001f)
			{
				const float trimmedDurationSeconds = std::max(0.0001f, trimEndSeconds - trimStartSeconds);
				if (anim.looping)
				{
					// Important: when a state has a non-zero trim start, times before the
					// trim window must snap to the trimmed start (not wrap to the tail),
					// otherwise edited start frames look like replay/glitch compensation.
					float localTime = anim.time - trimStartSeconds;
					if (localTime < 0.0f)
						localTime = 0.0f;
					localTime = std::fmod(localTime, trimmedDurationSeconds);
					anim.time = trimStartSeconds + localTime;
				}
				else
				{
					anim.time = std::clamp(anim.time, trimStartSeconds, trimEndSeconds);
				}
			}
			else
			{
				anim.time = 0.0f;
			}

			collectAnimationEvents(anim, previousAnimationTime, anim.time, durationSeconds);
			if (anim.enableRootMotion && rootMotionBoneIndex >= 0 && entity->HasComponent<TransformComponent>())
			{
				bool applyRootMotion = !(stateMachineComponent && stateMachineComponent->suppressStateMachineEvaluation);
				if (applyRootMotion && entity->HasComponent<CharacterControllerComponent>())
				{
					const auto& controller = entity->GetComponent<CharacterControllerComponent>();
					if (!controller.isGrounded)
						applyRootMotion = false;
				}

				const bool reachedEndThisFrame =
					!anim.looping &&
					durationSeconds > 0.0001f &&
					previousAnimationTime < trimEndSeconds &&
					anim.time >= trimEndSeconds;

				if (applyRootMotion && reachedEndThisFrame)
				{
					glm::vec3 localDelta = ComputeRootMotionDelta(*skel.skeleton, rootMotionBoneIndex, clip, trimStartSeconds, trimEndSeconds, false);
					localDelta.y = 0.0f;
					if (glm::dot(localDelta, localDelta) > 0.0f)
					{
						auto& transform = entity->GetComponent<TransformComponent>();
						glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
						glm::vec3 worldDelta = glm::vec3(yawRotation * glm::vec4(localDelta, 0.0f));
						transform.position += worldDelta;
					}
				}
			}
			for (const auto& eventName : anim.triggeredEventsThisFrame)
			{
				AnimationEventMessage message;
				message.entityID = entity->GetID();
				message.entityName = entity->GetName();
				message.eventName = eventName;
				message.eventTimeSeconds = anim.time;
				AnimationEventBus::Publish(message);

				for (const auto& evt : anim.events)
				{
					if (!evt.enabled || evt.name != eventName)
						continue;
					AnimationEventActionMessage action;
					action.entityID = entity->GetID();
					action.eventName = evt.name;
					action.triggerAudio = evt.triggerAudio;
					action.audioClipPath = evt.audioClipPath;
					action.audioVolume = evt.audioVolume;
					action.audioPitch = evt.audioPitch;
					action.triggerParticleBurst = evt.triggerParticleBurst;
					action.particleBurstCount = evt.particleBurstCount;
					action.triggerScriptCallback = evt.triggerScriptCallback;
					action.scriptCallbackName = evt.scriptCallbackName;
					AnimationEventBus::QueueAction(action);
				}
			}
			float timeTicks = anim.time * clip.ticksPerSecond;

			if (anim.blending)
			{
				// Advance the previous clip's time too so it keeps playing
				// (rather than freezing) while the blend is in progress.
				if (anim.playing)
					anim.previousTime += deltaTime * anim.playbackSpeed;

				const AnimationClip* prevClip = nullptr;
				if (anim.previousClipIndex >= 0 && anim.previousClipIndex < static_cast<int>(anim.clips->size()))
					prevClip = &(*anim.clips)[anim.previousClipIndex];

				float prevDuration = prevClip ? prevClip->GetDurationSeconds() : 0.0f;
				if (prevDuration > 0.0001f)
				{
					// During cross-fade, wrapping previousTime back to 0 replays early frames
					// and causes visible stutter when state trim/speed has been edited.
					// Clamp instead so the source pose progresses to its end without looping.
					anim.previousTime = std::clamp(anim.previousTime, 0.0f, prevDuration);
				}

				float prevTimeTicks = prevClip ? anim.previousTime * prevClip->ticksPerSecond : 0.0f;

				anim.blendElapsed += deltaTime;
				float blendT = std::clamp(anim.blendElapsed / anim.blendDuration, 0.0f, 1.0f);

				ComputeBlendedBoneMatrices(*skel.skeleton, prevClip, prevTimeTicks, &clip, timeTicks, blendT, rootMotionBoneIndex, anim.boneMatrices);

				if (blendT >= 1.0f)
				{
					anim.blending = false;
					anim.previousClipIndex = -1;
				}
			}
			else
			{
				ComputeBoneMatrices(*skel.skeleton, &clip, timeTicks, rootMotionBoneIndex, anim.boneMatrices);
			}

			if (static_cast<int>(anim.boneMatrices.size()) > MAX_ANIMATION_BONES)
			{
				static bool warned = false;
				if (!warned)
				{
					std::cerr << "[AnimationSystem] Skeleton has " << anim.boneMatrices.size()
						<< " bones, exceeding MAX_ANIMATION_BONES (" << MAX_ANIMATION_BONES
						<< "); extra bones will be ignored by the shader." << std::endl;
					warned = true;
				}
			}
		}
	}
}
