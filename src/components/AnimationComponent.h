#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "rendering/AnimationClip.h"

// Maximum bones per palette uploaded to the skinning shader. Must match
// MAX_BONES in shaders/lit_skinned.vert. Skeletons with more bones than this
// are clamped (extra bones have no effect) with a one-time console warning.
constexpr int MAX_ANIMATION_BONES = 100;

// Drives per-frame playback of a skinned entity's animation and stores the
// resulting bone matrix palette for MeshRendererSystem to upload to the
// skinning shader. Requires a sibling SkeletonComponent on the same entity.
struct AnimationComponent
{
	// Clips available to this entity; typically populated once from
	// Model::GetAnimationClips() when the skinned model is attached.
	std::shared_ptr<std::vector<MyEngine::AnimationClip>> clips = nullptr;

	int activeClipIndex = 0;
	float time = 0.0f; // seconds
	float playbackSpeed = 1.0f;
	bool playing = true;
	bool looping = true;

	// --- Cross-fade transition state ---
	// When true, AnimationSystem blends the previous clip's pose (at
	// previousTime) with the newly active clip's pose (at `time`), weighted
	// by blendElapsed/blendDuration, and clears `blending` once the fade
	// completes. Set up via TransitionTo(); not intended to be written
	// directly outside of that helper.
	bool blending = false;
	int previousClipIndex = -1;
	float previousTime = 0.0f;
	float blendElapsed = 0.0f;
	float blendDuration = 0.0f;

	// Final bone matrices (bone-space -> mesh-space), computed each frame by
	// AnimationSystem and consumed by MeshRendererSystem when drawing.
	std::vector<glm::mat4> boneMatrices;

	// Begins a cross-fade from the currently active clip to `clipIndex` over
	// `duration` seconds. If already blending, the in-progress blended pose
	// becomes the new "previous" pose so transitions can be chained smoothly
	// without popping. No-op if `clipIndex` is already the active clip and
	// no blend is in progress.
	void TransitionTo(int clipIndex, float duration)
	{
		if (clipIndex == activeClipIndex && !blending)
			return;

		previousClipIndex = activeClipIndex;
		previousTime = time;
		activeClipIndex = clipIndex;
		time = 0.0f;
		blendElapsed = 0.0f;
		blendDuration = duration > 0.0001f ? duration : 0.0001f;
		blending = true;
	}
};
