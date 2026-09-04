#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "rendering/Window.h"
#include <glad/glad.h>
#include <iostream>

namespace MyEngine {

    Window::Window(const std::string& title, int width, int height)
        : m_Title(title), m_Width(width), m_Height(height)
    {
    }

    Window::~Window()
    {
        Shutdown();
    }

    bool Window::Init()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
            SDL_GL_CONTEXT_PROFILE_CORE);

        m_Window = SDL_CreateWindow(
            m_Title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            m_Width, m_Height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
        );

        if (!m_Window)
        {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
            return false;
        }

        m_Context = SDL_GL_CreateContext(m_Window);
        if (!m_Context)
        {
            std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
            return false;
        }

        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD!" << std::endl;
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, m_Width, m_Height);

        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

        return true;
    }

    void Window::PollEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                m_ShouldClose = true;
        }
    }

    void Window::SwapBuffers()
    {
        SDL_GL_SwapWindow(m_Window);
    }

    void Window::Shutdown()
    {
        if (m_Context)
        {
            SDL_GL_DeleteContext(m_Context);
            m_Context = nullptr;
        }
        if (m_Window)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }
        SDL_Quit();
    }

    bool Window::ShouldClose() const
    {
        return m_ShouldClose;
    }

} // namespace MyEngine