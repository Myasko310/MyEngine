#pragma once

#include "rendering/AnimationClip.h"
#include "rendering/Skeleton.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <cctype>
#include <unordered_map>

namespace MyEngine
{
	inline constexpr const char* RelaxedFallClipName = "Relaxed Free Fall";

	// Generate in the supplied rig's bind space, not another character's joint axes.
	inline bool CreateRelaxedFallAnimation(const Skeleton& skeleton, AnimationClip& outClip)
	{
		const auto& bones = skeleton.GetBones();
		std::unordered_map<std::string, int> indices;
		std::vector<std::string> names;
		std::vector<glm::mat4> bindWorld(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			std::string name = bones[i].name;
			const auto separator = name.find_last_of(":|");
			if (separator != std::string::npos) name = name.substr(separator + 1);
			std::transform(name.begin(), name.end(), name.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			names.push_back(name);
			indices[name] = static_cast<int>(i);
			const int parent = bones[i].parentIndex;
			if (parent < -1 || parent >= static_cast<int>(i)) return false;
			bindWorld[i] = (parent >= 0 ? bindWorld[parent] : glm::mat4(1.0f)) * bones[i].localBindTransform;
		}
		for (const char* required : { "hips", "head", "leftarm", "leftforearm", "lefthand",
			"rightarm", "rightforearm", "righthand", "leftupleg", "leftleg", "leftfoot",
			"rightupleg", "rightleg", "rightfoot" })
			if (indices.find(required) == indices.end()) return false;

		auto position = [&](const char* name) { return glm::vec3(bindWorld[indices.at(name)][3]); };
		glm::vec3 up = position("head") - position("hips");
		glm::vec3 right = position("rightarm") - position("leftarm");
		if (glm::length(up) < 0.0001f) return false;
		up = glm::normalize(up);
		right -= up * glm::dot(right, up);
		if (glm::length(right) < 0.0001f) return false;
		right = glm::normalize(right);
		glm::vec3 forward = glm::normalize(glm::cross(right, up));
		if (indices.count("lefttoebase") && indices.count("righttoebase"))
		{
			glm::vec3 toeDirection = position("lefttoebase") - position("leftfoot") +
				position("righttoebase") - position("rightfoot");
			toeDirection -= up * glm::dot(toeDirection, up) + right * glm::dot(toeDirection, right);
			if (glm::length(toeDirection) > 0.0001f) forward = glm::normalize(toeDirection);
		}

		std::vector<glm::vec3> translations(bones.size()), scales(bones.size());
		std::vector<glm::quat> rotations(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			const auto& bone = bones[i];
			if (std::abs(glm::determinant(bone.skippedNodeLocalPrefix)) < 0.000001f) return false;
			const glm::mat4 local = glm::inverse(bone.skippedNodeLocalPrefix) * bone.localBindTransform;
			glm::vec3 skew;
			glm::vec4 perspective;
			if (!glm::decompose(local, scales[i], rotations[i], translations[i], skew, perspective)) return false;
			rotations[i] = glm::normalize(rotations[i]);
		}

		AnimationClip clip;
		clip.name = RelaxedFallClipName;
		clip.ticksPerSecond = 30.0f;
		clip.durationTicks = 60.0f;
		clip.tracks.resize(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			clip.tracks[i].boneName = bones[i].name;
			clip.tracks[i].positionKeys.push_back({ 0.0f, translations[i] });
			clip.tracks[i].scaleKeys.push_back({ 0.0f, scales[i] });
		}
		std::vector<glm::mat4> posedWorld(bones.size());
		for (int frame = 0; frame <= 60; ++frame)
		{
			const float phase = 6.28318530718f * static_cast<float>(frame % 60) / 60.0f;
			for (size_t i = 0; i < bones.size(); ++i)
			{
				const auto& bone = bones[i];
				const auto& name = names[i];
				const glm::mat4 parent = bone.parentIndex >= 0 ? posedWorld[bone.parentIndex] : glm::mat4(1.0f);
				const glm::mat4 prefix = parent * bone.skippedNodeLocalPrefix;
				glm::quat rotation = rotations[i];
				std::string child;
				glm::vec3 direction(0.0f);
				const bool left = name.rfind("left", 0) == 0;
				const std::string side = left ? "left" : "right";
				const float sign = left ? -1.0f : 1.0f;
				const float sidePhase = phase + (left ? 0.0f : 3.14159265359f);
				const float drift = std::sin(sidePhase);
				const float followThrough = std::sin(sidePhase - 0.65f);
				if (name == side + "arm")
				{
					child = side + "forearm";
					direction = right * sign * (0.65f + 0.18f * drift) - up * (0.72f - 0.12f * drift) +
						forward * (0.12f + 0.16f * followThrough);
				}
				else if (name == side + "forearm")
				{
					child = side + "hand";
					direction = right * sign * (0.25f + 0.10f * followThrough) - up * (0.70f - 0.10f * followThrough) +
						forward * (0.40f + 0.20f * followThrough);
				}
				else if (name == side + "upleg")
				{
					child = side + "leg";
					direction = right * sign * (0.14f + 0.06f * followThrough) - up * 0.96f +
						forward * (0.18f - 0.16f * drift);
				}
				else if (name == side + "leg")
				{
					child = side + "foot";
					direction = right * sign * 0.12f - up * (0.85f + 0.08f * followThrough) -
						forward * (0.45f - 0.20f * followThrough);
				}
				else if (name == "spine" || name == "spine1" || name == "spine2" || name == "neck")
				{
					child = name == "spine" ? "spine1" : name == "spine1" ? "spine2" : name == "spine2" ? "neck" : "head";
					const float torsoPhase = phase - (name == "spine" ? 0.0f : name == "spine1" ? 0.2f : name == "spine2" ? 0.4f : 0.6f);
					direction = up + forward * (0.05f + 0.06f * std::sin(torsoPhase + 0.8f)) +
						right * (0.09f * std::sin(torsoPhase));
				}
				const auto childIt = indices.find(child);
				if (childIt != indices.end())
				{
					// Convert the desired model-space direction into this joint's parent
					// space, including any FBX pre-rotation/skipped nodes.
					const glm::vec3 childLocal = glm::vec3(glm::inverse(bindWorld[i]) * bindWorld[childIt->second][3]);
					const glm::vec3 from = rotation * (scales[i] * childLocal);
					const glm::vec3 to = glm::mat3(glm::inverse(prefix)) * direction;
					if (glm::length(from) < 0.0001f || glm::length(to) < 0.0001f) return false;
					rotation = glm::normalize(glm::rotation(glm::normalize(from), glm::normalize(to)) * rotation);
				}
				clip.tracks[i].rotationKeys.push_back({ static_cast<float>(frame), rotation });
				posedWorld[i] = prefix * glm::translate(glm::mat4(1.0f), translations[i]) *
					glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scales[i]);
			}
		}
		outClip = std::move(clip);
		return true;
	}
}
