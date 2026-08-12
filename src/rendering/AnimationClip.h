#pragma once

#include <string>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MyEngine
{
	struct PositionKey
	{
		float time = 0.0f;
		glm::vec3 value{ 0.0f };
	};

	struct RotationKey
	{
		float time = 0.0f;
		glm::quat value{ 1.0f, 0.0f, 0.0f, 0.0f };
	};

	struct ScaleKey
	{
		float time = 0.0f;
		glm::vec3 value{ 1.0f };
	};

	// Per-bone keyframe tracks, matching assimp's aiNodeAnim layout (separate
	// position/rotation/scale tracks with independent key counts/timing).
	struct BoneAnimationTrack
	{
		std::string boneName;
		std::vector<PositionKey> positionKeys;
		std::vector<RotationKey> rotationKeys;
		std::vector<ScaleKey> scaleKeys;

		glm::vec3 SamplePosition(float timeTicks) const
		{
			if (positionKeys.empty())
				return glm::vec3(0.0f);
			if (positionKeys.size() == 1)
				return positionKeys[0].value;

			size_t next = FindNextKey(positionKeys, timeTicks);
			size_t prev = next == 0 ? 0 : next - 1;
			if (next == 0)
				return positionKeys[0].value;

			float t = SegmentFactor(positionKeys[prev].time, positionKeys[next].time, timeTicks);
			return glm::mix(positionKeys[prev].value, positionKeys[next].value, t);
		}

		glm::quat SampleRotation(float timeTicks) const
		{
			if (rotationKeys.empty())
				return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			if (rotationKeys.size() == 1)
				return rotationKeys[0].value;

			size_t next = FindNextKey(rotationKeys, timeTicks);
			size_t prev = next == 0 ? 0 : next - 1;
			if (next == 0)
				return rotationKeys[0].value;

			float t = SegmentFactor(rotationKeys[prev].time, rotationKeys[next].time, timeTicks);
			return glm::normalize(glm::slerp(rotationKeys[prev].value, rotationKeys[next].value, t));
		}

		glm::vec3 SampleScale(float timeTicks) const
		{
			if (scaleKeys.empty())
				return glm::vec3(1.0f);
			if (scaleKeys.size() == 1)
				return scaleKeys[0].value;

			size_t next = FindNextKey(scaleKeys, timeTicks);
			size_t prev = next == 0 ? 0 : next - 1;
			if (next == 0)
				return scaleKeys[0].value;

			float t = SegmentFactor(scaleKeys[prev].time, scaleKeys[next].time, timeTicks);
			return glm::mix(scaleKeys[prev].value, scaleKeys[next].value, t);
		}

	private:
		template <typename KeyT>
		static size_t FindNextKey(const std::vector<KeyT>& keys, float timeTicks)
		{
			for (size_t i = 0; i < keys.size(); ++i)
			{
				if (keys[i].time > timeTicks)
					return i;
			}
			// Time is past the last key; clamp to the final segment.
			return keys.size() - 1;
		}

		static float SegmentFactor(float startTime, float endTime, float timeTicks)
		{
			float span = endTime - startTime;
			if (span <= 0.0001f)
				return 0.0f;
			return std::clamp((timeTicks - startTime) / span, 0.0f, 1.0f);
		}
	};

	// A full animation clip imported from a single aiAnimation: a duration
	// (in ticks), a playback rate (ticks per second), and one track per
	// animated bone/node.
	class AnimationClip
	{
	public:
		std::string name;
		float durationTicks = 0.0f;
		float ticksPerSecond = 25.0f;
		std::vector<BoneAnimationTrack> tracks;

		float GetDurationSeconds() const
		{
			return ticksPerSecond > 0.0001f ? durationTicks / ticksPerSecond : 0.0f;
		}

		const BoneAnimationTrack* FindTrack(const std::string& boneName) const
		{
			for (const auto& track : tracks)
			{
				if (track.boneName == boneName)
					return &track;
			}
			return nullptr;
		}
	};
}
