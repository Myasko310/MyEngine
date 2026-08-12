#include "systems/AnimationSystem.h"

#include "components/AnimationComponent.h"
#include "components/SkeletonComponent.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace MyEngine
{
	// Samples a single bone's local transform (translate * rotate * scale)
	// from a clip's track at the given time, falling back to the bind-pose
	// local transform if the clip doesn't animate this bone.
	static glm::mat4 SampleLocalTransform(const Bone& bone, const AnimationClip* clip, float timeTicks)
	{
		if (!clip)
			return bone.localBindTransform;

		const BoneAnimationTrack* track = clip->FindTrack(bone.name);
		if (!track)
			return bone.localBindTransform;

		glm::vec3 pos = track->SamplePosition(timeTicks);
		glm::quat rot = track->SampleRotation(timeTicks);
		glm::vec3 scale = track->SampleScale(timeTicks);

		return glm::translate(glm::mat4(1.0f), pos) *
			glm::mat4_cast(rot) *
			glm::scale(glm::mat4(1.0f), scale);
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
		std::vector<glm::mat4>& outMatrices)
	{
		const auto& bones = skeleton.GetBones();
		std::vector<glm::mat4> accumulated(bones.size(), glm::mat4(1.0f));

		outMatrices.assign(bones.size(), glm::mat4(1.0f));

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const Bone& bone = bones[i];

			glm::mat4 localTransform = SampleLocalTransform(bone, clip, timeTicks);

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
		std::vector<glm::mat4>& outMatrices)
	{
		const auto& bones = skeleton.GetBones();
		std::vector<glm::mat4> accumulated(bones.size(), glm::mat4(1.0f));

		outMatrices.assign(bones.size(), glm::mat4(1.0f));

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const Bone& bone = bones[i];

			glm::mat4 fromLocal = SampleLocalTransform(bone, fromClip, fromTimeTicks);
			glm::mat4 toLocal = SampleLocalTransform(bone, toClip, toTimeTicks);
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
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;

			if (!entity->HasComponent<AnimationComponent>() || !entity->HasComponent<SkeletonComponent>())
				continue;

			auto& anim = entity->GetComponent<AnimationComponent>();
			auto& skel = entity->GetComponent<SkeletonComponent>();

			if (!skel.skeleton || !anim.clips || anim.clips->empty())
				continue;

			if (anim.activeClipIndex < 0 || anim.activeClipIndex >= static_cast<int>(anim.clips->size()))
				continue;

			const AnimationClip& clip = (*anim.clips)[anim.activeClipIndex];

			if (anim.playing)
				anim.time += deltaTime * anim.playbackSpeed;

			float durationSeconds = clip.GetDurationSeconds();
			if (durationSeconds > 0.0001f)
			{
				if (anim.looping)
				{
					anim.time = std::fmod(anim.time, durationSeconds);
					if (anim.time < 0.0f)
						anim.time += durationSeconds;
				}
				else
				{
					anim.time = std::clamp(anim.time, 0.0f, durationSeconds);
				}
			}
			else
			{
				anim.time = 0.0f;
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
					anim.previousTime = std::fmod(anim.previousTime, prevDuration);
					if (anim.previousTime < 0.0f)
						anim.previousTime += prevDuration;
				}

				float prevTimeTicks = prevClip ? anim.previousTime * prevClip->ticksPerSecond : 0.0f;

				anim.blendElapsed += deltaTime;
				float blendT = std::clamp(anim.blendElapsed / anim.blendDuration, 0.0f, 1.0f);

				ComputeBlendedBoneMatrices(*skel.skeleton, prevClip, prevTimeTicks, &clip, timeTicks, blendT, anim.boneMatrices);

				if (blendT >= 1.0f)
				{
					anim.blending = false;
					anim.previousClipIndex = -1;
				}
			}
			else
			{
				ComputeBoneMatrices(*skel.skeleton, &clip, timeTicks, anim.boneMatrices);
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
