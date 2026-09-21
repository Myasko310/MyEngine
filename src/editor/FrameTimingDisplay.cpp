#include "FrameTimingDisplay.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace MyEngine
{
	void FrameTimingDisplay::Update(const GameLoopContext& context)
	{
		m_CurrentFrameTimeMs = context.frameTimeMs;
		m_TargetFrameTimeMs = context.targetFrameTimeMs;
		m_InterpolationAlpha = context.interpolationAlpha;
		m_FixedFrameNumber = context.fixedFrameNumber;
		m_RenderFrameNumber = context.renderFrameNumber;

		// Add to frame history for rolling average
		m_FrameTimeHistory[m_HistoryIndex] = context.frameTimeMs;
		m_HistoryIndex = (m_HistoryIndex + 1) % FRAME_HISTORY_SIZE;

		// Calculate average and FPS
		float avgFrameTime = 0.0f;
		for (int i = 0; i < FRAME_HISTORY_SIZE; ++i)
		{
			avgFrameTime += m_FrameTimeHistory[i];
		}
		avgFrameTime /= FRAME_HISTORY_SIZE;
		m_CurrentFPS = (avgFrameTime > 0.001f) ? (1000.0f / avgFrameTime) : 0.0f;
	}

	void FrameTimingDisplay::Draw()
	{
		if (!m_Visible)
			return;

#ifdef USE_IMGUI
		// Create a simple overlay window for frame timing info
		ImGui::SetNextWindowPos(ImVec2(m_PositionX, m_PositionY), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(350.0f, 200.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.75f);

		if (ImGui::Begin("Frame Timing", &m_Visible, ImGuiWindowFlags_NoMove))
		{
			ImGui::Text("FPS: %.1f (Target: %.0f)", m_CurrentFPS, 1000.0f / m_TargetFrameTimeMs);
			ImGui::Separator();

			ImGui::Text("Frame Time:       %.2f ms", m_CurrentFrameTimeMs);
			ImGui::Text("Target Time:      %.2f ms", m_TargetFrameTimeMs);

			// Color-code frame time based on target
			float targetFPS = 1000.0f / m_TargetFrameTimeMs;
			float frameTimePercentage = m_CurrentFrameTimeMs / m_TargetFrameTimeMs;

			if (frameTimePercentage < 0.9f)
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Good");
			else if (frameTimePercentage < 1.0f)
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Caution");
			else
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Over Budget");

			ImGui::Separator();

			ImGui::Text("Interpolation Alpha: %.3f", m_InterpolationAlpha);
			ImGui::Text("Fixed Frame #:       %u", m_FixedFrameNumber);
			ImGui::Text("Render Frame #:      %u", m_RenderFrameNumber);

			ImGui::Separator();

			// Frame time graph
			ImGui::PlotHistogram("Frame Times (ms)", m_FrameTimeHistory, FRAME_HISTORY_SIZE, m_HistoryIndex, nullptr, 0.0f, m_TargetFrameTimeMs * 2.0f, ImVec2(300.0f, 50.0f));

			ImGui::End();
		}
#endif
	}

	std::string FrameTimingDisplay::FormatTime(float milliseconds) const
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << milliseconds << " ms";
		return oss.str();
	}

} // namespace MyEngine
