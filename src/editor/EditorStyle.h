#pragma once

#ifdef USE_IMGUI
#include <imgui.h>

namespace MyEngine::Editor
{
	// ============================================================================
	// Color Utilities & Theme System
	// ============================================================================

	// Core color interpolation
	ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t);

	// Apply professional dark enterprise theme
	void ApplyEditorTheme();

	// Semantic color accessors (for accessing theme colors)
	ImVec4 GetThemeColor(const char* semanticName);

	// ============================================================================
	// Spacing & Layout Constants
	// ============================================================================

	namespace Spacing
	{
		constexpr float PaddingSmall = 4.0f;
		constexpr float PaddingNormal = 8.0f;
		constexpr float PaddingLarge = 12.0f;
		constexpr float PaddingXLarge = 16.0f;

		constexpr float SpacingSmall = 4.0f;
		constexpr float SpacingNormal = 8.0f;
		constexpr float SpacingLarge = 12.0f;

		constexpr float CornerRadiusSmall = 3.0f;
		constexpr float CornerRadiusNormal = 5.0f;
		constexpr float CornerRadiusLarge = 8.0f;
	}

	// ============================================================================
	// Panel & Section Styling
	// ============================================================================

	// Begin a collapsible inspector section with professional styling
	bool BeginInspectorSection(const char* title, bool defaultOpen = true);
	void EndInspectorSection();

	// Inspector group label with semantic styling
	void InspectorGroupLabel(const char* label);

	// Full-width layout helper
	void InspectorFullWidth();

	// Styled section header with icon support
	void SectionHeader(const char* label, const char* icon = nullptr);

	// Visual separator line
	void Separator(float thickness = 1.0f, const ImVec4* color = nullptr);

	// Consistent spacing - adds vertical space
	void PushSpacing(int count = 1);

	// Visual divider with label
	void LabeledDivider(const char* label);

	// ============================================================================
	// Button Styling & Controls
	// ============================================================================

	bool InspectorStyledButton(const char* label, const ImVec2& size, const ImVec4& tint, float tintStrength);
	bool InspectorActionButton(const char* label);
	bool InspectorHalfButton(const char* label);
	bool InspectorDangerButton(const char* label);

	// New button variants
	bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool SuccessButton(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool WarningButton(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

	// Icon buttons (compact)
	bool IconButton(const char* icon, const ImVec2& size = ImVec2(32, 32), const char* tooltip = nullptr);

	// ============================================================================
	// Docking Support
	// ============================================================================

	// Initialize docking system (call after ImGui::CreateContext)
	void SetupDocking();

	// Create the main editor dockspace (call in render loop after ImGui::NewFrame)
	ImGuiID CreateEditorDockSpace();

	// Default flags for dockable panels (allows movement, resizing, docking)
	constexpr ImGuiWindowFlags DockablePanelFlags = ImGuiWindowFlags_None;

	// Configure window for optimal docking
	void SetupDockableWindow(const char* windowName, ImGuiWindowFlags& flags);

	// Begin a dockable panel with consistent styling
	bool BeginDockablePanel(const char* title, bool* p_open, ImGuiWindowFlags flags = 0);

	// End a dockable panel
	void EndDockablePanel();

	// ============================================================================
	// Layout Zone Helpers (for tabbed panel organization)
	// ============================================================================

	// Begin a panel in a specific dock zone
	bool BeginDockedPanel(const char* title, bool* p_open, int zoneHint);

	// Snap current window to a dock zone
	void SnapToDockZone(int zoneHint);

	// ============================================================================
	// Text & Typography Styling
	// ============================================================================

	// Push styles for different text emphasis levels
	void PushHeadingStyle();
	void PushSubheadingStyle();
	void PushMutedTextStyle();
	void PopTextStyle();

	// Formatted label with custom color
	void ColoredLabel(const char* label, const ImVec4& color);

	// ============================================================================
	// Input Field Styling
	// ============================================================================

	// Input field with rounded corners and enhanced styling
	bool StyledInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
	bool StyledInputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
	bool StyledInputInt(const char* label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);

	// ============================================================================
	// Visual Feedback & Indicators
	// ============================================================================

	// Status indicator with color
	void StatusIndicator(const char* label, const ImVec4& color, bool active = true);

	// Progress indicator
	void ProgressBar(float fraction, const ImVec2& size = ImVec2(-1, 0), const char* label = nullptr);

	// ============================================================================
	// Theming Utilities
	// ============================================================================

	// Push/Pop color scope helpers
	void PushButtonColors(const ImVec4& bgColor, const ImVec4& hoverColor, const ImVec4& activeColor);
	void PopButtonColors();
}
#endif
