#include "EditorStyle.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <cstring>

#ifdef USE_IMGUI
namespace MyEngine::Editor
{
	// ============================================================================
	// Theme Color Storage
	// ============================================================================

	static std::unordered_map<std::string, ImVec4> g_ThemeColors;
	static float g_EditorUiScale = 1.0f;
	static bool g_EditorUiCompactMode = false;

	// ============================================================================
	// Color Utilities
	// ============================================================================

	ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
	{
		return ImVec4(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}

	ImVec4 GetThemeColor(const char* semanticName)
	{
		auto it = g_ThemeColors.find(semanticName);
		if (it != g_ThemeColors.end())
			return it->second;
		return ImVec4(1, 1, 1, 1); // Default white
	}

	float GetEditorUiScale()
	{
		return g_EditorUiScale;
	}

	void SetEditorUiScale(float scale)
	{
		g_EditorUiScale = std::clamp(scale, 0.80f, 1.35f);
		if (ImGui::GetCurrentContext())
			ApplyEditorTheme();
	}

	bool IsEditorUiCompactMode()
	{
		return g_EditorUiCompactMode;
	}

	void SetEditorUiCompactMode(bool compact)
	{
		g_EditorUiCompactMode = compact;
		if (ImGui::GetCurrentContext())
			ApplyEditorTheme();
	}

	void ResetEditorUiAppearance()
	{
		g_EditorUiScale = 1.0f;
		g_EditorUiCompactMode = false;
		if (ImGui::GetCurrentContext())
			ApplyEditorTheme();
	}

	// ============================================================================
	// Professional Dark Enterprise Theme
	// ============================================================================

	void ApplyEditorTheme()
	{
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();

		const float compactFactor = g_EditorUiCompactMode ? 0.88f : 1.0f;
		const float densityScale = g_EditorUiScale * compactFactor;

		// Refined corner radii for professional look
		style.WindowRounding = Spacing::CornerRadiusLarge * g_EditorUiScale;
		style.ChildRounding = 6.0f * g_EditorUiScale;
		style.FrameRounding = Spacing::CornerRadiusNormal * g_EditorUiScale;
		style.PopupRounding = 6.0f * g_EditorUiScale;
		style.ScrollbarRounding = Spacing::CornerRadiusLarge * g_EditorUiScale;
		style.GrabRounding = Spacing::CornerRadiusNormal * g_EditorUiScale;
		style.TabRounding = Spacing::CornerRadiusNormal * g_EditorUiScale;

		// Improved spacing hierarchy
		style.WindowPadding = ImVec2(Spacing::PaddingLarge * densityScale, Spacing::PaddingLarge * densityScale);
		style.FramePadding = ImVec2((Spacing::PaddingLarge - 2.0f) * densityScale, (Spacing::PaddingSmall + 2.0f) * densityScale);
		style.CellPadding = ImVec2(Spacing::PaddingNormal * densityScale, (Spacing::PaddingSmall + 2.0f) * densityScale);
		style.ItemSpacing = ImVec2(Spacing::SpacingNormal * densityScale, (Spacing::SpacingSmall + 4.0f) * densityScale);
		style.ItemInnerSpacing = ImVec2((Spacing::SpacingSmall + 4.0f) * densityScale, (Spacing::SpacingSmall + 2.0f) * densityScale);
		style.IndentSpacing = 20.0f * densityScale;
		style.ScrollbarSize = 14.0f * densityScale;
		style.GrabMinSize = 12.0f * densityScale;

		// Professional border styling
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 0.5f;
		style.FrameBorderSize = 0.5f;
		style.PopupBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.WindowMenuButtonPosition = ImGuiDir_None;
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

		// ====================================================================
		// Professional Dark Color Palette
		// Optimized for: Eye comfort, semantic clarity, high contrast, hierarchy
		// ====================================================================

		ImVec4* colors = style.Colors;

		// Background layers - dark blue-gray gradation for depth
		const ImVec4 bg0 = ImVec4(0.086f, 0.090f, 0.106f, 1.00f);  // Darkest - main bg
		const ImVec4 bg1 = ImVec4(0.110f, 0.118f, 0.141f, 1.00f);  // Secondary
		const ImVec4 bg2 = ImVec4(0.138f, 0.149f, 0.180f, 1.00f);  // Tertiary (elevated)
		const ImVec4 bg3 = ImVec4(0.167f, 0.180f, 0.216f, 1.00f);  // Quaternary
		const ImVec4 bg4 = ImVec4(0.196f, 0.212f, 0.255f, 1.00f);  // Highest elevation

		// Primary accent - professional blue
		const ImVec4 accent = ImVec4(0.212f, 0.557f, 0.922f, 1.00f);
		const ImVec4 accentDim = ImVec4(0.212f, 0.557f, 0.922f, 0.20f);
		const ImVec4 accentSoft = ImVec4(0.212f, 0.557f, 0.922f, 0.30f);
		const ImVec4 accentStrong = ImVec4(0.294f, 0.627f, 0.980f, 1.00f);

		// Semantic colors
		const ImVec4 successColor = ImVec4(0.235f, 0.680f, 0.478f, 1.00f);  // Teal
		const ImVec4 warningColor = ImVec4(0.961f, 0.733f, 0.255f, 1.00f);  // Amber
		const ImVec4 errorColor = ImVec4(0.910f, 0.357f, 0.325f, 1.00f);    // Red
		const ImVec4 infoColor = ImVec4(0.145f, 0.662f, 0.914f, 1.00f);     // Light blue

		// Text - high contrast for readability
		const ImVec4 text = ImVec4(0.922f, 0.933f, 0.949f, 1.00f);          // ~93% brightness
		const ImVec4 textMuted = ImVec4(0.576f, 0.596f, 0.627f, 1.00f);     // ~60% brightness
		const ImVec4 textDisabled = ImVec4(0.392f, 0.408f, 0.431f, 1.00f);  // ~40% brightness

		// Borders - subtle but visible
		const ImVec4 border = ImVec4(0.196f, 0.216f, 0.255f, 0.60f);
		const ImVec4 borderStrong = ImVec4(0.255f, 0.282f, 0.333f, 0.85f);

		// Store semantic colors for later access
		g_ThemeColors["accent"] = accent;
		g_ThemeColors["success"] = successColor;
		g_ThemeColors["warning"] = warningColor;
		g_ThemeColors["error"] = errorColor;
		g_ThemeColors["info"] = infoColor;

		// Core window styling
		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textDisabled;
		colors[ImGuiCol_WindowBg] = bg0;
		colors[ImGuiCol_ChildBg] = ImVec4(bg1.x, bg1.y, bg1.z, 0.95f);
		colors[ImGuiCol_PopupBg] = ImVec4(bg0.x, bg0.y, bg0.z, 0.99f);
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

		// Frame styling (input fields, dropdowns)
		colors[ImGuiCol_FrameBg] = bg1;
		colors[ImGuiCol_FrameBgHovered] = bg2;
		colors[ImGuiCol_FrameBgActive] = accentDim;

		// Title bar
		colors[ImGuiCol_TitleBg] = bg0;
		colors[ImGuiCol_TitleBgActive] = bg2;
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(bg0.x, bg0.y, bg0.z, 0.75f);

		// Menu bar
		colors[ImGuiCol_MenuBarBg] = bg1;

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4(bg0.x, bg0.y, bg0.z, 0.60f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(bg2.x, bg2.y, bg2.z, 0.80f);
		colors[ImGuiCol_ScrollbarGrabHovered] = bg3;
		colors[ImGuiCol_ScrollbarGrabActive] = accent;

		// Checkmark, slider, etc.
		colors[ImGuiCol_CheckMark] = accentStrong;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentStrong;

		// Button styling
		colors[ImGuiCol_Button] = bg2;
		colors[ImGuiCol_ButtonHovered] = LerpColor(bg2, accent, 0.45f);
		colors[ImGuiCol_ButtonActive] = LerpColor(bg2, accentStrong, 0.65f);

		// Header (tree nodes, collapsible sections)
		colors[ImGuiCol_Header] = bg2;
		colors[ImGuiCol_HeaderHovered] = LerpColor(bg2, accent, 0.40f);
		colors[ImGuiCol_HeaderActive] = LerpColor(bg2, accentStrong, 0.55f);

		// Separator
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = accentSoft;
		colors[ImGuiCol_SeparatorActive] = accent;

		// Resize handles
		colors[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(accentStrong.x, accentStrong.y, accentStrong.z, 0.80f);

		// Tab styling
		colors[ImGuiCol_Tab] = bg1;
		colors[ImGuiCol_TabHovered] = LerpColor(bg1, accent, 0.35f);
		colors[ImGuiCol_TabActive] = LerpColor(bg1, accent, 0.55f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(bg0.x, bg0.y, bg0.z, 0.80f);
		colors[ImGuiCol_TabUnfocusedActive] = LerpColor(bg1, accent, 0.25f);

		// Plot data
		colors[ImGuiCol_PlotLines] = ImVec4(0.608f, 0.627f, 0.659f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = accentStrong;
		colors[ImGuiCol_PlotHistogram] = warningColor;
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(warningColor.x, warningColor.y, warningColor.z, 1.0f);

		// Text selection
		colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

		// Drag and drop
		colors[ImGuiCol_DragDropTarget] = ImVec4(warningColor.x, warningColor.y, warningColor.z, 0.95f);

		// Navigation
		colors[ImGuiCol_NavHighlight] = ImVec4(accentStrong.x, accentStrong.y, accentStrong.z, 0.80f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);

		// Table styling
		colors[ImGuiCol_TableHeaderBg] = bg2;
		colors[ImGuiCol_TableBorderStrong] = borderStrong;
		colors[ImGuiCol_TableBorderLight] = border;
		colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0.0f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
	}

	// ============================================================================
	// Panel & Section Styling
	// ============================================================================

	bool BeginInspectorSection(const char* title, bool defaultOpen)
	{
		ImGui::Spacing();
		bool result = ImGui::CollapsingHeader(
			title,
			defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
		ImGui::Spacing();
		return result;
	}

	void EndInspectorSection()
	{
		// Optional: Can be used for cleanup if needed in the future
	}

	void InspectorGroupLabel(const char* label)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColor("accent"));
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::Spacing();
	}

	void InspectorFullWidth()
	{
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	}

	void SectionHeader(const char* label, const char* icon)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColor("accent"));
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		if (icon)
			ImGui::Text("%s %s", icon, label);
		else
			ImGui::Text("%s", label);
		ImGui::PopFont();
		ImGui::PopStyleColor();

		// Thicker separator under header for emphasis
		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.5f);
		ImGui::Separator();
		ImGui::PopStyleVar();
		ImGui::Spacing();
	}

	void Separator(float thickness, const ImVec4* color)
	{
		if (color)
			ImGui::PushStyleColor(ImGuiCol_Separator, *color);

		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, thickness);
		ImGui::Separator();
		ImGui::PopStyleVar();

		if (color)
			ImGui::PopStyleColor();
	}

	void PushSpacing(int count)
	{
		for (int i = 0; i < count; ++i)
			ImGui::Spacing();
	}

	void LabeledDivider(const char* label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColor("accent"));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
		ImGui::Spacing();
		ImGui::SeparatorText(label);
		ImGui::PopStyleColor(2);
		ImGui::Spacing();
	}

	// ============================================================================
	// Button Styling & Controls
	// ============================================================================

	bool InspectorStyledButton(const char* label, const ImVec2& size, const ImVec4& tint, float tintStrength)
	{
		ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
		ImVec4 hovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
		ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.5f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Spacing::CornerRadiusNormal);
		ImGui::PushStyleColor(ImGuiCol_Button, LerpColor(base, tint, tintStrength));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LerpColor(hovered, tint, tintStrength + 0.10f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, LerpColor(active, tint, tintStrength + 0.20f));

		bool pressed = ImGui::Button(label, size);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		return pressed;
	}

	bool InspectorActionButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f),
			GetThemeColor("accent"),
			0.20f);
	}

	bool InspectorHalfButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f),
			GetThemeColor("accent"),
			0.20f);
	}

	bool InspectorDangerButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f),
			GetThemeColor("error"),
			0.40f);
	}

	bool PrimaryButton(const char* label, const ImVec2& size)
	{
		return InspectorStyledButton(label, size, GetThemeColor("accent"), 0.25f);
	}

	bool SecondaryButton(const char* label, const ImVec2& size)
	{
		ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_Button);
		return InspectorStyledButton(label, size, ImVec4(bg.x * 1.2f, bg.y * 1.2f, bg.z * 1.2f, 1.0f), 0.1f);
	}

	bool SuccessButton(const char* label, const ImVec2& size)
	{
		return InspectorStyledButton(label, size, GetThemeColor("success"), 0.30f);
	}

	bool WarningButton(const char* label, const ImVec2& size)
	{
		return InspectorStyledButton(label, size, GetThemeColor("warning"), 0.25f);
	}

	bool DangerButton(const char* label, const ImVec2& size)
	{
		return InspectorStyledButton(label, size, GetThemeColor("error"), 0.40f);
	}

	bool IconButton(const char* icon, const ImVec2& size, const char* tooltip)
	{
		bool pressed = ImGui::Button(icon, size);
		if (tooltip && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", tooltip);
		}
		return pressed;
	}

	// ============================================================================
	// Docking Support
	// ============================================================================

	void SetupDocking()
	{
		// Docking support would require ImGui to be compiled with IMGUI_ENABLE_DOCKING
		// The vcpkg build doesn't include it, so we use tab-based organization instead
	}

	ImGuiID CreateEditorDockSpace()
	{
		// With docking unavailable, we manage windows through tabs and zones
		// See main.cpp for the tab-based layout implementation
		return 0;
	}

	void SetupDockableWindow(const char* windowName, ImGuiWindowFlags& flags)
	{
		// Enhanced docking support for future ImGui versions with built-in docking
		// Placeholder for when docking branch is integrated
	}

	bool BeginDockablePanel(const char* title, bool* p_open, ImGuiWindowFlags flags)
	{
		// Apply professional panel styling
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		SetupDockableWindow(title, flags);
		bool result = ImGui::Begin(title, p_open, flags);
		return result;
	}

	// End panel with proper cleanup
	void EndDockablePanel()
	{
		ImGui::End();
		ImGui::PopStyleVar();
	}

	// ============================================================================
	// Text & Typography Styling
	// ============================================================================

	void PushHeadingStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColor("accent"));
	}

	void PushSubheadingStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
	}

	void PushMutedTextStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}

	void PopTextStyle()
	{
		ImGui::PopStyleColor();
	}

	void ColoredLabel(const char* label, const ImVec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
	}

	// ============================================================================
	// Input Field Styling
	// ============================================================================

	bool StyledInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags)
	{
		InspectorFullWidth();
		return ImGui::InputText(label, buf, buf_size, flags);
	}

	bool StyledInputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags)
	{
		InspectorFullWidth();
		return ImGui::InputFloat(label, v, step, step_fast, format, flags);
	}

	bool StyledInputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags)
	{
		InspectorFullWidth();
		return ImGui::InputInt(label, v, step, step_fast, flags);
	}

	// ============================================================================
	// Visual Feedback & Indicators
	// ============================================================================

	void StatusIndicator(const char* label, const ImVec4& color, bool active)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::AlignTextToFramePadding();
		if (active)
			ImGui::Text("● %s", label);
		else
			ImGui::Text("○ %s", label);
		ImGui::PopStyleColor();
	}

	void ProgressBar(float fraction, const ImVec2& size, const char* label)
	{
		ImGui::ProgressBar(fraction, size, label);
	}

	// ============================================================================
	// Color Scope Helpers
	// ============================================================================

	void PushButtonColors(const ImVec4& bgColor, const ImVec4& hoverColor, const ImVec4& activeColor)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
	}

	void PopButtonColors()
	{
		ImGui::PopStyleColor(3);
	}

	// ============================================================================
	// Layout Zone Helpers
	// ============================================================================

	bool BeginDockedPanel(const char* title, bool* p_open, int zoneHint)
	{
		// Use Layout system if available to position panel
		// For now, use standard Begin with dockable flags
		return ImGui::Begin(title, p_open, DockablePanelFlags);
	}

	void SnapToDockZone(int zoneHint)
	{
		// Helper to snap a window to a specific zone
		// Implementation depends on layout system integration
	}
}
#endif
