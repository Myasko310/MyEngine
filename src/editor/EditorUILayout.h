#pragma once

#ifdef USE_IMGUI
#include <imgui.h>
#include <string>

namespace MyEngine::Editor::Layout
{
	// Dock zones for organizing panels
	enum class DockZone
	{
		Left,      // Left sidebar (hierarchy, properties)
		Right,     // Right sidebar (inspector)
		Center,    // Main viewport area
		Bottom,    // Debug/diagnostics panels
		TopLeft,   // Top-left corner
		TopRight   // Top-right corner
	};

	// Layout configuration
	struct LayoutConfig
	{
		// Viewport dimensions
		float viewportWidth = 1280.0f;
		float viewportHeight = 720.0f;

		// Zone sizes (pixels or ratios)
		float leftZoneWidth = 280.0f;      // Left sidebar width
		float rightZoneWidth = 320.0f;     // Right inspector width
		float bottomZoneHeight = 200.0f;   // Bottom panel height

		// Margins
		float topMargin = 20.0f;           // Space for menu bar
		float panelPadding = 5.0f;         // Space between panels

		// Whether to show zones
		bool showLeftZone = true;
		bool showRightZone = true;
		bool showBottomZone = true;
	};

	// Global layout state
	struct LayoutState
	{
		LayoutConfig config;
		ImGuiID leftPanelTabId = 0;
		ImGuiID rightPanelTabId = 0;
		ImGuiID bottomPanelTabId = 0;
	};

	// Initialize layout system
	void InitializeLayout(float windowWidth, float windowHeight);

	// Update layout based on current viewport
	void UpdateLayout(float windowWidth, float windowHeight);

	// Get position and size for a dock zone
	ImVec2 GetDockZonePosition(DockZone zone);
	ImVec2 GetDockZoneSize(DockZone zone);

	// Begin a docked panel region (returns true if visible)
	bool BeginDockZoneLeft();
	bool BeginDockZoneRight();
	bool BeginDockZoneBottom();
	void EndDockZone();

	// Helpers for tabbed panels
	bool BeginPanelTab(const char* label, bool* p_open = nullptr);
	void EndPanelTab();

	// Get current layout state
	LayoutState& GetLayoutState();

	// Helper: Snap window to dock zone
	void SnapWindowToDockZone(DockZone zone);

	// Layout persistence
	void SaveLayout(const std::string& path);
	void LoadLayout(const std::string& path);
}

#endif
