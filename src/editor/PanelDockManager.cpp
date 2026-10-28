#ifdef USE_IMGUI
#include "PanelDockManager.h"
#include <algorithm>
#include <imgui.h>

namespace MyEngine::Editor
{
	static PanelDockManager* g_PanelDockManager = nullptr;

	PanelDockManager& PanelDockManager::Instance()
	{
		if (!g_PanelDockManager)
			g_PanelDockManager = new PanelDockManager();
		return *g_PanelDockManager;
	}

	PanelDockManager::PanelDockManager()
	{
		ResetLayout();
	}

	void PanelDockManager::RegisterPanel(const char* panelName, PanelPosition position)
	{
		if (!panelName)
			return;

		PanelState state{};
		state.position = position;
		if (position == PanelPosition::LeftSidebar || position == PanelPosition::RightSidebar)
			state.size = ImVec2(300, viewportH - 100.0f);
		else if (position == PanelPosition::BottomPanel)
			state.size = ImVec2(viewportW - 625.0f, 90.0f);
		else
			state.size = ImVec2(400, 600);

		state.customOffset = ImVec2(0, 0);
		panels[panelName] = state;
	}

	ImVec2 PanelDockManager::GetPanelPos(const char* panelName, float viewportWidth, float viewportHeight)
	{
		SetViewportSize(viewportWidth, viewportHeight);

		if (panels.find(panelName) == panels.end())
			return ImVec2(100, 100); // Default fallback

		auto& panel = panels[panelName];
		switch (panel.position)
		{
		case PanelPosition::LeftSidebar:
			return GetLeftSidebarPos();
		case PanelPosition::RightSidebar:
			return GetRightSidebarPos();
		case PanelPosition::BottomPanel:
			return GetBottomPanelPos();
		case PanelPosition::CenterViewport:
			return ImVec2(320, 120);
		case PanelPosition::Floating:
		default:
		{
			const float clampedWidth = std::max(120.0f, panel.size.x);
			const float clampedHeight = std::max(90.0f, panel.size.y);
			const float anchorX = std::max(0.0f, (viewportW - clampedWidth) * 0.5f);
			const float anchorY = std::max(24.0f, (viewportH - clampedHeight) * 0.5f);
			const float maxX = std::max(0.0f, viewportW - clampedWidth - 5.0f);
			const float maxY = std::max(24.0f, viewportH - clampedHeight - 5.0f);
			const float x = std::clamp(anchorX + panel.customOffset.x, 0.0f, maxX);
			const float y = std::clamp(anchorY + panel.customOffset.y, 24.0f, maxY);
			return ImVec2(x, y);
		}
		}
	}

	ImVec2 PanelDockManager::GetPanelSize(const char* panelName, float viewportWidth, float viewportHeight)
	{
		SetViewportSize(viewportWidth, viewportHeight);

		if (panels.find(panelName) == panels.end())
			return ImVec2(300, 400); // Default fallback

		auto& panel = panels[panelName];
		switch (panel.position)
		{
		case PanelPosition::LeftSidebar:
			return GetLeftSidebarSize();
		case PanelPosition::RightSidebar:
			return GetRightSidebarSize();
		case PanelPosition::BottomPanel:
			return GetBottomPanelSize();
		case PanelPosition::CenterViewport:
			return ImVec2(viewportWidth - 640, viewportHeight - 200);
		case PanelPosition::Floating:
		default:
			return panel.size;
		}
	}

	void PanelDockManager::SetViewportSize(float width, float height)
	{
		viewportW = width;
		viewportH = height;
	}

	void PanelDockManager::SnapPanel(const char* panelName) const
	{
		ImVec2 pos = const_cast<PanelDockManager*>(this)->GetPanelPos(panelName, viewportW, viewportH);
		ImVec2 size = const_cast<PanelDockManager*>(this)->GetPanelSize(panelName, viewportW, viewportH);
		ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	}

	void PanelDockManager::ResetLayout()
	{
		panels.clear();
		RegisterPanel("Scene Hierarchy", PanelPosition::LeftSidebar);
		RegisterPanel("Asset Browser", PanelPosition::LeftSidebar);
		RegisterPanel("Inspector", PanelPosition::RightSidebar);
		RegisterPanel("Performance", PanelPosition::BottomPanel);
		RegisterPanel("Material Browser", PanelPosition::Floating);
		RegisterPanel("Animation State Machine", PanelPosition::Floating);
		RegisterPanel("Physics", PanelPosition::Floating);
		RegisterPanel("Scripting", PanelPosition::Floating);
	}
}

#endif
