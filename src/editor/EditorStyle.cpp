#include "EditorStyle.h"

#ifdef USE_IMGUI
namespace MyEngine::Editor
{
	ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
	{
		return ImVec4(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}

	void ApplyEditorTheme()
	{
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 8.0f;
		style.ChildRounding = 7.0f;
		style.FrameRounding = 5.0f;
		style.PopupRounding = 6.0f;
		style.ScrollbarRounding = 8.0f;
		style.GrabRounding = 5.0f;
		style.TabRounding = 5.0f;

		style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.FramePadding = ImVec2(10.0f, 6.0f);
		style.CellPadding = ImVec2(8.0f, 6.0f);
		style.ItemSpacing = ImVec2(10.0f, 8.0f);
		style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
		style.IndentSpacing = 20.0f;
		style.ScrollbarSize = 15.0f;
		style.GrabMinSize = 11.0f;

		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.WindowMenuButtonPosition = ImGuiDir_None;
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

		ImVec4* colors = style.Colors;
		const ImVec4 bg0 = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
		const ImVec4 bg1 = ImVec4(0.13f, 0.145f, 0.17f, 1.00f);
		const ImVec4 bg2 = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
		const ImVec4 bg3 = ImVec4(0.21f, 0.24f, 0.29f, 1.00f);
		const ImVec4 accent = ImVec4(0.29f, 0.57f, 0.95f, 1.00f);
		const ImVec4 accentSoft = ImVec4(0.29f, 0.57f, 0.95f, 0.35f);
		const ImVec4 accentStrong = ImVec4(0.36f, 0.66f, 1.00f, 1.00f);
		const ImVec4 text = ImVec4(0.93f, 0.95f, 0.97f, 1.00f);
		const ImVec4 textMuted = ImVec4(0.58f, 0.61f, 0.66f, 1.00f);
		const ImVec4 border = ImVec4(0.20f, 0.24f, 0.30f, 0.70f);
		const ImVec4 borderStrong = ImVec4(0.29f, 0.34f, 0.42f, 0.95f);

		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textMuted;
		colors[ImGuiCol_WindowBg] = bg0;
		colors[ImGuiCol_ChildBg] = bg1;
		colors[ImGuiCol_PopupBg] = ImVec4(bg0.x, bg0.y, bg0.z, 0.98f);
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
		colors[ImGuiCol_FrameBg] = bg1;
		colors[ImGuiCol_FrameBgHovered] = bg2;
		colors[ImGuiCol_FrameBgActive] = accentSoft;
		colors[ImGuiCol_TitleBg] = bg0;
		colors[ImGuiCol_TitleBgActive] = bg1;
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(bg0.x, bg0.y, bg0.z, 0.78f);
		colors[ImGuiCol_MenuBarBg] = bg1;
		colors[ImGuiCol_ScrollbarBg] = bg0;
		colors[ImGuiCol_ScrollbarGrab] = bg2;
		colors[ImGuiCol_ScrollbarGrabHovered] = bg3;
		colors[ImGuiCol_ScrollbarGrabActive] = accent;
		colors[ImGuiCol_CheckMark] = accentStrong;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentStrong;
		colors[ImGuiCol_Button] = bg2;
		colors[ImGuiCol_ButtonHovered] = LerpColor(bg2, accent, 0.55f);
		colors[ImGuiCol_ButtonActive] = LerpColor(bg2, accentStrong, 0.75f);
		colors[ImGuiCol_Header] = bg2;
		colors[ImGuiCol_HeaderHovered] = LerpColor(bg2, accent, 0.50f);
		colors[ImGuiCol_HeaderActive] = LerpColor(bg2, accentStrong, 0.65f);
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = accentSoft;
		colors[ImGuiCol_SeparatorActive] = accent;
		colors[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(accentStrong.x, accentStrong.y, accentStrong.z, 0.85f);
		colors[ImGuiCol_Tab] = bg1;
		colors[ImGuiCol_TabHovered] = LerpColor(bg1, accent, 0.45f);
		colors[ImGuiCol_TabActive] = LerpColor(bg1, accent, 0.65f);
		colors[ImGuiCol_TabUnfocused] = bg0;
		colors[ImGuiCol_TabUnfocusedActive] = bg2;
		colors[ImGuiCol_PlotLines] = ImVec4(0.70f, 0.73f, 0.78f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = accentStrong;
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.89f, 0.64f, 0.25f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.98f, 0.73f, 0.32f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.32f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.98f, 0.78f, 0.28f, 0.95f);
		colors[ImGuiCol_NavHighlight] = ImVec4(accentStrong.x, accentStrong.y, accentStrong.z, 0.85f);
		colors[ImGuiCol_TableHeaderBg] = bg1;
		colors[ImGuiCol_TableBorderStrong] = borderStrong;
		colors[ImGuiCol_TableBorderLight] = border;
		colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0.0f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
	}

	bool BeginInspectorSection(const char* title, bool defaultOpen)
	{
		ImGui::Spacing();
		return ImGui::CollapsingHeader(
			title,
			defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
	}

	void InspectorGroupLabel(const char* label)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", label);
		ImGui::Separator();
	}

	void InspectorFullWidth()
	{
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	}

	bool InspectorStyledButton(const char* label, const ImVec2& size, const ImVec4& tint, float tintStrength)
	{
		ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
		ImVec4 hovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
		ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, LerpColor(base, tint, tintStrength));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LerpColor(hovered, tint, tintStrength + 0.10f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, LerpColor(active, tint, tintStrength + 0.15f));
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		return pressed;
	}

	bool InspectorActionButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f),
			ImVec4(0.29f, 0.57f, 0.95f, 1.0f),
			0.20f);
	}

	bool InspectorHalfButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f),
			ImVec4(0.29f, 0.57f, 0.95f, 1.0f),
			0.20f);
	}

	bool InspectorDangerButton(const char* label)
	{
		return InspectorStyledButton(
			label,
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f),
			ImVec4(0.77f, 0.28f, 0.28f, 1.0f),
			0.45f);
	}
}
#endif
