#pragma once

#include "renderer/RenderPlatform.h"

struct GLFWwindow;

namespace MyEngine
{
	class OpenGLRenderPlatform final : public IRenderPlatform
	{
	public:
		OpenGLRenderPlatform() = default;
		~OpenGLRenderPlatform() override;

		bool Initialize(int width, int height, const char* title) override;
		void Shutdown() override;
		void PollEvents() override;
		void Present() override;
		bool ShouldClose() const override;
		GLFWwindow* GetWindow() const override;

	private:
		GLFWwindow* m_Window = nullptr;
		bool m_Initialized = false;
	};
}
