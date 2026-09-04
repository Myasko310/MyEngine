#pragma once

#include "renderer/RenderBackend.h"

#include <memory>

namespace MyEngine
{
	class IRenderPlatform;

	std::unique_ptr<IRenderPlatform> CreateRenderPlatform(RenderBackendType backend);
}
