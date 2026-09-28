#pragma once

#ifdef USE_IMGUI
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace MyEngine::Editor
{
	// Panel positions that snap windows to docking-like positions
	enum class PanelPosition
	{
		// Predefined dock zones
		LeftSidebar,      // Left side (fixed width, full height)
		RightSidebar,     // Right side (fixed width, full height)
		BottomPanel,      // Bottom area (full width, fixed height)
		CenterViewport,   // Main viewport area
		Floating,         // Free floating position
	};

	struct PanelLayout
	{
		PanelPosition position = PanelPosition::Floating;
		ImVec2 size = ImVec2(400, 600);
		ImVec2 customOffset = ImVec2(0, 0); // Only used for Floating
	};

	class PanelDockManager
	{
	public:
		static PanelDockManager& Instance();

		// Register a panel with a position hint
		void RegisterPanel(const char* panelName, PanelPosition position);

		// Get the position and size for a panel
		ImVec2 GetPanelPos(const char* panelName, float viewportWidth, float viewportHeight);
		ImVec2 GetPanelSize(const char* panelName, float viewportWidth, float viewportHeight);

		// Set viewport dimensions (call whenever window is resized)
		void SetViewportSize(float width, float height);

		// Snap a panel to dock position
		void SnapPanel(const char* panelName) const;

		// Reset all panels to default positions
		void ResetLayout();

	private:
		PanelDockManager();

		struct PanelState
		{
			PanelPosition position;
			ImVec2 size;
			ImVec2 customOffset = ImVec2(0, 0); // For floating windows
		};

		std::unordered_map<std::string, PanelState> panels;
		float viewportW = 1280.0f;
		float viewportH = 720.0f;

		// Calculate zone positions
		ImVec2 GetLeftSidebarPos() const { return ImVec2(5, 20); }
		ImVec2 GetLeftSidebarSize() const { return ImVec2(300, viewportH - 100); }

		ImVec2 GetRightSidebarPos() const { return ImVec2(viewportW - 315, 20); }
		ImVec2 GetRightSidebarSize() const { return ImVec2(300, viewportH - 100); }

		ImVec2 GetBottomPanelPos() const { return ImVec2(310, viewportH - 100); }
		ImVec2 GetBottomPanelSize() const { return ImVec2(viewportW - 625, 90); }
	};
}

#endif
