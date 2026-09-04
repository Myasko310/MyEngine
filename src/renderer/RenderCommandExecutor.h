#pragma once

#include "renderer/RenderCommandList.h"

namespace MyEngine
{
	class IRenderCommandExecutor
	{
	public:
		virtual ~IRenderCommandExecutor() = default;
		virtual void Execute(const RenderCommandList& commandList) = 0;
	};
}
