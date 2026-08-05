#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <string>

namespace MyEngine {

    class Window {
    public:
        Window(const std::string& title, int width, int height);
        ~Window();

        bool Init();
        void PollEvents();
        void SwapBuffers();
        void Shutdown();
        bool ShouldClose() const;

        int GetWidth()  const { return m_Width; }
        int GetHeight() const { return m_Height; }

    private:
        std::string    m_Title;
        int            m_Width;
        int            m_Height;
        SDL_Window* m_Window = nullptr;
        SDL_GLContext  m_Context = nullptr;
        bool           m_ShouldClose = false;
    };

} // namespace MyEngine