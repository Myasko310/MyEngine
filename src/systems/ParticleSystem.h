#pragma once

#include <glm/glm.hpp>
#include <memory>

class Scene;

namespace MyEngine
{
	// CPU-simulated billboard particle system.
	//
	// Each entity that has a ParticleEmitterComponent (+ TransformComponent) is
	// simulated and rendered by this system.  Call Update every frame before
	// Render; call Render after the opaque MeshRendererSystem pass so that
	// additive/alpha-blended particles composite correctly.
	class ParticleSystem
	{
	public:
		ParticleSystem();
		~ParticleSystem();

		ParticleSystem(const ParticleSystem&) = delete;
		ParticleSystem& operator=(const ParticleSystem&) = delete;

		// Advance simulation: spawn new particles, apply velocity/gravity,
		// age and recycle dead particles. Should be called once per frame.
		void Update(Scene& scene, float deltaTime);

		// Render all live particles as instanced, camera-facing billboards.
		// view/projection should match the primary camera for this frame.
		void Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection);
		void SetDeterministicSeed(unsigned int seed);

	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;
	};
}
