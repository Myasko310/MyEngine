#pragma once

#include "character/SparringRig.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "ecs/TransformHierarchy.h"

namespace MyEngine
{
	struct BurstPress
	{
		uint32_t id, target;
		bool damageApplied = false;
	};

	struct PlayerStrikeBones
	{
		uint32_t pressCounter = 0;
		float incomingBurstDamage = 0, incomingBurstPosture = 0;
		uint32_t incomingBurstOwner = 0;
		std::shared_ptr<BurstPress> queuedPress, meleePress;
		std::array<int,4> indices{{-1,-1,-1,-1}};
		void Bind(const Entity& entity)
		{
			SparringRig rig;
			rig.Bind(entity);
			indices = {rig.bone[rig.RHand],rig.bone[rig.LHand],rig.bone[rig.RFoot],rig.bone[rig.LFoot]};
		}
	};

	namespace CombatShapes
	{
		struct Sphere { glm::vec3 center; float radius; };
		inline Sphere WorldSphere(const glm::mat4& world, glm::vec3 local, float radius)
		{
			const float scale = std::max({glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), glm::length(glm::vec3(world[2]))});
			return {glm::vec3(world * glm::vec4(local,1)), radius * scale};
		}
		struct Strikes { std::array<Sphere,2> spheres{}; int count = 0; };
		inline Strikes SampleStrikes(const Scene& scene, const Entity& entity, int slot, glm::vec3 center, float radius)
		{
			Strikes result;
			const auto world = TransformHierarchy::GetWorldMatrix(scene,entity);
			if (slot >= 0 && entity.HasComponent<SparringRig>()) center = entity.GetComponent<SparringRig>().Strike(entity,slot,center);
			else if (slot >= 0 && entity.HasComponent<PlayerStrikeBones>() && entity.HasComponent<SkeletonComponent>() && entity.HasComponent<AnimationComponent>())
			{
				const auto& sk = entity.GetComponent<SkeletonComponent>().skeleton;
				const auto& anim = entity.GetComponent<AnimationComponent>();
				bool kick = slot == 1;
				if (anim.clips && anim.activeClipIndex >= 0 && anim.activeClipIndex < static_cast<int>(anim.clips->size()))
				{
					auto name = (*anim.clips)[anim.activeClipIndex].name;
					std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
					if (name.find("kick") != std::string::npos) kick = true;
					else if (name.find("jab") != std::string::npos || name.find("punch") != std::string::npos || name.find("uppercut") != std::string::npos) kick = false;
				}
				for (int side = 0; side < 2; ++side)
				{
					const int i = entity.GetComponent<PlayerStrikeBones>().indices[(kick ? 2 : 0) + side];
					if (!sk || i < 0 || i >= static_cast<int>(sk->GetBoneCount()) || i >= static_cast<int>(anim.boneMatrices.size())) continue;
					const glm::vec3 point = (anim.boneMatrices[i] * glm::inverse(sk->GetBones()[i].offsetMatrix))[3];
					result.spheres[result.count++] = WorldSphere(world,point,radius);
				}
			}
			if (result.count == 0) result.spheres[result.count++] = WorldSphere(world,center,radius);
			return result;
		}
		template<class F> void ForEachHurtSphere(const Scene& scene, const Entity& target, const CombatHurtboxComponent& hurt, F visit)
		{
			const auto world = TransformHierarchy::GetWorldMatrix(scene,target);
			if (target.HasComponent<SparringRig>())
			{
				const auto pose = target.GetComponent<SparringRig>().Pose(target);
				for (int j = 0; j < SparringRig::Count; ++j) visit(j,WorldSphere(world,pose[j],SparringRig::radius[j]));
			}
			else visit(-1,WorldSphere(world,hurt.center,hurt.radius));
		}
	}
}
