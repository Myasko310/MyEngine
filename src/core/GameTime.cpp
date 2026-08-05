#include "core/GameTime.h"
#include <SDL2/SDL.h>

namespace MyEngine {

    float     GameTime::s_DeltaTime = 0.0f;
    float     GameTime::s_TotalTime = 0.0f;
    long long GameTime::s_LastTime = 0;

    void GameTime::Init()
    {
        s_LastTime = SDL_GetTicks64();
        s_DeltaTime = 0.0f;
        s_TotalTime = 0.0f;
    }

    void GameTime::Update()
    {
        long long now = SDL_GetTicks64();
        s_DeltaTime = (now - s_LastTime) / 1000.0f;
        s_TotalTime += s_DeltaTime;
        s_LastTime = now;
    }

    float GameTime::DeltaTime() { return s_DeltaTime; }
    float GameTime::TotalTime() { return s_TotalTime; }

} // namespace MyEngine