#pragma once

#include <unordered_map>

#include <GLFW/glfw3.h>

namespace MyEngine
{
    class Input
    {
    public:
        static void Init(GLFWwindow* window, bool captureMouse = true);

        // Call once per frame BEFORE glfwPollEvents()
        static void Update();

        static void Shutdown();

        // Keyboard
        static bool IsKeyDown(int key);
        static bool IsKeyPressed(int key);
        static bool IsKeyReleased(int key);

        // Mouse buttons
        static bool IsMouseButtonDown(int button);
        static bool IsMouseButtonPressed(int button);
        static bool IsMouseButtonReleased(int button);

        // Mouse movement
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();

        // Mouse wheel
        static float GetMouseWheelX();
        static float GetMouseWheelY();

        // Mouse capture
        static void SetMouseCaptured(bool captured);
        static bool IsMouseCaptured();

        static GLFWwindow* GetWindow();

    private:
        static void KeyCallback(
            GLFWwindow* window,
            int key,
            int scancode,
            int action,
            int mods
        );

        static void MouseButtonCallback(
            GLFWwindow* window,
            int button,
            int action,
            int mods
        );

        static void CursorPositionCallback(
            GLFWwindow* window,
            double xpos,
            double ypos
        );

        static void ScrollCallback(
            GLFWwindow* window,
            double xoffset,
            double yoffset
        );

    private:
        static GLFWwindow* s_window;

        static std::unordered_map<int, bool> s_currentKeys;
        static std::unordered_map<int, bool> s_previousKeys;

        static std::unordered_map<int, bool> s_currentMouseButtons;
        static std::unordered_map<int, bool> s_previousMouseButtons;

        static float s_mouseDeltaX;
        static float s_mouseDeltaY;

        static float s_mouseWheelX;
        static float s_mouseWheelY;

        static double s_lastMouseX;
        static double s_lastMouseY;

        static bool s_firstMouse;
        static bool s_mouseCaptured;
    };
}