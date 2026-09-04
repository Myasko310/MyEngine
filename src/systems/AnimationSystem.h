#pragma once

class Scene;

namespace MyEngine
{
	// Advances playback time for every entity with an AnimationComponent
	// (+ sibling SkeletonComponent), samples the active AnimationClip per
	// bone, and computes the final bone matrix palette used for skinning.
	class AnimationSystem
	{
	public:
		void Update(Scene& scene, float deltaTime);
	};
}
