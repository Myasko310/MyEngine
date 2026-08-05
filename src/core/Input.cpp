#include "core/Input.h"

namespace MyEngine
{
    GLFWwindow* Input::s_window = nullptr;

    std::unordered_map<int, bool> Input::s_currentKeys;
    std::unordered_map<int, bool> Input::s_previousKeys;

    std::unordered_map<int, bool> Input::s_currentMouseButtons;
    std::unordered_map<int, bool> Input::s_previousMouseButtons;

    float Input::s_mouseDeltaX = 0.0f;
    float Input::s_mouseDeltaY = 0.0f;

    float Input::s_mouseWheelX = 0.0f;
    float Input::s_mouseWheelY = 0.0f;

    double Input::s_lastMouseX = 0.0;
    double Input::s_lastMouseY = 0.0;

    bool Input::s_firstMouse = true;
    bool Input::s_mouseCaptured = false;

    void Input::Init(GLFWwindow* window, bool captureMouse)
    {
        s_window = window;

        s_currentKeys.clear();
        s_previousKeys.clear();

        s_currentMouseButtons.clear();
        s_previousMouseButtons.clear();

        s_mouseDeltaX = 0.0f;
        s_mouseDeltaY = 0.0f;

        s_mouseWheelX = 0.0f;
        s_mouseWheelY = 0.0f;

        s_firstMouse = true;

        glfwSetKeyCallback(s_window, KeyCallback);
        glfwSetMouseButtonCallback(s_window, MouseButtonCallback);
        glfwSetCursorPosCallback(s_window, CursorPositionCallback);
        glfwSetScrollCallback(s_window, ScrollCallback);

        SetMouseCaptured(captureMouse);
    }

    void Input::Update()
    {
        s_previousKeys = s_currentKeys;
        s_previousMouseButtons = s_currentMouseButtons;

        s_mouseDeltaX = 0.0f;
        s_mouseDeltaY = 0.0f;

        s_mouseWheelX = 0.0f;
        s_mouseWheelY = 0.0f;
    }

    void Input::Shutdown()
    {
        s_window = nullptr;

        s_currentKeys.clear();
        s_previousKeys.clear();

        s_currentMouseButtons.clear();
        s_previousMouseButtons.clear();
    }

    bool Input::IsKeyDown(int key)
    {
        auto it = s_currentKeys.find(key);

        if (it == s_currentKeys.end())
            return false;

        return it->second;
    }

    bool Input::IsKeyPressed(int key)
    {
        bool current = IsKeyDown(key);

        bool previous = false;
        auto it = s_previousKeys.find(key);

        if (it != s_previousKeys.end())
            previous = it->second;

        return current && !previous;
    }

    bool Input::IsKeyReleased(int key)
    {
        bool current = IsKeyDown(key);

        bool previous = false;
        auto it = s_previousKeys.find(key);

        if (it != s_previousKeys.end())
            previous = it->second;

        return !current && previous;
    }

    bool Input::IsMouseButtonDown(int button)
    {
        auto it = s_currentMouseButtons.find(button);

        if (it == s_currentMouseButtons.end())
            return false;

        return it->second;
    }

    bool Input::IsMouseButtonPressed(int button)
    {
        bool current = IsMouseButtonDown(button);

        bool previous = false;
        auto it = s_previousMouseButtons.find(button);

        if (it != s_previousMouseButtons.end())
            previous = it->second;

        return current && !previous;
    }

    bool Input::IsMouseButtonReleased(int button)
    {
        bool current = IsMouseButtonDown(button);

        bool previous = false;
        auto it = s_previousMouseButtons.find(button);

        if (it != s_previousMouseButtons.end())
            previous = it->second;

        return !current && previous;
    }

    float Input::GetMouseDeltaX()
    {
        return s_mouseDeltaX;
    }

    float Input::GetMouseDeltaY()
    {
        return s_mouseDeltaY;
    }

    float Input::GetMouseWheelX()
    {
        return s_mouseWheelX;
    }

    float Input::GetMouseWheelY()
    {
        return s_mouseWheelY;
    }

    void Input::SetMouseCaptured(bool captured)
    {
        if (!s_window)
            return;

        s_mouseCaptured = captured;
        s_firstMouse = true;

        glfwSetInputMode(
            s_window,
            GLFW_CURSOR,
            captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );
    }

    bool Input::IsMouseCaptured()
    {
        return s_mouseCaptured;
    }

    GLFWwindow* Input::GetWindow()
    {
        return s_window;
    }

    void Input::KeyCallback(
        GLFWwindow* window,
        int key,
        int scancode,
        int action,
        int mods
    )
    {
        if (key == GLFW_KEY_UNKNOWN)
            return;

        if (action == GLFW_PRESS)
        {
            s_currentKeys[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            s_currentKeys[key] = false;
        }
    }

    void Input::MouseButtonCallback(
        GLFWwindow* window,
        int button,
        int action,
        int mods
    )
    {
        if (action == GLFW_PRESS)
        {
            s_currentMouseButtons[button] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            s_currentMouseButtons[button] = false;
        }
    }

    void Input::CursorPositionCallback(
        GLFWwindow* window,
        double xpos,
        double ypos
    )
    {
        if (s_firstMouse)
        {
            s_lastMouseX = xpos;
            s_lastMouseY = ypos;
            s_firstMouse = false;
            return;
        }

        s_mouseDeltaX += static_cast<float>(xpos - s_lastMouseX);
        s_mouseDeltaY += static_cast<float>(s_lastMouseY - ypos);

        s_lastMouseX = xpos;
        s_lastMouseY = ypos;
    }

    void Input::ScrollCallback(
        GLFWwindow* window,
        double xoffset,
        double yoffset
    )
    {
        s_mouseWheelX += static_cast<float>(xoffset);
        s_mouseWheelY += static_cast<float>(yoffset);
    }
}