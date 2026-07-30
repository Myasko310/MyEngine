#include "core/Input.h"
#include <unordered_map>

namespace MyEngine {

    // ── Statics ──────────────────────────────────────────────
    float Input::s_mouseDeltaX = 0.0f;
    float Input::s_mouseDeltaY = 0.0f;
    float Input::s_rawDeltaX = 0.0f;
    float Input::s_rawDeltaY = 0.0f;

    static std::unordered_map<SDL_Keycode, bool> s_currentKeys;
    static std::unordered_map<SDL_Keycode, bool> s_previousKeys;

    void Input::Init()
    {
        // Lock mouse to window and hide cursor
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    void Input::Update()
    {
        // Flush accumulated mouse delta into readable values
        s_mouseDeltaX = s_rawDeltaX;
        s_mouseDeltaY = s_rawDeltaY;

        // Reset raw accumulators for next frame
        s_rawDeltaX = 0.0f;
        s_rawDeltaY = 0.0f;

        // Snapshot keyboard state for IsKeyPressed
        s_previousKeys = s_currentKeys;
    }

    void Input::ProcessEvent(const SDL_Event& event)
    {
        if (event.type == SDL_MOUSEMOTION)
        {
            // Accumulate relative mouse movement
            s_rawDeltaX += (float)event.motion.xrel;
            s_rawDeltaY += (float)event.motion.yrel;
        }

        if (event.type == SDL_KEYDOWN)
            s_currentKeys[event.key.keysym.sym] = true;

        if (event.type == SDL_KEYUP)
            s_currentKeys[event.key.keysym.sym] = false;
    }

    bool Input::IsKeyDown(SDL_Keycode key)
    {
        auto it = s_currentKeys.find(key);
        return it != s_currentKeys.end() && it->second;
    }

    bool Input::IsKeyPressed(SDL_Keycode key)
    {
        bool current = s_currentKeys.count(key) && s_currentKeys[key];
        bool previous = s_previousKeys.count(key) && s_previousKeys[key];
        return current && !previous;
    }

    float Input::GetMouseDeltaX() { return s_mouseDeltaX; }
    float Input::GetMouseDeltaY() { return s_mouseDeltaY; }

} // namespace MyEngine