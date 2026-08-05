#pragma once
#include <GLFW/glfw3.h>
#include "imgui.h"

// Initialize and shutdown the GLFW platform backend for ImGui.
// install_callbacks: if true, the backend will install its own GLFW callbacks
// (mouse, scroll, key, char). If your application already uses callbacks,
// set this to false and forward events to ImGui_ImplGlfw_*Callback manually.
bool ImGui_ImplGlfw_InitForOpenGL(GLFWwindow* window, bool install_callbacks);
void ImGui_ImplGlfw_Shutdown();
void ImGui_ImplGlfw_NewFrame();

// Optional callbacks (call these from your own GLFW callbacks if not
// letting the backend install them).
void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c);
