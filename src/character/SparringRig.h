#pragma once

#include "components/AnimationComponent.h"
#include "components/SkeletonComponent.h"
#include "ecs/Entity.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace MyEngine
{
	struct SparringRig
	{
		enum Joint { Hip, Chest, Head, LElbow, LHand, RElbow, RHand, LKnee, LFoot, RKnee, RFoot, Count };
		inline static constexpr int parent[Count] = {-1,Hip,Chest,Chest,LElbow,Chest,RElbow,Hip,LKnee,Hip,RKnee};
		inline static constexpr float radius[Count] = {.28f,.34f,.23f,.17f,.16f,.17f,.16f,.19f,.18f,.19f,.18f};
		using Points = std::array<glm::vec3,Count>;
		Points rest{{{0,.90f,0},{0,1.32f,0},{0,1.75f,0},{-.37f,1.25f,0},{-.48f,1.0f,.12f},{.37f,1.25f,0},{.48f,1.0f,.12f},{-.18f,.52f,0},{-.18f,.16f,.08f},{.18f,.52f,0},{.18f,.16f,.08f}}};
		Points points{}, previous{}, initial{};
		std::array<int,Count> bone;
		std::vector<glm::mat4> basePalette;
		bool down = false, initialized = false;
		float downTime = 0, pendingDamage = 0, pendingPosture = 0;
		uint32_t lastAttacker = 0;
		int lastRegion = -1;
		static constexpr int knockdownHits = 4;
		static constexpr float recoverySeconds = 3.f;
		int standingHits = 0;
		void CountHit() { if (!down && standingHits < knockdownHits) ++standingHits; }
		bool savedTrigger = false;
		glm::vec3 burstRecoil{0};
		void Push(glm::vec3 direction, float strength)
		{
			direction.y = 0;
			const float length = glm::length(direction);
			if (length < .001f) return;
			burstRecoil += direction * (strength / length);
			const float speed = glm::length(burstRecoil);
			if (speed > 14.f) burstRecoil *= 14.f / speed;
		}
		SparringRig() { bone.fill(-1); }

		void Bind(const Entity& entity)
		{
			if (!entity.HasComponent<SkeletonComponent>()) return;
			const auto& sk = entity.GetComponent<SkeletonComponent>().skeleton;
			if (!sk) return;
			const char* labels[Count] = {"hips","spine2","head","leftforearm","lefthand","rightforearm","righthand","leftleg","leftfoot","rightleg","rightfoot"};
			for (int i = 0; i < static_cast<int>(sk->GetBoneCount()); ++i)
			{
				std::string name;
				for (unsigned char ch : sk->GetBones()[i].name) if (std::isalnum(ch)) name += static_cast<char>(std::tolower(ch));
				for (int j = 0; j < Count; ++j)
				{
					std::string suffix = labels[j];
					if (name.size() >= suffix.size() && name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0) bone[j] = i;
				}
			}
		}

		void ApplyPalette(AnimationComponent& anim, const Skeleton& skeleton) const
		{
			if (!initialized || basePalette.size() != skeleton.GetBoneCount()) return;
			std::array<glm::mat4,Count> adjustment;
			for (int j = 0; j < Count; ++j)
			{
				const int other = parent[j] < 0 ? Chest : parent[j];
				const auto a = initial[j]-initial[other], b = points[j]-points[other];
				auto rotation = glm::mat4(1);
				if (glm::length(a) > .001f && glm::length(b) > .001f) rotation = glm::mat4_cast(glm::rotation(glm::normalize(a),glm::normalize(b)));
				adjustment[j] = glm::translate(glm::mat4(1),points[j]) * rotation * glm::translate(glm::mat4(1),-initial[j]);
			}
			anim.boneMatrices.resize(basePalette.size());
			for (int i = 0; i < static_cast<int>(basePalette.size()); ++i)
			{
				int j = Hip;
				for (int k = i; k >= 0; )
				{
					auto found = std::find(bone.begin(),bone.end(),k);
					if (found != bone.end()) { j = static_cast<int>(found-bone.begin()); break; }
					const int up = skeleton.GetBones()[k].parentIndex;
					if (up >= k) break;
					k = up;
				}
				anim.boneMatrices[i] = adjustment[j] * basePalette[i];
			}
		}
		Points Pose(const Entity& entity) const
		{
			if (down && initialized) return points;
			Points result = rest;
			if (!entity.HasComponent<SkeletonComponent>() || !entity.HasComponent<AnimationComponent>()) return result;
			const auto& sk = entity.GetComponent<SkeletonComponent>().skeleton;
			const auto& palette = entity.GetComponent<AnimationComponent>().boneMatrices;
			for (int j = 0; sk && j < Count; ++j) if (bone[j] >= 0 && bone[j] < static_cast<int>(palette.size()) && bone[j] < static_cast<int>(sk->GetBoneCount()))
				result[j] = glm::vec3((palette[bone[j]] * glm::inverse(sk->GetBones()[bone[j]].offsetMatrix))[3]);
			return result;
		}
		static Joint Striker(int slot) { return slot == 0 ? RHand : (slot == 1 ? RFoot : LHand); }
		glm::vec3 Strike(const Entity& entity, int slot, glm::vec3 authoredCenter) const
		{
			const auto j = Striker(slot);
			if (bone[j] >= 0 && entity.HasComponent<AnimationComponent>() && bone[j] < static_cast<int>(entity.GetComponent<AnimationComponent>().boneMatrices.size())) return Pose(entity)[j];
			return authoredCenter + glm::vec3(rest[j].x * .35f,0,0);
		}
		void BeginDown()
		{
			if (!down) initialized = false;
			down = true; downTime = 0;
		}
	};
}
