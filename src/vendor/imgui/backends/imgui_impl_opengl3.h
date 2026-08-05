#pragma once
#include "imgui.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ImGui_ImplOpenGL3_Init(const char* glsl_version);
void ImGui_ImplOpenGL3_Shutdown();
void ImGui_ImplOpenGL3_NewFrame();
void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);

#ifdef __cplusplus
}
#endif
