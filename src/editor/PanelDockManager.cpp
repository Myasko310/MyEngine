#ifdef USE_IMGUI
#include "PanelDockManager.h"
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
		// Default panel layouts
		RegisterPanel("Scene Hierarchy", PanelPosition::LeftSidebar);
		RegisterPanel("Asset Browser", PanelPosition::LeftSidebar);
		RegisterPanel("Inspector", PanelPosition::RightSidebar);
		RegisterPanel("Performance", PanelPosition::BottomPanel);
		RegisterPanel("Material Browser", PanelPosition::Floating);
		RegisterPanel("Animation State Machine", PanelPosition::Floating);
		RegisterPanel("Physics", PanelPosition::Floating);
		RegisterPanel("Scripting", PanelPosition::Floating);
	}

	void PanelDockManager::RegisterPanel(const char* panelName, PanelPosition position)
	{
		if (!panelName) return;
		panels[panelName] = PanelState{ position, ImVec2(400, 600) };
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
			return ImVec2(panel.size.x + panel.customOffset.x, panel.size.y + panel.customOffset.y);
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
		for (auto& [name, state] : panels)
		{
			state.customOffset = ImVec2(0, 0);
		}
	}
}

#endif
