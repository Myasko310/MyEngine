#pragma once

#include <memory>

#include "rendering/Skeleton.h"

// Holds the shared bone hierarchy for a skinned entity. This is typically
// shared (via shared_ptr) across all entities instantiated from the same
// skinned model, since the skeleton itself doesn't change per-instance -
// only the AnimationComponent's playback state and resulting bone matrices do.
struct SkeletonComponent
{
	std::shared_ptr<MyEngine::Skeleton> skeleton = nullptr;
};
