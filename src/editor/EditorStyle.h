#pragma once

#ifdef USE_IMGUI
#include <imgui.h>

namespace MyEngine::Editor
{
	ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t);
	void ApplyEditorTheme();
	bool BeginInspectorSection(const char* title, bool defaultOpen = true);
	void InspectorGroupLabel(const char* label);
	void InspectorFullWidth();
	bool InspectorStyledButton(const char* label, const ImVec2& size, const ImVec4& tint, float tintStrength);
	bool InspectorActionButton(const char* label);
	bool InspectorHalfButton(const char* label);
	bool InspectorDangerButton(const char* label);
}
#endif
