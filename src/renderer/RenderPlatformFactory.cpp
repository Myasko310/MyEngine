#include "renderer/RenderPlatformFactory.h"

#ifdef MYENGINE_ENABLE_DX12_BACKEND
#include "renderer/DirectX12RenderPlatform.h"
#endif
#include "renderer/OpenGLRenderPlatform.h"

#include <iostream>

namespace MyEngine
{
	std::unique_ptr<IRenderPlatform> CreateRenderPlatform(RenderBackendType backend)
	{
		if (backend == RenderBackendType::DirectX12)
		{
#ifdef MYENGINE_ENABLE_DX12_BACKEND
			return std::make_unique<DirectX12RenderPlatform>();
#else
			std::cerr << "[Renderer] DirectX12 backend requested but not compiled in (MYENGINE_ENABLE_DX12_BACKEND=OFF). Falling back to OpenGL." << std::endl;
#endif
		}

		return std::make_unique<OpenGLRenderPlatform>();
	}
}
