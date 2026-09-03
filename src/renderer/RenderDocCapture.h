#pragma once

namespace MyEngine
{
	class RenderDocCapture
	{
	public:
		RenderDocCapture() = default;
		~RenderDocCapture();

		RenderDocCapture(const RenderDocCapture&) = delete;
		RenderDocCapture& operator=(const RenderDocCapture&) = delete;

		bool Initialize();
		bool IsAvailable() const;
		void RequestCapture();
		void BeginFrameCapture();
		void EndFrameCapture();

	private:
		bool m_Requested = false;
		bool m_ActiveCapture = false;

#ifdef _WIN32
		void* m_RenderDocModule = nullptr;
		void* m_RenderDocApi = nullptr;
#endif
	};
}
