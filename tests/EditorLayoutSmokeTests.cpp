#include "editor/PanelDockManager.h"

#include <cmath>
#include <iostream>

namespace
{
	bool NearlyEqual(float a, float b, float epsilon = 0.001f)
	{
		return std::fabs(a - b) <= epsilon;
	}
}

bool EditorLayoutSmokeTest()
{
#ifndef USE_IMGUI
	return true;
#else
	auto& dockManager = MyEngine::Editor::PanelDockManager::Instance();
	dockManager.SetViewportSize(1280.0f, 720.0f);
	dockManager.ResetLayout();

	const ImVec2 leftPos = dockManager.GetPanelPos("Scene Hierarchy", 1280.0f, 720.0f);
	const ImVec2 leftSize = dockManager.GetPanelSize("Scene Hierarchy", 1280.0f, 720.0f);
	if (!NearlyEqual(leftPos.x, 5.0f) || !NearlyEqual(leftPos.y, 20.0f) ||
		!NearlyEqual(leftSize.x, 300.0f) || !NearlyEqual(leftSize.y, 620.0f))
	{
		std::cerr << "EditorLayoutSmokeTest: left sidebar layout mismatch" << std::endl;
		return false;
	}

	const ImVec2 rightPos = dockManager.GetPanelPos("Inspector", 1280.0f, 720.0f);
	const ImVec2 rightSize = dockManager.GetPanelSize("Inspector", 1280.0f, 720.0f);
	if (!NearlyEqual(rightPos.x, 965.0f) || !NearlyEqual(rightPos.y, 20.0f) ||
		!NearlyEqual(rightSize.x, 300.0f) || !NearlyEqual(rightSize.y, 620.0f))
	{
		std::cerr << "EditorLayoutSmokeTest: right sidebar layout mismatch" << std::endl;
		return false;
	}

	const ImVec2 floatingPos = dockManager.GetPanelPos("Material Browser", 1280.0f, 720.0f);
	const ImVec2 floatingSize = dockManager.GetPanelSize("Material Browser", 1280.0f, 720.0f);
	if (!NearlyEqual(floatingPos.x, 440.0f) || !NearlyEqual(floatingPos.y, 60.0f) ||
		!NearlyEqual(floatingSize.x, 400.0f) || !NearlyEqual(floatingSize.y, 600.0f))
	{
		std::cerr << "EditorLayoutSmokeTest: floating panel should reset to centered anchor" << std::endl;
		return false;
	}

	const ImVec2 unknownPos = dockManager.GetPanelPos("UnknownPanel", 1280.0f, 720.0f);
	const ImVec2 unknownSize = dockManager.GetPanelSize("UnknownPanel", 1280.0f, 720.0f);
	if (!NearlyEqual(unknownPos.x, 100.0f) || !NearlyEqual(unknownPos.y, 100.0f) ||
		!NearlyEqual(unknownSize.x, 300.0f) || !NearlyEqual(unknownSize.y, 400.0f))
	{
		std::cerr << "EditorLayoutSmokeTest: unknown panel fallback changed unexpectedly" << std::endl;
		return false;
	}

	return true;
#endif
}
