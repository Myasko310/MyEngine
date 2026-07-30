#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

namespace MyEngine {

    class Input {
    public:
        static void Init();
        static void Update();
        static void ProcessEvent(const SDL_Event& event);

        // Keyboard
        static bool IsKeyDown(SDL_Keycode key);
        static bool IsKeyPressed(SDL_Keycode key);  // true only first frame

        // Mouse delta (relative movement this frame)
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();

    private:
        static float s_mouseDeltaX;
        static float s_mouseDeltaY;

        static float s_rawDeltaX;   // accumulated from events
        static float s_rawDeltaY;
    };

} // namespace MyEngine