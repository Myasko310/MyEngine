#pragma once

#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

namespace MyEngine
{
    class Input
    {
    public:
        struct ReplayFrame
        {
            std::unordered_map<int, bool> keys;
            std::unordered_map<int, bool> mouseButtons;
            std::vector<unsigned char> gamepadButtons;
            std::vector<float> gamepadAxes;
            float mouseDeltaX = 0.0f;
            float mouseDeltaY = 0.0f;
            float mouseWheelX = 0.0f;
            float mouseWheelY = 0.0f;
            float fixedTimestep = 0.02f;
            int maxSubsteps = 5;
            unsigned int particleSeed = 0u;
        };
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

        // Deterministic replay (raw input capture/playback)
        static void BeginInputRecording(unsigned int seed);
        static void StopInputRecording();
        static bool IsInputRecording();
        static void BeginInputPlayback();
        static void StopInputPlayback();
        static bool IsInputPlayback();
        static unsigned int GetReplaySeed();
        static size_t GetReplayFrameCount();
        static size_t GetReplayPlaybackIndex();
        static const std::vector<ReplayFrame>& GetReplayFrames();
        static void FinalizeReplayFrame(const unsigned char* gamepadButtons, int gamepadButtonCount, const float* gamepadAxes, int gamepadAxisCount, float fixedTimestep, int maxSubsteps, unsigned int particleSeed);
        static bool TryGetPlaybackGamepadState(unsigned char* outButtons, int buttonCount, float* outAxes, int axisCount);
        static bool TryGetPlaybackSimulationConfig(float* outFixedTimestep, int* outMaxSubsteps, unsigned int* outParticleSeed);

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

        static bool s_isRecording;
        static bool s_isPlayback;
        static unsigned int s_replaySeed;
        static size_t s_replayPlaybackIndex;
        static std::vector<ReplayFrame> s_replayFrames;
        static std::vector<unsigned char> s_playbackGamepadButtons;
        static std::vector<float> s_playbackGamepadAxes;

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