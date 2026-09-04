#pragma once

struct GLFWwindow;

namespace MyEngine
{
	class IRenderPlatform
	{
	public:
		virtual ~IRenderPlatform() = default;

		virtual bool Initialize(int width, int height, const char* title) = 0;
		virtual void Shutdown() = 0;
		virtual void PollEvents() = 0;
		virtual void Present() = 0;
		virtual bool ShouldClose() const = 0;
		virtual GLFWwindow* GetWindow() const = 0;
	};
}
