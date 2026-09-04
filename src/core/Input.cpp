#include "core/Input.h"

namespace MyEngine
{
    GLFWwindow* Input::s_window = nullptr;

    std::unordered_map<int, bool> Input::s_currentKeys;
    std::unordered_map<int, bool> Input::s_previousKeys;

    std::unordered_map<int, bool> Input::s_currentMouseButtons;
    std::unordered_map<int, bool> Input::s_previousMouseButtons;

    bool Input::s_isRecording = false;
    bool Input::s_isPlayback = false;
    unsigned int Input::s_replaySeed = 0;
    size_t Input::s_replayPlaybackIndex = 0;
    std::vector<Input::ReplayFrame> Input::s_replayFrames;
    std::vector<unsigned char> Input::s_playbackGamepadButtons;
    std::vector<float> Input::s_playbackGamepadAxes;

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

        if (s_isPlayback && s_replayPlaybackIndex < s_replayFrames.size())
        {
            const ReplayFrame& frame = s_replayFrames[s_replayPlaybackIndex++];
            s_currentKeys = frame.keys;
            s_currentMouseButtons = frame.mouseButtons;
            s_mouseDeltaX = frame.mouseDeltaX;
            s_mouseDeltaY = frame.mouseDeltaY;
            s_mouseWheelX = frame.mouseWheelX;
            s_mouseWheelY = frame.mouseWheelY;
            return;
        }

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

    void Input::BeginInputRecording(unsigned int seed)
    {
        s_isRecording = true;
        s_isPlayback = false;
        s_replaySeed = seed;
        s_replayPlaybackIndex = 0;
        s_replayFrames.clear();
        s_playbackGamepadButtons.assign(15, 0);
        s_playbackGamepadAxes.assign(6, 0.0f);
    }

    void Input::StopInputRecording()
    {
        s_isRecording = false;
    }

    bool Input::IsInputRecording()
    {
        return s_isRecording;
    }

    void Input::BeginInputPlayback()
    {
        s_isPlayback = true;
        s_isRecording = false;
        s_replayPlaybackIndex = 0;
        s_playbackGamepadButtons.assign(15, 0);
        s_playbackGamepadAxes.assign(6, 0.0f);
    }

    void Input::StopInputPlayback()
    {
        s_isPlayback = false;
        s_replayPlaybackIndex = 0;
        s_playbackGamepadButtons.assign(15, 0);
        s_playbackGamepadAxes.assign(6, 0.0f);
    }

    bool Input::IsInputPlayback()
    {
        return s_isPlayback;
    }

    unsigned int Input::GetReplaySeed()
    {
        return s_replaySeed;
    }

    size_t Input::GetReplayFrameCount()
    {
        return s_replayFrames.size();
    }

    size_t Input::GetReplayPlaybackIndex()
    {
        return s_replayPlaybackIndex;
    }

    const std::vector<Input::ReplayFrame>& Input::GetReplayFrames()
    {
        return s_replayFrames;
    }

    void Input::FinalizeReplayFrame(const unsigned char* gamepadButtons, int gamepadButtonCount, const float* gamepadAxes, int gamepadAxisCount, float fixedTimestep, int maxSubsteps, unsigned int particleSeed)
    {
        if (!s_isRecording)
            return;

        ReplayFrame frame;
        frame.keys = s_currentKeys;
        frame.mouseButtons = s_currentMouseButtons;
        frame.mouseDeltaX = s_mouseDeltaX;
        frame.mouseDeltaY = s_mouseDeltaY;
        frame.mouseWheelX = s_mouseWheelX;
        frame.mouseWheelY = s_mouseWheelY;
        frame.fixedTimestep = fixedTimestep;
        frame.maxSubsteps = maxSubsteps;
        frame.particleSeed = particleSeed;

        int buttonCount = std::max(gamepadButtonCount, 0);
        int axisCount = std::max(gamepadAxisCount, 0);
        frame.gamepadButtons.assign(static_cast<size_t>(buttonCount), 0);
        frame.gamepadAxes.assign(static_cast<size_t>(axisCount), 0.0f);

        if (gamepadButtons)
        {
            for (int i = 0; i < buttonCount; ++i)
                frame.gamepadButtons[static_cast<size_t>(i)] = gamepadButtons[i];
        }
        if (gamepadAxes)
        {
            for (int i = 0; i < axisCount; ++i)
                frame.gamepadAxes[static_cast<size_t>(i)] = gamepadAxes[i];
        }

        s_replayFrames.push_back(std::move(frame));
    }

    bool Input::TryGetPlaybackGamepadState(unsigned char* outButtons, int buttonCount, float* outAxes, int axisCount)
    {
        if (!s_isPlayback)
            return false;

        size_t frameIndex = 0;
        if (!s_replayFrames.empty())
        {
            if (s_replayPlaybackIndex == 0)
                frameIndex = 0;
            else
                frameIndex = std::min(s_replayPlaybackIndex - 1, s_replayFrames.size() - 1);

            const ReplayFrame& frame = s_replayFrames[frameIndex];
            s_playbackGamepadButtons = frame.gamepadButtons;
            s_playbackGamepadAxes = frame.gamepadAxes;
        }

        if (outButtons)
        {
            for (int i = 0; i < buttonCount; ++i)
            {
                size_t idx = static_cast<size_t>(i);
                outButtons[i] = (idx < s_playbackGamepadButtons.size()) ? s_playbackGamepadButtons[idx] : 0;
            }
        }

        if (outAxes)
        {
            for (int i = 0; i < axisCount; ++i)
            {
                size_t idx = static_cast<size_t>(i);
                outAxes[i] = (idx < s_playbackGamepadAxes.size()) ? s_playbackGamepadAxes[idx] : 0.0f;
            }
        }

        return true;
    }

    bool Input::TryGetPlaybackSimulationConfig(float* outFixedTimestep, int* outMaxSubsteps, unsigned int* outParticleSeed)
    {
        if (!s_isPlayback || s_replayFrames.empty())
            return false;

        size_t frameIndex = 0;
        if (s_replayPlaybackIndex == 0)
            frameIndex = 0;
        else
            frameIndex = std::min(s_replayPlaybackIndex - 1, s_replayFrames.size() - 1);

        const ReplayFrame& frame = s_replayFrames[frameIndex];
        if (outFixedTimestep)
            *outFixedTimestep = frame.fixedTimestep;
        if (outMaxSubsteps)
            *outMaxSubsteps = frame.maxSubsteps;
        if (outParticleSeed)
            *outParticleSeed = frame.particleSeed;
        return true;
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
