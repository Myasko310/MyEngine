// GLFW backend for ImGui: provides time, mouse/keyboard input and clipboard support.
// Adapted and simplified from Dear ImGui examples to satisfy runtime input needs.
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include <cstring>

static GLFWwindow* g_Window = nullptr;
static double g_Time = 0.0;

// Optional: store previous callbacks so we can chain to them
static GLFWmousebuttonfun g_PrevUserCallbackMousebutton = nullptr;
static GLFWscrollfun g_PrevUserCallbackScroll = nullptr;
static GLFWkeyfun g_PrevUserCallbackKey = nullptr;
static GLFWcharfun g_PrevUserCallbackChar = nullptr;

bool ImGui_ImplGlfw_InitForOpenGL(GLFWwindow* window, bool install_callbacks)
{
	g_Window = window;
	g_Time = 0.0;

	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "imgui_impl_glfw";

	if (install_callbacks && g_Window)
	{
		g_PrevUserCallbackMousebutton = glfwSetMouseButtonCallback(g_Window, [](GLFWwindow* w, int button, int action, int mods){
			ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
			if (g_PrevUserCallbackMousebutton) g_PrevUserCallbackMousebutton(w, button, action, mods);
		});
		g_PrevUserCallbackScroll = glfwSetScrollCallback(g_Window, [](GLFWwindow* w, double x, double y){
			ImGui_ImplGlfw_ScrollCallback(w, x, y);
			if (g_PrevUserCallbackScroll) g_PrevUserCallbackScroll(w, x, y);
		});
		g_PrevUserCallbackKey = glfwSetKeyCallback(g_Window, [](GLFWwindow* w, int key, int scancode, int action, int mods){
			ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);
			if (g_PrevUserCallbackKey) g_PrevUserCallbackKey(w, key, scancode, action, mods);
		});
		g_PrevUserCallbackChar = glfwSetCharCallback(g_Window, [](GLFWwindow* w, unsigned int c){
			ImGui_ImplGlfw_CharCallback(w, c);
			if (g_PrevUserCallbackChar) g_PrevUserCallbackChar(w, c);
		});
	}

	return true;
}

void ImGui_ImplGlfw_Shutdown()
{
	// restore previous callbacks (best-effort)
	if (g_Window)
	{
		if (g_PrevUserCallbackMousebutton) glfwSetMouseButtonCallback(g_Window, g_PrevUserCallbackMousebutton);
		if (g_PrevUserCallbackScroll) glfwSetScrollCallback(g_Window, g_PrevUserCallbackScroll);
		if (g_PrevUserCallbackKey) glfwSetKeyCallback(g_Window, g_PrevUserCallbackKey);
		if (g_PrevUserCallbackChar) glfwSetCharCallback(g_Window, g_PrevUserCallbackChar);
	}
	g_Window = nullptr;
}

void ImGui_ImplGlfw_NewFrame()
{
	if (!g_Window)
		return;

	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(g_Window, &w, &h);
	glfwGetFramebufferSize(g_Window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)w, (float)h);
	if (w > 0 && h > 0)
		io.DisplayFramebufferScale = ImVec2((float)display_w / (float)w, (float)display_h / (float)h);

	// Setup time step
	double current_time = glfwGetTime();
	io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f/60.0f);
	g_Time = current_time;

	// Mouse position
	double mx, my;
	glfwGetCursorPos(g_Window, &mx, &my);
	io.MousePos = ImVec2((float)mx, (float)my);

	// Mouse buttons (submit modern events)
	for (int i = 0; i < 3; i++)
		 io.AddMouseButtonEvent(i, glfwGetMouseButton(g_Window, i) == GLFW_PRESS);

	// Modifier keys: submit left/right separately
	io.AddKeyEvent(ImGuiKey_LeftCtrl, glfwGetKey(g_Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightCtrl, glfwGetKey(g_Window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftShift, glfwGetKey(g_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightShift, glfwGetKey(g_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftAlt, glfwGetKey(g_Window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightAlt, glfwGetKey(g_Window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftSuper, glfwGetKey(g_Window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightSuper, glfwGetKey(g_Window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS);

	// NOTE: glfw callbacks will feed characters and keys when callbacks are installed.
}

// Callbacks
void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	(void)mods;
	ImGuiIO& io = ImGui::GetIO();
	if (button >= 0 && button < 3)
		io.MouseDown[button] = (action == GLFW_PRESS);
}

void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xoffset;
	io.MouseWheel += (float)yoffset;
}

void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	// Map GLFW key to ImGuiKey
	ImGuiKey imgui_key = ImGuiKey_None;
	if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_Z)
	{
		// rough mapping for printable keys
		imgui_key = (ImGuiKey)(ImGuiKey_Space + (key - GLFW_KEY_SPACE));
	}
	else
	{
		switch (key)
		{
			case GLFW_KEY_TAB: imgui_key = ImGuiKey_Tab; break;
			case GLFW_KEY_LEFT: imgui_key = ImGuiKey_LeftArrow; break;
			case GLFW_KEY_RIGHT: imgui_key = ImGuiKey_RightArrow; break;
			case GLFW_KEY_UP: imgui_key = ImGuiKey_UpArrow; break;
			case GLFW_KEY_DOWN: imgui_key = ImGuiKey_DownArrow; break;
			case GLFW_KEY_PAGE_UP: imgui_key = ImGuiKey_PageUp; break;
			case GLFW_KEY_PAGE_DOWN: imgui_key = ImGuiKey_PageDown; break;
			case GLFW_KEY_HOME: imgui_key = ImGuiKey_Home; break;
			case GLFW_KEY_END: imgui_key = ImGuiKey_End; break;
			case GLFW_KEY_INSERT: imgui_key = ImGuiKey_Insert; break;
			case GLFW_KEY_DELETE: imgui_key = ImGuiKey_Delete; break;
			case GLFW_KEY_BACKSPACE: imgui_key = ImGuiKey_Backspace; break;
			case GLFW_KEY_ENTER: imgui_key = ImGuiKey_Enter; break;
			case GLFW_KEY_ESCAPE: imgui_key = ImGuiKey_Escape; break;
			case GLFW_KEY_SPACE: imgui_key = ImGuiKey_Space; break;
			default: imgui_key = ImGuiKey_None; break;
		}
	}

	if (imgui_key != ImGuiKey_None)
	{
		io.AddKeyEvent(imgui_key, action != GLFW_RELEASE);
	}

	// update modifiers as well (submit left/right modifier key states)
	io.AddKeyEvent(ImGuiKey_LeftCtrl, glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightCtrl, glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftShift, glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightShift, glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftAlt, glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightAlt, glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_LeftSuper, glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS);
	io.AddKeyEvent(ImGuiKey_RightSuper, glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS);
}

void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c)
{
	ImGuiIO& io = ImGui::GetIO();
	if (c > 0 && c <= 0xFFFF)
		io.AddInputCharacter((unsigned short)c);
}
