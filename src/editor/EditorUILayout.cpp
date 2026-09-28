#ifdef USE_IMGUI
#include "EditorUILayout.h"
#include <imgui.h>

namespace MyEngine::Editor::Layout
{
	static LayoutState g_LayoutState;

	void InitializeLayout(float windowWidth, float windowHeight)
	{
		g_LayoutState.config.viewportWidth = windowWidth;
		g_LayoutState.config.viewportHeight = windowHeight;
	}

	void UpdateLayout(float windowWidth, float windowHeight)
	{
		g_LayoutState.config.viewportWidth = windowWidth;
		g_LayoutState.config.viewportHeight = windowHeight;
	}

	ImVec2 GetDockZonePosition(DockZone zone)
	{
		auto& config = g_LayoutState.config;
		float padding = config.panelPadding;
		float topOffset = config.topMargin;

		switch (zone)
		{
		case DockZone::Left:
			return ImVec2(padding, topOffset);

		case DockZone::Right:
			return ImVec2(
				config.viewportWidth - config.rightZoneWidth - padding,
				topOffset
			);

		case DockZone::Center:
			return ImVec2(
				config.leftZoneWidth + padding * 2,
				topOffset
			);

		case DockZone::Bottom:
			return ImVec2(
				padding,
				config.viewportHeight - config.bottomZoneHeight - padding
			);

		case DockZone::TopLeft:
			return ImVec2(padding, topOffset);

		case DockZone::TopRight:
			return ImVec2(
				config.viewportWidth - config.rightZoneWidth - padding,
				topOffset
			);

		default:
			return ImVec2(0, 0);
		}
	}

	ImVec2 GetDockZoneSize(DockZone zone)
	{
		auto& config = g_LayoutState.config;
		float padding = config.panelPadding;
		float topOffset = config.topMargin;

		switch (zone)
		{
		case DockZone::Left:
			return ImVec2(
				config.leftZoneWidth,
				config.viewportHeight - topOffset - config.bottomZoneHeight - padding * 3
			);

		case DockZone::Right:
			return ImVec2(
				config.rightZoneWidth,
				config.viewportHeight - topOffset - config.bottomZoneHeight - padding * 3
			);

		case DockZone::Center:
			return ImVec2(
				config.viewportWidth - config.leftZoneWidth - config.rightZoneWidth - padding * 4,
				config.viewportHeight - topOffset - config.bottomZoneHeight - padding * 3
			);

		case DockZone::Bottom:
			return ImVec2(
				config.viewportWidth - padding * 2,
				config.bottomZoneHeight
			);

		case DockZone::TopLeft:
			return ImVec2(
				config.leftZoneWidth,
				topOffset
			);

		case DockZone::TopRight:
			return ImVec2(
				config.rightZoneWidth,
				topOffset
			);

		default:
			return ImVec2(100, 100);
		}
	}

	bool BeginDockZoneLeft()
	{
		ImGui::SetNextWindowPos(GetDockZonePosition(DockZone::Left), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(GetDockZoneSize(DockZone::Left), ImGuiCond_FirstUseEver);

		bool visible = ImGui::Begin("##LeftDockZone", nullptr, 
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		return visible;
	}

	bool BeginDockZoneRight()
	{
		ImGui::SetNextWindowPos(GetDockZonePosition(DockZone::Right), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(GetDockZoneSize(DockZone::Right), ImGuiCond_FirstUseEver);

		bool visible = ImGui::Begin("##RightDockZone", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		return visible;
	}

	bool BeginDockZoneBottom()
	{
		ImGui::SetNextWindowPos(GetDockZonePosition(DockZone::Bottom), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(GetDockZoneSize(DockZone::Bottom), ImGuiCond_FirstUseEver);

		bool visible = ImGui::Begin("##BottomDockZone", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		return visible;
	}

	void EndDockZone()
	{
		ImGui::End();
	}

	bool BeginPanelTab(const char* label, bool* p_open)
	{
		ImGuiTabItemFlags flags = 0;
		if (p_open && !*p_open)
			flags |= ImGuiTabItemFlags_SetSelected;

		return ImGui::BeginTabItem(label, p_open, flags);
	}

	void EndPanelTab()
	{
		ImGui::EndTabItem();
	}

	LayoutState& GetLayoutState()
	{
		return g_LayoutState;
	}

	void SnapWindowToDockZone(DockZone zone)
	{
		ImVec2 pos = GetDockZonePosition(zone);
		ImVec2 size = GetDockZoneSize(zone);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
	}

	void SaveLayout(const std::string& path)
	{
		// TODO: Implement layout persistence to INI file
		// Store zone sizes, visible panels, active tabs
	}

	void LoadLayout(const std::string& path)
	{
		// TODO: Implement layout loading from INI file
		// Restore zone sizes, visible panels, active tabs
	}
}

#endif
