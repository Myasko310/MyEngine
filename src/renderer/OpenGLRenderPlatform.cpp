#include "renderer/OpenGLRenderPlatform.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace MyEngine
{
	OpenGLRenderPlatform::~OpenGLRenderPlatform()
	{
		Shutdown();
	}

	bool OpenGLRenderPlatform::Initialize(int width, int height, const char* title)
	{
		if (m_Initialized)
			return true;

		if (!glfwInit())
		{
			std::cerr << "Failed to initialize GLFW." << std::endl;
			return false;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef _DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

		m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (!m_Window)
		{
			std::cerr << "Failed to create GLFW window." << std::endl;
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(m_Window);
		glfwSwapInterval(1);

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
		{
			std::cerr << "Failed to initialize GLAD." << std::endl;
			glfwDestroyWindow(m_Window);
			m_Window = nullptr;
			glfwTerminate();
			return false;
		}

		m_Initialized = true;
		return true;
	}

	void OpenGLRenderPlatform::Shutdown()
	{
		if (!m_Initialized)
			return;

		if (m_Window)
		{
			glfwDestroyWindow(m_Window);
			m_Window = nullptr;
		}

		glfwTerminate();
		m_Initialized = false;
	}

	void OpenGLRenderPlatform::PollEvents()
	{
		glfwPollEvents();
	}

	void OpenGLRenderPlatform::Present()
	{
		if (m_Window)
			glfwSwapBuffers(m_Window);
	}

	bool OpenGLRenderPlatform::ShouldClose() const
	{
		return m_Window ? glfwWindowShouldClose(m_Window) != 0 : true;
	}

	GLFWwindow* OpenGLRenderPlatform::GetWindow() const
	{
		return m_Window;
	}
}
