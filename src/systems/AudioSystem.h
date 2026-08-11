#pragma once

class Scene;

namespace MyEngine
{
	// Synchronizes ECS audio components with the OpenAL backend each frame:
	// updates the active listener's position/orientation, and updates/plays/stops
	// entities with an AudioSourceComponent.
	class AudioSystem
	{
	public:
		AudioSystem();
		~AudioSystem();

		AudioSystem(const AudioSystem&) = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		void Update(Scene& scene, float deltaTime);

		// Releases all OpenAL sources owned by AudioSourceComponents in the scene.
		// Should be called before the scene/entities are destroyed (e.g. on New Scene).
		void ReleaseAll(Scene& scene);
	};
}
