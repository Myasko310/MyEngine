#include "core/GameLoop.h"
#include <iostream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace MyEngine
{
	GameLoop::GameLoop()
		: m_FrameStartTime(std::chrono::high_resolution_clock::now())
		, m_PreviousFrameTime(std::chrono::high_resolution_clock::now())
	{
	}

	void GameLoop::Initialize(
		float targetFPS,
		FramePacingMode pacingMode,
		float fixedLogicTimestep)
	{
		m_PacingMode = pacingMode;
		float maxFps = (targetFPS > 10.0f) ? targetFPS : 10.0f;
		m_Context.targetFrameTimeMs = 1000.0f / maxFps;
		m_Context.fixedDeltaTime = fixedLogicTimestep;

		m_FixedTimestep.Reset();
		m_FixedTimestep.SetMaxSubsteps(5);  // Prevent spiral-of-death

		std::cout << "[GameLoop] Initialized: Target FPS=" << targetFPS 
				  << ", Mode=" << static_cast<int>(pacingMode)
				  << ", FixedDeltaTime=" << fixedLogicTimestep << "s" << std::endl;
	}

	void GameLoop::UpdateTiming()
	{
		// Calculate delta time
		auto currentTime = std::chrono::high_resolution_clock::now();
		m_Context.renderDeltaTime = std::chrono::duration<float>(
			currentTime - m_PreviousFrameTime
		).count();
		m_PreviousFrameTime = currentTime;

		// Clamp delta time to prevent spiral-of-death
		m_Context.renderDeltaTime = (m_Context.renderDeltaTime < 0.1f) ? m_Context.renderDeltaTime : 0.1f;

		// Update fixed timestep accumulator
		m_FixedTimestep.Update(m_Context.renderDeltaTime);

		// Get interpolation factor for rendering between fixed updates
		m_Context.interpolationAlpha = m_FixedTimestep.GetInterpolationFactor();

		m_Context.renderFrameNumber++;
	}

	void GameLoop::ApplyFramePacing()
	{
		if (m_PacingMode == FramePacingMode::Uncapped)
		{
			// No frame pacing - run as fast as possible
			m_FrameStartTime = std::chrono::high_resolution_clock::now();
			return;
		}

		auto currentTime = std::chrono::high_resolution_clock::now();
		float elapsedMs = std::chrono::duration<float, std::milli>(
			currentTime - m_FrameStartTime
		).count();

		m_Context.frameTimeMs = elapsedMs;

		if (m_PacingMode == FramePacingMode::FixedRefreshRate)
		{
			// Sleep to hit target frame time
			float targetMs = m_Context.targetFrameTimeMs;
			float sleepMs = targetMs - elapsedMs;

			if (sleepMs > 0.5f)  // Only sleep if we're more than 0.5ms ahead
			{
				#ifdef _WIN32
					// Use Windows high-precision Sleep
					Sleep(static_cast<DWORD>(sleepMs));
				#else
					// Standard sleep (less precise on many platforms)
					std::this_thread::sleep_for(
						std::chrono::milliseconds(static_cast<long>(sleepMs))
					);
				#endif
			}
		}
		// VSyncPresent is handled by renderPlatform->Present() in caller

		m_FrameStartTime = std::chrono::high_resolution_clock::now();
	}

	void GameLoop::Shutdown()
	{
		std::cout << "[GameLoop] Shutting down..." << std::endl;
	}

	void GameLoop::SetTargetFPS(float fps)
	{
		float maxFps = (fps > 10.0f) ? fps : 10.0f;
		m_Context.targetFrameTimeMs = 1000.0f / maxFps;
	}

} // namespace MyEngine
