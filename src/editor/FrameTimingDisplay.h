#pragma once

#include "core/GameLoop.h"
#include <string>

namespace MyEngine
{
	// Simple frame timing and performance metrics display
	// Shows FPS, frame time, and interpolation factor in-game
	class FrameTimingDisplay
	{
	public:
		FrameTimingDisplay() = default;
		~FrameTimingDisplay() = default;

		// Update display with latest metrics
		void Update(const GameLoopContext& context);

		// Draw the overlay (call after rendering, before ImGui::Render)
		void Draw();

		// Configuration
		void SetPosition(float x, float y) { m_PositionX = x; m_PositionY = y; }
		void SetVisible(bool visible) { m_Visible = visible; }
		bool IsVisible() const { return m_Visible; }
		void ToggleVisible() { m_Visible = !m_Visible; }

	private:
		// Format a time value in milliseconds to a readable string
		std::string FormatTime(float milliseconds) const;

		// Current metrics
		float m_CurrentFPS = 0.0f;
		float m_CurrentFrameTimeMs = 0.0f;
		float m_TargetFrameTimeMs = 16.67f;
		float m_InterpolationAlpha = 0.0f;
		uint32_t m_FixedFrameNumber = 0;
		uint32_t m_RenderFrameNumber = 0;

		// Rolling average for smoother FPS display
		static constexpr int FRAME_HISTORY_SIZE = 30;
		float m_FrameTimeHistory[FRAME_HISTORY_SIZE] = {};
		int m_HistoryIndex = 0;

		// Display state
		bool m_Visible = true;
		float m_PositionX = 10.0f;
		float m_PositionY = 10.0f;
	};

} // namespace MyEngine
