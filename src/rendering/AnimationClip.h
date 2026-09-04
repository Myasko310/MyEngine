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

		// Normalized lookup: matches bones by canonical name (leaf part, lowercased, alphanumeric only).
		// Falls back to exact match if normalized match not found.
		const BoneAnimationTrack* FindTrackNormalized(const std::string& boneName) const
		{
			// First try exact match for performance
			const BoneAnimationTrack* exactMatch = FindTrack(boneName);
			if (exactMatch)
				return exactMatch;

			// Try normalized comparison
			std::string normalized = NormalizeNameForLookup(boneName);
			for (const auto& track : tracks)
			{
				if (NormalizeNameForLookup(track.boneName) == normalized)
					return &track;
			}
			return nullptr;
		}

	private:
		static std::string NormalizeNameForLookup(const std::string& name)
		{
			if (name.empty())
				return name;

			// Extract leaf part after last delimiter
			std::string leaf = name;
			size_t delimiterPos = leaf.find_last_of("|:/\\");
			if (delimiterPos != std::string::npos && delimiterPos + 1 < leaf.size())
				leaf = leaf.substr(delimiterPos + 1);

			// Remove Blender-style suffixes (.001, .002, etc.)
			if (leaf.size() > 4)
			{
				size_t dotPos = leaf.rfind('.');
				if (dotPos != std::string::npos && dotPos + 1 < leaf.size())
				{
					std::string suffix = leaf.substr(dotPos + 1);
					// Check if suffix is all digits
					bool allDigits = true;
					for (char c : suffix)
					{
						if (!std::isdigit(static_cast<unsigned char>(c)))
						{
							allDigits = false;
							break;
						}
					}
					if (allDigits && suffix.size() <= 3)
						leaf = leaf.substr(0, dotPos);
				}
			}

			// Convert to lowercase and keep only alphanumeric characters
			std::string result;
			result.reserve(leaf.size());
			for (char c : leaf)
			{
				unsigned char uc = static_cast<unsigned char>(c);
				if (std::isalnum(uc))
					result.push_back(static_cast<char>(std::tolower(uc)));
			}

			return result.empty() ? leaf : result;
		}
	};
}
