#include "renderer/RenderDocCapture.h"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <cstdint>
#endif

namespace MyEngine
{
#ifdef _WIN32
	namespace
	{
		constexpr int kRenderDocApiV142 = 10402;

		struct RenderDocApi142
		{
			void (*StartFrameCapture)(void* device, void* wndHandle);
			std::uint32_t (*IsFrameCapturing)();
			std::uint32_t (*EndFrameCapture)(void* device, void* wndHandle);
		};

		using RenderDocGetApiFn = int(*)(int version, void** outApi);
	}
#endif

	RenderDocCapture::~RenderDocCapture()
	{
#ifdef _WIN32
		if (m_RenderDocModule)
		{
			FreeLibrary(static_cast<HMODULE>(m_RenderDocModule));
			m_RenderDocModule = nullptr;
			m_RenderDocApi = nullptr;
		}
#endif
	}

	bool RenderDocCapture::Initialize()
	{
#ifdef _WIN32
		if (m_RenderDocApi)
			return true;

		HMODULE module = LoadLibraryA("renderdoc.dll");
		if (!module)
			return false;

		auto getApi = reinterpret_cast<RenderDocGetApiFn>(GetProcAddress(module, "RENDERDOC_GetAPI"));
		if (!getApi)
		{
			FreeLibrary(module);
			return false;
		}

		RenderDocApi142* api = nullptr;
		if (getApi(kRenderDocApiV142, reinterpret_cast<void**>(&api)) == 0 || !api)
		{
			FreeLibrary(module);
			return false;
		}

		m_RenderDocModule = module;
		m_RenderDocApi = api;
		std::cout << "[RenderDoc] API loaded. Press F9 to capture next frame." << std::endl;
		return true;
#else
		return false;
#endif
	}

	bool RenderDocCapture::IsAvailable() const
	{
#ifdef _WIN32
		return m_RenderDocApi != nullptr;
#else
		return false;
#endif
	}

	void RenderDocCapture::RequestCapture()
	{
		if (!IsAvailable())
			return;

		m_Requested = true;
	}

	void RenderDocCapture::BeginFrameCapture()
	{
#ifdef _WIN32
		if (!m_RenderDocApi || !m_Requested || m_ActiveCapture)
			return;

		auto* api = static_cast<RenderDocApi142*>(m_RenderDocApi);
		api->StartFrameCapture(nullptr, nullptr);
		m_ActiveCapture = true;
		m_Requested = false;
#endif
	}

	void RenderDocCapture::EndFrameCapture()
	{
#ifdef _WIN32
		if (!m_RenderDocApi || !m_ActiveCapture)
			return;

		auto* api = static_cast<RenderDocApi142*>(m_RenderDocApi);
		api->EndFrameCapture(nullptr, nullptr);
		m_ActiveCapture = false;
#endif
	}
}
