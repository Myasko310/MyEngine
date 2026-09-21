#pragma once

#include <glm/glm.hpp>
#include <functional>

namespace MyEngine
{
	// ============================================================
	// FixedTimestep Manager
	// ============================================================
	// Decouples physics/logic updates from rendering framerate.
	// Maintains a fixed timestep accumulator, with interpolation
	// factor for smooth rendering between physics frames.
	//
	// Usage:
	//   FixedTimestep timestep(1.0f / 50.0f); // 50 Hz physics
	//   while (running) {
	//       float deltaTime = calculateDeltaTime();
	//       timestep.Update(deltaTime);
	//       while (timestep.ShouldFixedUpdate()) {
	//           physics.Update(timestep.GetFixedDeltaTime());
	//       }
	//       float alpha = timestep.GetInterpolationFactor();
	//       camera.Render(alpha);
	//   }
	// ============================================================
	class FixedTimestep
	{
	public:
		explicit FixedTimestep(float fixedDeltaTime = 0.02f);

		// Call once per frame with measured deltaTime
		void Update(float deltaTime);

		// Returns true if a fixed update should run this frame
		bool ShouldFixedUpdate();

		// Returns the fixed timestep duration
		float GetFixedDeltaTime() const { return m_FixedDeltaTime; }

		// Interpolation factor [0, 1) for smooth rendering between physics frames
		// Use this to lerp between previous and current state for smooth visuals
		float GetInterpolationFactor() const { return m_InterpolationFactor; }

		// Reset accumulator (useful when pausing/resuming)
		void Reset();

		// Configure max substeps to prevent "spiral of death" in frame drops
		void SetMaxSubsteps(int maxSubsteps) { m_MaxSubsteps = maxSubsteps; }
		int GetMaxSubsteps() const { return m_MaxSubsteps; }

		// Diagnostics
		int GetSubstepsThisFrame() const { return m_SubstepsThisFrame; }

	private:
		float m_FixedDeltaTime;         // Fixed timestep (e.g., 0.02f for 50 Hz)
		float m_Accumulator;             // Time accumulator
		float m_InterpolationFactor;     // [0, 1) for rendering interpolation
		int m_MaxSubsteps;               // Prevent spiral of death
		int m_SubstepsThisFrame;         // Diagnostics
	};
}
