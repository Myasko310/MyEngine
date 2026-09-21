#pragma once

#include "core/FixedTimestep.h"
#include <chrono>
#include <memory>

class Scene;
struct GLFWwindow;

namespace MyEngine
{
	class PerformanceMetrics;

	// Frame rate capping / V-Sync modes
	enum class FramePacingMode
	{
		Uncapped,           // No frame rate limit (as fast as possible)
		FixedRefreshRate,   // Target a specific refresh rate (60, 120, 144 Hz, etc.)
		VSyncPresent        // Wait for V-Sync (monitor refresh rate)
	};

	// Context passed to systems during update phase
	struct GameLoopContext
	{
		// Timing information
		float fixedDeltaTime = 1.0f / 60.0f;    // Fixed timestep (e.g., 1/60 for 60 Hz logic)
		float renderDeltaTime = 0.0f;            // Variable frame delta (real time since last render)
		float interpolationAlpha = 0.0f;         // [0, 1) alpha for rendering interpolation
		uint32_t fixedFrameNumber = 0;           // Count of fixed-step updates
		uint32_t renderFrameNumber = 0;          // Count of render frames

		// Frame statistics
		float frameTimeMs = 0.0f;                // Total frame time in milliseconds
		float targetFrameTimeMs = 16.67f;        // Target frame time (inverse of target FPS)
		float gpuFrameTimeMs = 0.0f;             // GPU frame time (if available)
	};

	// Central game loop timing orchestrator
	// Manages fixed-timestep updates, frame pacing, and interpolation
	// NOTE: This class handles TIMING ONLY - actual system updates remain in main.cpp
	class GameLoop
	{
	public:
		GameLoop();
		~GameLoop() = default;

		// Initialize the game loop with target settings
		void Initialize(
			float targetFPS = 120.0f,
			FramePacingMode pacingMode = FramePacingMode::FixedRefreshRate,
			float fixedLogicTimestep = 1.0f / 60.0f
		);

		// Main frame timing update - call once per render frame BEFORE systems update
		// Updates delta times and fixed timestep accumulator
		void UpdateTiming();

		// Apply frame pacing (sleep if needed) - call AFTER all rendering
		void ApplyFramePacing();

		// Shutdown and cleanup
		void Shutdown();

		// Frame timing accessors
		const GameLoopContext& GetContext() const { return m_Context; }
		float GetDeltaTime() const { return m_Context.renderDeltaTime; }
		float GetInterpolationAlpha() const { return m_Context.interpolationAlpha; }
		uint32_t GetFixedFrameNumber() const { return m_Context.fixedFrameNumber; }
		uint32_t GetRenderFrameNumber() const { return m_Context.renderFrameNumber; }
		float GetTargetFrameTimeMs() const { return m_Context.targetFrameTimeMs; }
		float GetFrameTimeMs() const { return m_Context.frameTimeMs; }

		// Fixed timestep query
		bool ShouldFixedUpdate() { return m_FixedTimestep.ShouldFixedUpdate(); }
		void AdvanceFixedFrame() { m_Context.fixedFrameNumber++; }

		// Frame rate control
		void SetTargetFPS(float fps);
		float GetTargetFPS() const { return 1000.0f / m_Context.targetFrameTimeMs; }
		void SetFramePacingMode(FramePacingMode mode) { m_PacingMode = mode; }
		FramePacingMode GetFramePacingMode() const { return m_PacingMode; }

		// Debug & diagnostics
		uint32_t GetSubstepsThisFrame() const { return m_FixedTimestep.GetSubstepsThisFrame(); }

	private:
		// Timing management
		GameLoopContext m_Context;
		FixedTimestep m_FixedTimestep;
		std::chrono::high_resolution_clock::time_point m_FrameStartTime;
		std::chrono::high_resolution_clock::time_point m_PreviousFrameTime;
		FramePacingMode m_PacingMode = FramePacingMode::FixedRefreshRate;

		// Frame rate capping
		float m_FrameTimeAccumulator = 0.0f;
	};

} // namespace MyEngine
